#include <algorithm>
#include <filesystem>

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include "AssimpModel.h"
#include "Tools.h"
#include "Logger.h"

bool AssimpModel::loadModel(VkRenderData &renderData, std::string modelFilename, unsigned int extraImportFlags) {
  Logger::log(1, "%s: loading model from file '%s'\n", __FUNCTION__, modelFilename.c_str());

  Assimp::Importer importer;
  /* we need to flip texture coordinates for Vulkan */
  const aiScene *scene = importer.ReadFile(modelFilename,
    aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_ValidateDataStructure | aiProcess_FlipUVs | extraImportFlags);

  if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
    Logger::log(1, "%s error: assimp error '%s' while loading file '%s'\n", __FUNCTION__, importer.GetErrorString(), modelFilename.c_str());
    return false;
  }

  unsigned int numMeshes = scene->mNumMeshes;
  Logger::log(1, "%s: found %i mesh%s\n", __FUNCTION__, numMeshes, numMeshes == 1 ? "" : "es");

  for (unsigned int i = 0; i < numMeshes; ++i) {
    unsigned int numVertices = scene->mMeshes[i]->mNumVertices;
    unsigned int numFaces = scene->mMeshes[i]->mNumFaces;

    mVertexCount += numVertices;
    mTriangleCount += numFaces;

    Logger::log(1, "%s: mesh %i contains %i vertices and %i faces\n", __FUNCTION__, i, numVertices, numFaces);
  }
  Logger::log(1, "%s: model contains %i vertices and %i faces\n", __FUNCTION__, mVertexCount, mTriangleCount);

  aiNode* rootNode = scene->mRootNode;

  if (scene->HasTextures()) {
    unsigned int numTextures = scene->mNumTextures;

    for (int i = 0; i < scene->mNumTextures; ++i) {
      std::string texName = scene->mTextures[i]->mFilename.C_Str();

      int height = scene->mTextures[i]->mHeight;
      int width = scene->mTextures[i]->mWidth;
      aiTexel* data = scene->mTextures[i]->pcData;

      VkTextureData newTex{};
      if (!Texture::loadTexture(renderData, newTex, texName, data, width, height)) {
        return false;
      }

      std::string internalTexName = "*" + std::to_string(i);
      Logger::log(1, "%s: - added internal texture '%s'\n", __FUNCTION__, internalTexName.c_str());
      mTextures.insert({internalTexName, newTex});
    }

    Logger::log(1, "%s: scene has %i embedded textures\n", __FUNCTION__, numTextures);
  }

  /* add a white texture in case there is no diffuse tex but colors */
  std::string whiteTexName = "textures/white.png";
  if (!Texture::loadTexture(renderData, mWhiteTexture, whiteTexName)) {
    Logger::log(1, "%s error: could not load white default texture '%s'\n", __FUNCTION__, whiteTexName.c_str());
    return false;
  }

  /* add a placeholder texture in case there is no diffuse tex */
  std::string placeholderTexName = "textures/missing_tex.png";
  if (!Texture::loadTexture(renderData, mPlaceholderTexture, placeholderTexName)) {
    Logger::log(1, "%s error: could not load placeholder texture '%s'\n", __FUNCTION__, placeholderTexName.c_str());
    return false;
  }

  /* the textures are stored directly or relative to the model file */
  std::string assetDirectory = modelFilename.substr(0, modelFilename.find_last_of('/'));

  /* nodes */
  Logger::log(1, "%s: ... processing nodes...\n", __FUNCTION__);

  std::string rootNodeName = rootNode->mName.C_Str();
  mRootNode = AssimpNode::createNode(rootNodeName);
  Logger::log(2, "%s: root node name: '%s'\n", __FUNCTION__, rootNodeName.c_str());

  processNode(renderData, mRootNode, rootNode, scene, assetDirectory);

  Logger::log(1, "%s: ... processing nodes finished...\n", __FUNCTION__);

  for (const auto& entry : mNodeList) {
    std::vector<std::shared_ptr<AssimpNode>> childNodes = entry->getChilds();

    std::string parentName = entry->getParentNodeName();
    Logger::log(1, "%s: --- found node %s in node list, it has %i children, parent is %s\n", __FUNCTION__, entry->getNodeName().c_str(), childNodes.size(), parentName.c_str());

    for (const auto& node : childNodes) {
      Logger::log(1, "%s: ---- child: %s\n", __FUNCTION__, node->getNodeName().c_str());
    }
  }

  std::vector<glm::mat4> boneOffsetMatricesList{};
  std::vector<int32_t> boneParentIndexList{};

  for (const auto& bone : mBoneList) {
    boneOffsetMatricesList.emplace_back(bone->getOffsetMatrix());

    std::string parentNodeName = mNodeMap.at(bone->getBoneName())->getParentNodeName();
    const auto boneIter = std::find_if(mBoneList.begin(), mBoneList.end(), [parentNodeName](std::shared_ptr<AssimpBone>& bone) { return bone->getBoneName() == parentNodeName; });
    if (boneIter == mBoneList.end()) {
        boneParentIndexList.emplace_back(-1); // root node gets a -1 to identify
    } else {
        boneParentIndexList.emplace_back(std::distance(mBoneList.begin(), boneIter));
    }
  }

  Logger::log(1, "%s: -- bone parents --\n", __FUNCTION__);
  for (unsigned int i = 0; i < mBoneList.size(); ++i) {
    Logger::log(1, "%s: bone %i (%s) has parent %i (%s)\n", __FUNCTION__, i, mBoneList.at(i)->getBoneName().c_str(), boneParentIndexList.at(i),
      boneParentIndexList.at(i) < 0 ? "invalid" : mBoneList.at(boneParentIndexList.at(i))->getBoneName().c_str());
  }
  Logger::log(1, "%s: -- bone parents --\n", __FUNCTION__);

  /* create vertex buffers for the meshes */
  for (const auto& mesh : mModelMeshes) {
    VkVertexBufferData vertexBuffer;
    VertexBuffer::init(renderData, vertexBuffer, mesh.vertices.size() * sizeof(VkVertex));
    VertexBuffer::uploadData(renderData, vertexBuffer, mesh);
    mVertexBuffers.emplace_back(vertexBuffer);

    VkIndexBufferData indexBuffer;
    IndexBuffer::init(renderData, indexBuffer, mesh.indices.size() * sizeof(uint32_t));
    IndexBuffer::uploadData(renderData, indexBuffer, mesh);
    mIndexBuffers.emplace_back(indexBuffer);
  }

  /* init all SSBOs */
  ShaderStorageBuffer::init(renderData, mAnimLookupBuffer);
  ShaderStorageBuffer::init(renderData, mShaderBoneMatrixOffsetBuffer);
  ShaderStorageBuffer::init(renderData, mShaderBoneParentBuffer);

  /* animations */
  unsigned int numAnims = scene->mNumAnimations;
  for (unsigned int i = 0; i < numAnims; ++i) {
    aiAnimation* animation = scene->mAnimations[i];
    mMaxClipDuration = std::max(mMaxClipDuration, static_cast<float>(animation->mDuration));
  }
  Logger::log(1, "%s: longest clip duration is %f\n", __FUNCTION__, mMaxClipDuration);

  for (unsigned int i = 0; i < numAnims; ++i) {
    aiAnimation* animation = scene->mAnimations[i];

    Logger::log(1, "%s: -- animation clip %i has %i skeletal channels, %i mesh channels, and %i morph mesh channels\n",
      __FUNCTION__, i, animation->mNumChannels, animation->mNumMeshChannels, animation->mNumMorphMeshChannels);

    std::shared_ptr<AssimpAnimClip> animClip = std::make_shared<AssimpAnimClip>();
    animClip->addChannels(animation, mMaxClipDuration, mBoneList);
    if (animClip->getClipName().empty()) {
      animClip->setClipName(std::to_string(i));
    }
    mAnimClips.emplace_back(animClip);
  }

  if (!mAnimClips.empty()) {
    std::vector<glm::vec4> animLookupData{};

    /* store inverse scaling factor in first element of lookup row */
    const int LOOKUP_SIZE = 1023 + 1;

    std::vector<glm::vec4> emptyTranslateVector(LOOKUP_SIZE, glm::vec4(0.0f));
    emptyTranslateVector.at(0) = glm::vec4(0.0f);

    std::vector<glm::vec4> emptyRotateVector(LOOKUP_SIZE, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)); // x, y, z, w
    emptyRotateVector.at(0) = glm::vec4(0.0f);

    std::vector<glm::vec4> emptyScaleVector(LOOKUP_SIZE, glm::vec4(1.0f));
    emptyScaleVector.at(0) = glm::vec4(0.0f);

    /* init all transform values with defaults  */
    for (int i = 0; i < mBoneList.size() * mAnimClips.at(0)->getNumChannels(); ++i) {
      animLookupData.insert(animLookupData.end(), emptyTranslateVector.begin(), emptyTranslateVector.end());
      animLookupData.insert(animLookupData.end(), emptyRotateVector.begin(), emptyRotateVector.end());
      animLookupData.insert(animLookupData.end(), emptyScaleVector.begin(), emptyScaleVector.end());
    }

    for (int clipId = 0; clipId < mAnimClips.size(); ++clipId) {
      Logger::log(1, "%s: generating lookup data for clip %i\n", __FUNCTION__, clipId);
      for (const auto& channel : mAnimClips.at(clipId)->getChannels()) {
        int boneId = channel->getBoneId();
        if (boneId >= 0) {
          int offset = clipId * mBoneList.size() * LOOKUP_SIZE * 3 + boneId * LOOKUP_SIZE * 3;

          animLookupData.at(offset) = glm::vec4(channel->getInvTranslationScaling(), 0.0f, 0.0f, 0.0f);
          const std::vector<glm::vec4>& translations = channel->getTranslationData();
          std::copy(translations.begin(), translations.end(), animLookupData.begin() + offset + 1);

          offset += LOOKUP_SIZE;
          animLookupData.at(offset) = glm::vec4(channel->getInvRotationScaling(), 0.0f, 0.0f, 0.0f);
          const std::vector<glm::vec4>& rotations = channel->getRotationData();
          std::copy(rotations.begin(), rotations.end(), animLookupData.begin() + offset + 1);

          offset += LOOKUP_SIZE;
          animLookupData.at(offset) = glm::vec4(channel->getInvScaleScaling(), 0.0f, 0.0f, 0.0f);
          const std::vector<glm::vec4>& scalings = channel->getScalingData();
          std::copy(scalings.begin(), scalings.end(), animLookupData.begin() + offset + 1);
        }
      }
    }

    Logger::log(1, "%s: generated %i elements of lookup data (%i bytes)\n", __FUNCTION__, animLookupData.size(), animLookupData.size() * sizeof(glm::vec4));
    ShaderStorageBuffer::uploadSsboData(renderData, mAnimLookupBuffer, animLookupData);
  }

  ShaderStorageBuffer::uploadSsboData(renderData, mShaderBoneMatrixOffsetBuffer, boneOffsetMatricesList);
  ShaderStorageBuffer::uploadSsboData(renderData, mShaderBoneParentBuffer, boneParentIndexList);

  /* create descriptor set for per-model data */
  createDescriptorSet(renderData);

  mModelSettings.msModelFilenamePath = modelFilename;
  mModelSettings.msModelFilename = std::filesystem::path(modelFilename).filename().generic_string();

  /* get root transformation matrix from model's root node */
  mRootTransformMatrix = Tools::convertAiToGLM(rootNode->mTransformation);

  if (!mBoneList.empty()) {
    for (const auto& bone: mBoneList) {
      mBoneNameList.emplace_back(bone->getBoneName());
    }
  }

  Logger::log(1, "%s: - model has a total of %i texture%s\n", __FUNCTION__, mTextures.size(), mTextures.size() == 1 ? "" : "s");
  Logger::log(1, "%s: - model has a total of %i bone%s\n", __FUNCTION__, mBoneList.size(), mBoneList.size() == 1 ? "" : "s");
  Logger::log(1, "%s: - model has a total of %i animation%s\n", __FUNCTION__, numAnims, numAnims == 1 ? "" : "s");

  Logger::log(1, "%s: successfully loaded model '%s' (%s)\n", __FUNCTION__, modelFilename.c_str(), mModelSettings.msModelFilename.c_str());
  return true;
}

void AssimpModel::processNode(VkRenderData &renderData, std::shared_ptr<AssimpNode> node, aiNode* aNode, const aiScene* scene, std::string assetDirectory) {
  std::string nodeName = aNode->mName.C_Str();
  Logger::log(1, "%s: node name: '%s'\n", __FUNCTION__, nodeName.c_str());

  unsigned int numMeshes = aNode->mNumMeshes;
  if (numMeshes > 0) {
    Logger::log(1, "%s: - node has %i meshes\n", __FUNCTION__, numMeshes);
    for (unsigned int i = 0; i < numMeshes; ++i) {
      aiMesh* modelMesh = scene->mMeshes[aNode->mMeshes[i]];

      AssimpMesh mesh;
      mesh.processMesh(renderData, modelMesh, scene, assetDirectory, mTextures);

      mModelMeshes.emplace_back(mesh.getMesh());

      /* avoid inserting duplicate bone Ids - meshes can reference the same bones */
      std::vector<std::shared_ptr<AssimpBone>> flatBones = mesh.getBoneList();
      for (const auto& bone : flatBones) {
        const auto iter = std::find_if(mBoneList.begin(), mBoneList.end(), [bone](std::shared_ptr<AssimpBone>& otherBone) { return bone->getBoneId() == otherBone->getBoneId(); });
        if (iter == mBoneList.end()) {
          mBoneList.emplace_back(bone);
        }
      }
    }
  }

  mNodeMap.insert({nodeName, node});
  mNodeList.emplace_back(node);

  unsigned int numChildren = aNode->mNumChildren;
  Logger::log(1, "%s: - node has %i children \n", __FUNCTION__, numChildren);

  for (unsigned int i = 0; i < numChildren; ++i) {
    std::string childName = aNode->mChildren[i]->mName.C_Str();
    Logger::log(1, "%s: --- found child node '%s'\n", __FUNCTION__, childName.c_str());

    std::shared_ptr<AssimpNode> childNode = node->addChild(childName);
    processNode(renderData, childNode, aNode->mChildren[i], scene, assetDirectory);
  }
}

glm::mat4 AssimpModel::getRootTranformationMatrix() {
  return mRootTransformMatrix;
}

bool AssimpModel::createDescriptorSet(VkRenderData& renderData) {
  {
    /* matrix multiplication, per-model data */
    VkDescriptorSetAllocateInfo computeMatrixMultPerModelDescriptorAllocateInfo{};
    computeMatrixMultPerModelDescriptorAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    computeMatrixMultPerModelDescriptorAllocateInfo.descriptorPool = renderData.rdDescriptorPool;
    computeMatrixMultPerModelDescriptorAllocateInfo.descriptorSetCount = 1;
    computeMatrixMultPerModelDescriptorAllocateInfo.pSetLayouts = &renderData.rdAssimpComputeMatrixMultPerModelDescriptorLayout;

    VkResult result = vkAllocateDescriptorSets(renderData.rdVkbDevice.device, &computeMatrixMultPerModelDescriptorAllocateInfo,
      &mMatrixMultPerModelDescriptorSet);
    if (result != VK_SUCCESS) {
      Logger::log(1, "%s error: could not allocate Assimp Matrix Mult Compute per-model descriptor set (error: %i)\n", __FUNCTION__, result);
      return false;
    }

    VkDescriptorBufferInfo parentNodeInfo{};
    parentNodeInfo.buffer = mShaderBoneParentBuffer.buffer;
    parentNodeInfo.offset = 0;
    parentNodeInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo boneOffsetInfo{};
    boneOffsetInfo.buffer = mShaderBoneMatrixOffsetBuffer.buffer;
    boneOffsetInfo.offset = 0;
    boneOffsetInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet parentNodeWriteDescriptorSet{};
    parentNodeWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    parentNodeWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    parentNodeWriteDescriptorSet.dstSet = mMatrixMultPerModelDescriptorSet;
    parentNodeWriteDescriptorSet.dstBinding = 0;
    parentNodeWriteDescriptorSet.descriptorCount = 1;
    parentNodeWriteDescriptorSet.pBufferInfo = &parentNodeInfo;

    VkWriteDescriptorSet boneOffsetWriteDescriptorSet{};
    boneOffsetWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    boneOffsetWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    boneOffsetWriteDescriptorSet.dstSet = mMatrixMultPerModelDescriptorSet;
    boneOffsetWriteDescriptorSet.dstBinding = 1;
    boneOffsetWriteDescriptorSet.descriptorCount = 1;
    boneOffsetWriteDescriptorSet.pBufferInfo = &boneOffsetInfo;

    std::vector<VkWriteDescriptorSet> matrixMultWriteDescriptorSets =
      {  parentNodeWriteDescriptorSet, boneOffsetWriteDescriptorSet };

    vkUpdateDescriptorSets(renderData.rdVkbDevice.device, static_cast<uint32_t>(matrixMultWriteDescriptorSets.size()),
       matrixMultWriteDescriptorSets.data(), 0, nullptr);
  }
  {
    /* transform, per-model */
    VkDescriptorSetAllocateInfo computeTransformPerModelDescriptorAllocateInfo{};
    computeTransformPerModelDescriptorAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    computeTransformPerModelDescriptorAllocateInfo.descriptorPool = renderData.rdDescriptorPool;
    computeTransformPerModelDescriptorAllocateInfo.descriptorSetCount = 1;
    computeTransformPerModelDescriptorAllocateInfo.pSetLayouts = &renderData.rdAssimpComputeTransformPerModelDescriptorLayout;

    VkResult result = vkAllocateDescriptorSets(renderData.rdVkbDevice.device, &computeTransformPerModelDescriptorAllocateInfo,
      &mTransformPerModelDescriptorSet);
    if (result != VK_SUCCESS) {
      Logger::log(1, "%s error: could not allocate Assimp Transform Compute per-model descriptor set (error: %i)\n", __FUNCTION__, result);
      return false;
    }

    VkDescriptorBufferInfo animLookupInfo{};
    animLookupInfo.buffer = mAnimLookupBuffer.buffer;
    animLookupInfo.offset = 0;
    animLookupInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet animLookupWriteDescriptorSet{};
    animLookupWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    animLookupWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    animLookupWriteDescriptorSet.dstSet = mTransformPerModelDescriptorSet;
    animLookupWriteDescriptorSet.dstBinding = 0;
    animLookupWriteDescriptorSet.descriptorCount = 1;
    animLookupWriteDescriptorSet.pBufferInfo = &animLookupInfo;

    vkUpdateDescriptorSets(renderData.rdVkbDevice.device, 1, &animLookupWriteDescriptorSet, 0, nullptr);
  }

  return true;
}

void AssimpModel::draw(VkRenderData &renderData, bool selectionModeActive) {
  for (unsigned int i = 0; i < mModelMeshes.size(); ++i) {
    VkMesh& mesh = mModelMeshes.at(i);

    // find diffuse texture by name
    VkTextureData diffuseTex{};
    auto diffuseTexName = mesh.textures.find(aiTextureType_DIFFUSE);
    if (diffuseTexName != mesh.textures.end()) {
      auto diffuseTexture = mTextures.find(diffuseTexName->second);
      if (diffuseTexture != mTextures.end()) {
        diffuseTex = diffuseTexture->second;
      }
    }

    /* switch between animated and non-animated pipeline layout */
    VkPipelineLayout renderLayout;
    if (hasAnimations()) {
      if (selectionModeActive) {
        renderLayout = renderData.rdAssimpSkinningSelectionPipelineLayout;
      } else {
        renderLayout = renderData.rdAssimpSkinningPipelineLayout;
      }
    } else {
      if (selectionModeActive) {
        renderLayout = renderData.rdAssimpSelectionPipelineLayout;
      } else {
        renderLayout = renderData.rdAssimpPipelineLayout;
      }
    }

    if (diffuseTex.image != VK_NULL_HANDLE) {
      vkCmdBindDescriptorSets(renderData.rdCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        renderLayout, 0, 1, &diffuseTex.descriptorSet, 0, nullptr);
    } else {
      if (mesh.usesPBRColors) {
        vkCmdBindDescriptorSets(renderData.rdCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
          renderLayout, 0, 1, &mWhiteTexture.descriptorSet, 0, nullptr);
      } else {
        vkCmdBindDescriptorSets(renderData.rdCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
          renderLayout, 0, 1, &mPlaceholderTexture.descriptorSet, 0, nullptr);
      }
    }

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(renderData.rdCommandBuffer, 0, 1, &mVertexBuffers.at(i).buffer, &offset);
    vkCmdBindIndexBuffer(renderData.rdCommandBuffer, mIndexBuffers.at(i).buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(renderData.rdCommandBuffer, static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
  }
}

void AssimpModel::drawInstanced(VkRenderData &renderData, uint32_t instanceCount, bool selectionModeActive) {
  for (unsigned int i = 0; i < mModelMeshes.size(); ++i) {
    VkMesh& mesh = mModelMeshes.at(i);

    // find diffuse texture by name
    VkTextureData diffuseTex{};
    auto diffuseTexName = mesh.textures.find(aiTextureType_DIFFUSE);
    if (diffuseTexName != mesh.textures.end()) {
      auto diffuseTexture = mTextures.find(diffuseTexName->second);
      if (diffuseTexture != mTextures.end()) {
        diffuseTex = diffuseTexture->second;
      }
    }

    /* switch between animated and non-animated pipeline layout */
    VkPipelineLayout renderLayout;
    if (hasAnimations()) {
      if (selectionModeActive) {
        renderLayout = renderData.rdAssimpSkinningSelectionPipelineLayout;
      } else {
        renderLayout = renderData.rdAssimpSkinningPipelineLayout;
      }
    } else {
      if (selectionModeActive) {
        renderLayout = renderData.rdAssimpSelectionPipelineLayout;
      } else {
        renderLayout = renderData.rdAssimpPipelineLayout;
      }
    }

    if (diffuseTex.image != VK_NULL_HANDLE) {
      vkCmdBindDescriptorSets(renderData.rdCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        renderLayout, 0, 1, &diffuseTex.descriptorSet, 0, nullptr);
    } else {
      if (mesh.usesPBRColors) {
        vkCmdBindDescriptorSets(renderData.rdCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
          renderLayout, 0, 1, &mWhiteTexture.descriptorSet, 0, nullptr);
      } else {
        vkCmdBindDescriptorSets(renderData.rdCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
          renderLayout, 0, 1, &mPlaceholderTexture.descriptorSet, 0, nullptr);
      }
    }

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(renderData.rdCommandBuffer, 0, 1, &mVertexBuffers.at(i).buffer, &offset);
    vkCmdBindIndexBuffer(renderData.rdCommandBuffer, mIndexBuffers.at(i).buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(renderData.rdCommandBuffer, static_cast<uint32_t>(mesh.indices.size()), instanceCount, 0, 0, 0);
  }
}

unsigned int AssimpModel::getTriangleCount() {
  return mTriangleCount;
}

void AssimpModel::cleanup(VkRenderData &renderData) {
  vkFreeDescriptorSets(renderData.rdVkbDevice.device, renderData.rdDescriptorPool, 1, &mTransformPerModelDescriptorSet);
  vkFreeDescriptorSets(renderData.rdVkbDevice.device, renderData.rdDescriptorPool, 1, &mMatrixMultPerModelDescriptorSet);

  for (auto buffer : mVertexBuffers) {
    VertexBuffer::cleanup(renderData, buffer);
  }
  for (auto buffer : mIndexBuffers) {
    IndexBuffer::cleanup(renderData, buffer);
  }

  ShaderStorageBuffer::cleanup(renderData, mShaderBoneMatrixOffsetBuffer);
  ShaderStorageBuffer::cleanup(renderData, mShaderBoneParentBuffer);
  ShaderStorageBuffer::cleanup(renderData, mAnimLookupBuffer);

  for (auto& tex : mTextures) {
    Texture::cleanup(renderData, tex.second);
  }

  Texture::cleanup(renderData, mPlaceholderTexture);
  Texture::cleanup(renderData, mWhiteTexture);
}

std::string AssimpModel::getModelFileName() {
  return mModelSettings.msModelFilename;
}

std::string AssimpModel::getModelFileNamePath() {
  return mModelSettings.msModelFilenamePath;
}

const std::vector<std::shared_ptr<AssimpNode>>& AssimpModel::getNodeList() {
  return mNodeList;
}

const std::unordered_map<std::string, std::shared_ptr<AssimpNode>>& AssimpModel::getNodeMap() {
  return mNodeMap;
}

const std::vector<std::shared_ptr<AssimpBone>>& AssimpModel::getBoneList() {
  return mBoneList;
}

const std::vector<std::string> AssimpModel::getBoneNameList() {
  return mBoneNameList;
}

const std::vector<std::shared_ptr<AssimpAnimClip>>& AssimpModel::getAnimClips() {
  return mAnimClips;
}

bool AssimpModel::hasAnimations() {
  return !mAnimClips.empty();
}

VkShaderStorageBufferData& AssimpModel::getBoneMatrixOffsetBuffer() {
  return mShaderBoneMatrixOffsetBuffer;
}

VkShaderStorageBufferData& AssimpModel::getBoneParentBuffer() {
  return mShaderBoneParentBuffer;
}

VkShaderStorageBufferData& AssimpModel::getAnimLookupBuffer() {
  return mAnimLookupBuffer;
}

VkDescriptorSet& AssimpModel::getMatrixMultDescriptorSet() {
  return mMatrixMultPerModelDescriptorSet;
}

VkDescriptorSet& AssimpModel::getTransformDescriptorSet() {
  return mTransformPerModelDescriptorSet;
}

void AssimpModel::setModelSettings(ModelSettings settings) {
  mModelSettings = settings;
}

ModelSettings AssimpModel::getModelSettings() {
  return mModelSettings;
}

float AssimpModel::getMaxClipDuration() {
  return mMaxClipDuration;
}

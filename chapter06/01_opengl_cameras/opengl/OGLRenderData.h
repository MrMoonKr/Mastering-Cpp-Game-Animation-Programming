/* OpenGL */
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

#include <glm/glm.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <assimp/material.h>

#include "Enums.h"
#include "Callbacks.h"

struct OGLVertex {
  glm::vec4 position = glm::vec4(0.0f); // last float is uv.x
  glm::vec4 color = glm::vec4(1.0f);
  glm::vec4 normal = glm::vec4(0.0f); // last float is uv.y
  glm::uvec4 boneNumber = glm::uvec4(0);
  glm::vec4 boneWeight = glm::vec4(0.0f);
};

struct OGLMesh {
  std::vector<OGLVertex> vertices{};
  std::vector<uint32_t> indices{};
  std::unordered_map<aiTextureType, std::string> textures{};
  bool usesPBRColors = false;
};

struct OGLLineVertex {
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 color = glm::vec3(0.0f);
};

struct OGLLineMesh {
  std::vector<OGLLineVertex> vertices{};
};

/* data format to be uploaded to compute shader */
struct NodeTransformData {
  glm::vec4 translation = glm::vec4(0.0f);
  glm::vec4 scale = glm::vec4(1.0f);
  glm::vec4 rotation = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); // this is a quaternion
};

struct OGLRenderData {
  GLFWwindow *rdWindow = nullptr;

  int rdWidth = 0;
  int rdHeight = 0;
  bool rdFullscreen = false;

  unsigned int rdTriangleCount = 0;
  unsigned int rdMatricesSize = 0;

  float rdFrameTime = 0.0f;
  float rdMatrixGenerateTime = 0.0f;
  float rdUploadToVBOTime = 0.0f;
  float rdUploadToUBOTime = 0.0f;
  float rdUIGenerateTime = 0.0f;
  float rdUIDrawTime = 0.0f;

  int rdMoveForward = 0;
  int rdMoveRight = 0;
  int rdMoveUp = 0;

  bool rdHighlightSelectedInstance = false;
  float rdSelectedInstanceHighlightValue = 1.0f;

  appMode rdApplicationMode = appMode::edit;
  instanceEditMode rdInstanceEditMode = instanceEditMode::move;

  appExitCallback rdAppExitCallbackFunction;
  bool rdRequestApplicationExit = false;
  bool rdNewConfigRequest = false;
  bool rdLoadConfigRequest = false;
  bool rdSaveConfigRequest = false;
};

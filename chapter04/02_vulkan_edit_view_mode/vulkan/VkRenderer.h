/* Vulkan renderer */
#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <functional>

#include <glm/glm.hpp>

/* Vulkan also before GLFW */
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

#include "Timer.h"
#include "Texture.h"
#include "UniformBuffer.h"
#include "UserInterface.h"
#include "VertexBuffer.h"
#include "ShaderStorageBuffer.h"
#include "Camera.h"
#include "CoordArrowsModel.h"
#include "RotationArrowsModel.h"
#include "ScaleArrowsModel.h"
#include "AssimpModel.h"
#include "AssimpInstance.h"

#include "VkRenderData.h"
#include "ModelAndInstanceData.h"

using getWindowTitleCallback = std::function<std::string(void)>;
using setWindowTitleCallback = std::function<void(std::string)>;

class VkRenderer {
  public:
    VkRenderer(GLFWwindow *window);

    bool init(unsigned int width, unsigned int height);
    void setSize(unsigned int width, unsigned int height);

    bool draw(float deltaTime);

    void handleKeyEvents(int key, int scancode, int action, int mods);
    void handleMouseButtonEvents(int button, int action, int mods);
    void handleMousePositionEvents(double xPos, double yPos);

    bool hasModel(std::string modelFileName);
    std::shared_ptr<AssimpModel> getModel(std::string modelFileName);
    bool addModel(std::string modelFileName);
    void deleteModel(std::string modelFileName);

    std::shared_ptr<AssimpInstance> addInstance(std::shared_ptr<AssimpModel> model);
    void addInstances(std::shared_ptr<AssimpModel> model, int numInstances);
    void deleteInstance(std::shared_ptr<AssimpInstance> instance);
    void cloneInstance(std::shared_ptr<AssimpInstance> instance);
    void cloneInstances(std::shared_ptr<AssimpInstance> instance, int numClones);

    void centerInstance(std::shared_ptr<AssimpInstance> instance);

    void cleanup();

    setWindowTitleCallback setWindowTitle;
    getWindowTitleCallback getWindowTitle;

  private:
    VkRenderData mRenderData{};
    ModelAndInstanceData mModelInstData{};

    Timer mFrameTimer{};
    Timer mMatrixGenerateTimer{};
    Timer mUploadToVBOTimer{};
    Timer mUploadToUBOTimer{};
    Timer mUIGenerateTimer{};
    Timer mUIDrawTimer{};

    UserInterface mUserInterface{};
    Camera mCamera{};

    VkPushConstants mModelData{};
    VkComputePushConstants mComputeModelData{};
    VkUniformBufferData mPerspectiveViewMatrixUBO{};
    VkVertexBufferData mLineVertexBuffer{};

    /* for animated and non-animated models */
    VkShaderStorageBufferData mShaderModelRootMatrixBuffer{};
    std::vector<glm::mat4> mWorldPosMatrices{};

    /* color hightlight for selection etc */
    std::vector<glm::vec2> mSelectedInstance{};
    VkShaderStorageBufferData mSelectedInstanceBuffer{};

    /* for animated models */
    VkShaderStorageBufferData mShaderBoneMatrixBuffer{};

    /* for compute shader */
    bool mHasDedicatedComputeQueue = false;
    VkShaderStorageBufferData mShaderTRSMatrixBuffer{};
    VkShaderStorageBufferData mShaderNodeTransformBuffer{};
    std::vector<NodeTransformData> mShaderNodeTransFormData{};

    CoordArrowsModel mCoordArrowsModel{};
    RotationArrowsModel mRotationArrowsModel{};
    ScaleArrowsModel mScaleArrowsModel{};
    VkLineMesh mCoordArrowsMesh{};

    unsigned int mCoordArrowsLineIndexCount = 0;

    std::shared_ptr<VkLineMesh> mLineMesh = nullptr;

    bool mMouseLock = false;
    int mMouseXPos = 0;
    int mMouseYPos = 0;
    bool mMousePick = false;

    bool mMouseMove = false;
    bool mMouseMoveVertical = false;
    int mMouseMoveVerticalShiftKey = 0;
    InstanceSettings mSavedInstanceSettings{};

    void handleMovementKeys();
    void updateTriangleCount();
    void assignInstanceIndices();

    /* identity matrices */
    VkUploadMatrices mMatrices{ glm::mat4(1.0f), glm::mat4(1.0f) };

    std::string mOrigWindowTitle;
    void setModeInWindowTitle();

    void undoLastOperation();
    void redoLastOperation();

    /* Vulkan specific code */
    VkSurfaceKHR mSurface = VK_NULL_HANDLE;
    VmaAllocator rdAllocator = nullptr;

    VkDeviceSize mMinSSBOOffsetAlignment = 0;

    bool deviceInit();
    bool getQueues();
    bool initVma();
    bool createDescriptorPool();
    bool createDescriptorLayouts();
    bool createDescriptorSets();
    bool createDepthBuffer();
    bool createSelectionImage();
    bool createLineVertexBuffer();
    bool createMatrixUBO();
    bool createSSBOs();
    bool createSwapchain();
    bool createRenderPass();
    bool createPipelineLayouts();
    bool createPipelines();
    bool createFramebuffer();
    bool createCommandPools();
    bool createCommandBuffers();
    bool createSyncObjects();

    bool initUserInterface();

    bool recreateSwapchain();

    void updateDescriptorSets();
    void updateComputeDescriptorSets();

    void runComputeShaders(std::shared_ptr<AssimpModel> model, int numInstances, uint32_t modelOffset);
};

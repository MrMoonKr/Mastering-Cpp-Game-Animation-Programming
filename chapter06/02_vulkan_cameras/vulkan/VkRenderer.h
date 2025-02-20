/* Vulkan renderer */
#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <chrono>

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
#include "CameraSettings.h"
#include "CoordArrowsModel.h"
#include "RotationArrowsModel.h"
#include "ScaleArrowsModel.h"
#include "AssimpModel.h"
#include "AssimpInstance.h"

#include "VkRenderData.h"
#include "ModelInstanceCamData.h"

class VkRenderer {
  public:
    VkRenderer(GLFWwindow *window);

    bool init(unsigned int width, unsigned int height);
    void setSize(unsigned int width, unsigned int height);

    bool draw(float deltaTime);

    void handleKeyEvents(int key, int scancode, int action, int mods);
    void handleMouseButtonEvents(int button, int action, int mods);
    void handleMousePositionEvents(double xPos, double yPos);
    void handleMouseWheelEvents(double xOffset, double yOffset);

    void addNullModelAndInstance();
    void removeAllModelsAndInstances();

    bool hasModel(std::string modelFileName);
    std::shared_ptr<AssimpModel> getModel(std::string modelFileName);
    bool addModel(std::string modelFileName, bool addInitialInstance = true, bool withUndo = true);
    void addExistingModel(std::shared_ptr<AssimpModel> model, int indexPos);
    void deleteModel(std::string modelFileName, bool withUnd = true);

    std::shared_ptr<AssimpInstance> addInstance(std::shared_ptr<AssimpModel> model, bool withUndo = true);
    void addExistingInstance(std::shared_ptr<AssimpInstance> instance, int indexPos, int indexPerModelPos);
    void addInstances(std::shared_ptr<AssimpModel> model, int numInstances);
    void deleteInstance(std::shared_ptr<AssimpInstance> instance, bool withUndo = true);
    void cloneInstance(std::shared_ptr<AssimpInstance> instance);
    void cloneInstances(std::shared_ptr<AssimpInstance> instance, int numClones);
    std::shared_ptr<AssimpInstance> getInstanceById(int instanceId);

    void centerInstance(std::shared_ptr<AssimpInstance> instance);

    void requestExitApplication();
    void doExitApplication();

    ModelInstanceCamData& getModInstCamData();

    void cleanup();

  private:
    VkRenderData mRenderData{};
    ModelInstanceCamData mModelInstCamData{};

    Timer mFrameTimer{};
    Timer mMatrixGenerateTimer{};
    Timer mUploadToVBOTimer{};
    Timer mUploadToUBOTimer{};
    Timer mUIGenerateTimer{};
    Timer mUIDrawTimer{};

    UserInterface mUserInterface{};

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
    float mMouseWheelScale = 1.0f;
    int mMouseWheelScaleShiftKey = 0;
    bool mMouseWheelScrolling = false;
    std::chrono::time_point<std::chrono::steady_clock> mMouseWheelLastScrollTime{};
    CameraSettings mSavedCameraWheelSettings{};

    bool mMousePick = false;
    int mSavedSelectedInstanceId = 0;

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

    void toggleFullscreen();

    bool mApplicationRunning = false;

    void undoLastOperation();
    void redoLastOperation();

    void createSettingsContainerCallbacks();
    void clearUndoRedoStacks();

    const std::string mDefaultConfigFileName = "config/conf.acfg";
    bool loadConfigFile(std::string configFileName);
    bool saveConfigFile(std::string configFileName);
    void createEmptyConfig();

    void loadDefaultFreeCam();

    bool mConfigIsDirty = false;
    std::string mWindowTitleDirtySign;
    void setConfigDirtyFlag(bool flag);
    bool getConfigDirtyFlag();

    void cloneCamera();
    void deleteCamera();
    CameraSettings mSavedCameraSettings{};

    std::string generateUniqueCameraName(std::string camBaseName);
    bool checkCameraNameUsed(std::string cameraName);

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

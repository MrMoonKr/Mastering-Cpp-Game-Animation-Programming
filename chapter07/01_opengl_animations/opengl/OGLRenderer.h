#pragma once
#include <vector>
#include <string>
#include <memory>
#include <chrono>

#include <glm/glm.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Timer.h"
#include "Framebuffer.h"
#include "LineVertexBuffer.h"
#include "Texture.h"
#include "Shader.h"
#include "UniformBuffer.h"
#include "ShaderStorageBuffer.h"
#include "UserInterface.h"
#include "CameraSettings.h"
#include "ModelSettings.h"
#include "CoordArrowsModel.h"
#include "RotationArrowsModel.h"
#include "ScaleArrowsModel.h"
#include "AssimpModel.h"
#include "AssimpInstance.h"

#include "OGLRenderData.h"
#include "ModelInstanceCamData.h"

class OGLRenderer {
  public:
    OGLRenderer(GLFWwindow *window);

    bool init(unsigned int width, unsigned int height);
    void setSize(unsigned int width, unsigned int height);
    void uploadAssimpData(OGLMesh vertexData);
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
    OGLRenderData mRenderData{};
    ModelInstanceCamData mModelInstCamData{};

    Timer mFrameTimer{};
    Timer mMatrixGenerateTimer{};
    Timer mIKTimer{};
    Timer mUploadToVBOTimer{};
    Timer mUploadToUBOTimer{};
    Timer mUIGenerateTimer{};
    Timer mUIDrawTimer{};

    Shader mLineShader{};
    Shader mAssimpShader{};
    Shader mAssimpSkinningShader{};

    Shader mAssimpSelectionShader{};
    Shader mAssimpSkinningSelectionShader{};

    Shader mAssimpTransformComputeShader{};
    Shader mAssimpMatrixComputeShader{};

    Framebuffer mFramebuffer{};
    LineVertexBuffer mLineVertexBuffer{};
    UniformBuffer mUniformBuffer{};
    UserInterface mUserInterface{};

    /* for animated and non-animated models */
    ShaderStorageBuffer mShaderModelRootMatrixBuffer{};
    std::vector<glm::mat4> mWorldPosMatrices{};

    /* color hightlight for selection etc */
    std::vector<glm::vec2> mSelectedInstance{};
    ShaderStorageBuffer mSelectedInstanceBuffer{};

    /* for animated models */
    ShaderStorageBuffer mShaderBoneMatrixBuffer{};
    std::vector<PerInstanceAnimData> mPerInstanceAnimData{};
    ShaderStorageBuffer mPerInstanceAnimDataBuffer{};

    /* for compute shader */
    ShaderStorageBuffer mShaderTRSMatrixBuffer{};

    CoordArrowsModel mCoordArrowsModel{};
    RotationArrowsModel mRotationArrowsModel{};
    ScaleArrowsModel mScaleArrowsModel{};
    OGLLineMesh mCoordArrowsMesh{};
    std::shared_ptr<OGLLineMesh> mLineMesh = nullptr;

    unsigned int mCoordArrowsLineIndexCount = 0;

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

    /* create identity matrix by default */
    glm::mat4 mViewMatrix = glm::mat4(1.0f);
    glm::mat4 mProjectionMatrix = glm::mat4(1.0f);

    std::string mOrigWindowTitle;
    void setModeInWindowTitle();
    void setAppMode(appMode newMode);

    void toggleFullscreen();
    void checkMouseEnable();

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
};

#pragma once
#include <string>
#include <memory>
#include <chrono>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "OGLRenderer.h"
#include "AudioManager.h"
#include "Callbacks.h"

class Window {
  public:
    bool init(unsigned int width, unsigned int height, std::string title);
    void mainLoop();
    void cleanup();

    GLFWwindow* getWindow();
    std::string getWindowTitle();
    void setWindowTitle(std::string newTitle);

  private:
    GLFWwindow *mWindow = nullptr;

    std::string mWindowTitle;

    std::unique_ptr<OGLRenderer> mRenderer = nullptr;

    AudioManager mAudioManager{};
};

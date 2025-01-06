#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include <yaml-cpp/yaml.h>

#include "YamlParserTypes.h"
#include "ModelInstanceCamData.h"
#include "InstanceSettings.h"
#include "CameraSettings.h"
#include "OGLRenderData.h"

class YamlParser {
  public:
    /* loading */
    bool loadYamlFile(std::string fileName);
    std::string getFileName();
    bool checkFileVersion();

    std::vector<std::string> getModelFileNames();
    std::vector<ExtendedInstanceSettings> getInstanceConfigs();
    std::vector<CameraSettings> getCameraConfigs();

    int getSelectedModelNum();
    int getSelectedInstanceNum();
    int getSelectedCameraNum();
    bool getHighlightActivated();

    glm::vec3 getCameraPosition();
    float getCameraElevation();
    float getCameraAzimuth();

    /* saving */
    bool createConfigFile(OGLRenderData renderData, ModelInstanceCamData modInstCamData);
    bool writeYamlFile(std::string fileName);

    /* misc */
    bool hasKey(std::string key);
    bool getValue(std::string key, std::string& value);

  private:
    void createInstanceToCamMap(ModelInstanceCamData modInstCamData);
    std::unordered_map<int, std::vector<std::string>> mInstanceToCamMap;

    std::string mYamlFileName;
    YAML::Node mYamlNode{};
    YAML::Emitter mYamlEmit{};

    const std::string mYamlConfigFileVersion = "2.0";
    std::string mYamlFileVersion;
};

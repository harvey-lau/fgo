/**
 *
 *
 */

#include "FGoFuzzingUtils.h"

#include "FGoDefs.h"

#include "json/json.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

static std::string globalError;

int parse(const std::string &infoDirectory, target_info_t *target_info)
{
    if (!target_info) {
        globalError = "Null pointer of target information item";
        return -1;
    }

    std::string infoDir = infoDirectory;
    if (infoDir.empty()) {
        char *tmpValueFromEnv = getenv(DIST_DIR_ENVAR);
        if (!tmpValueFromEnv) {
            globalError =
                std::string("Failed to find information directory from environment variable '"
                ) +
                DIST_DIR_ENVAR + "'";
            return -1;
        }
        infoDir = tmpValueFromEnv;
    }

    // Information directory
    auto infoDirPath = std::filesystem::path(infoDir);
    if (!std::filesystem::exists(infoDirPath)) {
        globalError = "The information directory '" + infoDirPath.string() + "' doesn't exist";
        return -1;
    }
    if (!std::filesystem::is_directory(infoDirPath)) {
        globalError = "The information directory path '" + infoDirPath.string() +
                      "' doesn't point to a directory";
        return -1;
    }

    // Target information file
    std::string targetFileName = std::string(TARGET_INFO_FILENAME) + ".json";
    auto infoJsonFile = infoDirPath / targetFileName;
    if (!std::filesystem::exists(infoJsonFile)) {
        globalError = "The information file '" + infoJsonFile.string() + "' doesn't exist";
        return -1;
    }
    if (!std::filesystem::is_regular_file(infoJsonFile)) {
        globalError = "The information directory path '" + infoJsonFile.string() +
                      "' doesn't point to a directory";
        return -1;
    }

    std::ifstream ifs(infoJsonFile.string());
    if (!ifs.is_open()) {
        globalError = "Failed to open target information file '" + infoJsonFile.string() + "'";
        return -1;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    builder["collectComments"] = true;
    JSONCPP_STRING errs;
    if (!Json::parseFromStream(builder, ifs, &root, &errs)) {
        globalError = "Invalid Json format. Error: " + errs +
                      ". The target information file '" + infoJsonFile.string() +
                      "' maybe destroyed";
        return -1;
    }

    if (!root.isMember("TargetCount") || (root["TargetCount"].type() != Json::uintValue &&
                                          root["TargetCount"].type() != Json::intValue))
    {
        globalError =
            "Failed to get target count from the Json value. The target information file '" +
            infoJsonFile.string() + "' maybe destroyed";
        return -1;
    }
    if (!root.isMember("TargetInfo") || root["TargetInfo"].type() != Json::arrayValue) {
        globalError = "Failed to get target information from the Json value. The target "
                      "information file '" +
                      infoJsonFile.string() + "' maybe destroyed";
        return -1;
    }
    uint32_t targetCount = root["TargetCount"].asUInt();
    if (root["TargetInfo"].size() != targetCount) {
        globalError = "Incompatible target count. The target "
                      "information file '" +
                      infoJsonFile.string() + "' maybe destroyed";
        return -1;
    }

    target_info->target_count = targetCount;
    target_info->target_start = (uint32_t *)malloc(sizeof(uint32_t) * targetCount);
    target_info->quantile_size = (uint32_t *)malloc(sizeof(uint32_t) * targetCount);
    target_info->target_quantile = (double **)malloc(sizeof(double *) * targetCount);
    for (Json::Value::ArrayIndex i = 0; i < targetCount; ++i) {
        if (!root["TargetInfo"][i].isMember("Start") ||
            (root["TargetInfo"][i]["Start"].type() != Json::uintValue &&
             root["TargetInfo"][i]["Start"].type() != Json::intValue))
        {
            globalError = "Failed to find item 'Start' at Target " + std::to_string(i) +
                          ". The target "
                          "information file '" +
                          infoJsonFile.string() + "' maybe destroyed";
            return -1;
        }
        if (!root["TargetInfo"][i].isMember("Quantile") ||
            root["TargetInfo"][i]["Quantile"].type() != Json::arrayValue)
        {
            globalError = "Failed to find item 'Quantile' at Target " + std::to_string(i) +
                          ". The target "
                          "information file '" +
                          infoJsonFile.string() + "' maybe destroyed";
            return -1;
        }
        target_info->target_start[i] = root["TargetInfo"][i]["Start"].asUInt();

        Json::Value::ArrayIndex quantileSize = root["TargetInfo"][i]["Quantile"].size();
        target_info->quantile_size[i] = quantileSize;
        target_info->target_quantile[i] = (double *)malloc(sizeof(double) * quantileSize);
        for (Json::Value::ArrayIndex j = 0; j < quantileSize; ++j) {
            if (root["TargetInfo"][i]["Quantile"][j].type() != Json::realValue) {
                globalError = "Failed to find Item " + std::to_string(j) +
                              " at 'Quantile' at Target " + std::to_string(i) +
                              ". The target "
                              "information file '" +
                              infoJsonFile.string() + "' maybe destroyed";
                return -1;
            }
            target_info->target_quantile[i][j] =
                root["TargetInfo"][i]["Quantile"][j].asDouble();
        }
    }

    return 0;
}

int parse(const char *info_dir, target_info_t *target_info)
{
    const char *tmp_info_dir = "";
    if (info_dir) tmp_info_dir = info_dir;
    return parse(std::string(tmp_info_dir), target_info);
}

const char *parse_error(void)
{
    return globalError.c_str();
}

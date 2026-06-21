#include "../include/core/settings.h"
#include "../include/core/settings_serialization_advanced.h"
#include <iostream>
#include <nlohmann/json.hpp>

namespace llama_gui {
namespace core {

using json = nlohmann::json;

// Forward declarations (without cv-qualifier for non-member functions)
void serializeDisplaySettings(json& j);
void serializeServerSettings(json& j);
void serializeChatSettings(json& j);
void serializeFileSettings(json& j);
void serializePerformanceSettings(json& j);
void serializeRagSettings(json& j);
void serializeOpenRouterSettings(json& j);
void serializeKiloCodeSettings(json& j);
void serializeUniversalOpenAISettings(json& j);
void serializeSamplingSettings(json& j);
void serializeModelLoadingSettings(json& j);
void serializeGpuSettings(json& j);
void serializeCacheSettings(json& j);
void serializeRopeSettings(json& j);
void serializeControlVectorSettings(json& j);
void serializeTensorOverrideSettings(json& j);

void deserializeDisplaySettings(const json& j);
void deserializeServerSettings(const json& j);
void deserializeChatSettings(const json& j);
void deserializeFileSettings(const json& j);
void deserializePerformanceSettings(const json& j);
void deserializeRagSettings(const json& j);
void deserializeOpenRouterSettings(const json& j);
void deserializeKiloCodeSettings(const json& j);
void deserializeUniversalOpenAISettings(const json& j);
void deserializeSamplingSettings(const json& j);
void deserializeModelLoadingSettings(const json& j);
void deserializeGpuSettings(const json& j);
void deserializeCacheSettings(const json& j);
void deserializeRopeSettings(const json& j);
void deserializeControlVectorSettings(const json& j);
void deserializeTensorOverrideSettings(const json& j);

// =========================================================================
// Основная сериализация - вызывает модульные функции
// =========================================================================

std::string Settings::serialize_to_json() const {
    json j;

    // Основные настройки
    serializeDisplaySettings(j);
    serializeServerSettings(j);
    serializeChatSettings(j);
    serializeFileSettings(j);
    serializePerformanceSettings(j);
    serializeRagSettings(j);
    serializeOpenRouterSettings(j);
    serializeKiloCodeSettings(j);
    serializeUniversalOpenAISettings(j);

    // Расширенные настройки llama.cpp
    serializeSamplingSettings(j);
    serializeModelLoadingSettings(j);
    serializeGpuSettings(j);
    serializeCacheSettings(j);
    serializeRopeSettings(j);
    serializeControlVectorSettings(j);
    serializeTensorOverrideSettings(j);

    // Настройки сервера и выполнения
    serializeServerRuntimeSettings(j);
    serializeBatchSettings(j);
    serializeGrammarSettings(j);
    serializeOutputSettings(j);

    // Статистика производительности моделей
    j["model_performance"] = json::parse(model_performance_manager_.to_json());

    // Custom settings
    j["custom"] = custom_settings_;

    return j.dump(4);
}

bool Settings::deserialize_from_json(const std::string& json_data) {
    try {
        auto j = json::parse(json_data);

        // Основные настройки
        deserializeDisplaySettings(j);
        deserializeServerSettings(j);
        deserializeChatSettings(j);
        deserializeFileSettings(j);
        deserializePerformanceSettings(j);
        deserializeRagSettings(j);
        deserializeOpenRouterSettings(j);
        deserializeKiloCodeSettings(j);
        deserializeUniversalOpenAISettings(j);

        // Расширенные настройки llama.cpp
        deserializeSamplingSettings(j);
        deserializeModelLoadingSettings(j);
        deserializeGpuSettings(j);
        deserializeCacheSettings(j);
        deserializeRopeSettings(j);
        deserializeControlVectorSettings(j);
        deserializeTensorOverrideSettings(j);

        // Настройки сервера и выполнения
        deserializeServerRuntimeSettings(j);
        deserializeBatchSettings(j);
        deserializeGrammarSettings(j);
        deserializeOutputSettings(j);

        // Статистика производительности моделей
        if (j.contains("model_performance")) {
            std::string perf_json = j["model_performance"].dump();
            model_performance_manager_.from_json(perf_json);
        }

        // Custom settings
        if (j.contains("custom")) {
            custom_settings_.clear();
            for (auto& [key, value] : j["custom"].items()) {
                custom_settings_[key] = value.get<std::string>();
            }
        }

        std::cout << "Settings successfully deserialized from JSON" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to deserialize settings: " << e.what() << std::endl;
        return false;
    }
}

} // namespace core
} // namespace llama_gui

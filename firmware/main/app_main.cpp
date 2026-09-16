#include "access_control.hpp"
#include "esp_log.h"
#include "presence_fusion.hpp"

namespace {
constexpr const char* TAG = "multi_pet_feeder";
}

extern "C" void app_main(void) {
    static mpf::AccessController access_controller{};
    static mpf::PresenceFusion presence_fusion{};

    (void)access_controller;
    (void)presence_fusion;

    ESP_LOGI(TAG, "Multi-pet feeder domain core initialized");
    ESP_LOGI(TAG, "Hardware drivers are intentionally not wired yet");
}

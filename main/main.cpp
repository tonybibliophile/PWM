#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "vtx_controller.hpp"

extern "C" void app_main(void)
{
    ESP_LOGI("main", "About to run the full VTX setup sequence...");
    run_vtx_sequence(SEQ_FULL_SETUP);
    ESP_LOGI("main", "VTX setup sequence is done. Entering idle loop.");
    while(true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
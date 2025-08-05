#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "vtx_controller.hpp" // <<< 只需要引入我們設計好的控制器公開介面

// ESP32 程式的唯一入口
extern "C" void app_main(void)
{
    ESP_LOGI("main", "About to run the full VTX setup sequence...");
    
    // 呼叫我們封裝好的單一函式，來完成所有 PWM 訊號的產生工作。
    // app_main 的執行會在這裡暫停，直到整個序列（包含所有延遲）都播放完畢。
    run_vtx_sequence(SEQ_FULL_SETUP);

    // 序列執行完畢後，這行日誌才會被印出。
    ESP_LOGI("main", "VTX setup sequence is done. Entering idle loop.");
    
    // 主任務已完成其主要工作，進入無限迴圈的待機狀態。
    while(true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
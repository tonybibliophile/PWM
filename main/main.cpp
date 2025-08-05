#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <vector>
#include "ComplexPwmSequencer.hpp"

// 定義硬體腳位的綽號
#define PWM_PIN        GPIO_NUM_32
#define PWM_TIMER      LEDC_TIMER_0 //若有其他需要用到TIMER 則可改成其他 TIMER_1,2,3
#define PWM_CHANNEL    LEDC_CHANNEL_0 //同理可改CHANNEL 0-7

// ESP32 程式的唯一入口
extern "C" void app_main(void)
{
    // 1. 定義 PWM 的基本行為 
    SequencerConfig config = {
        .speed_mode         = LEDC_HIGH_SPEED_MODE,
        .freqA_hz           = 270, // 低頻率
        .freqB_hz           = 400, // 高頻率
        .switch_interval_ms = 305  //頻率切換間隔(ms)
    };

    // 2. 建立序列器物件 
    ComplexPwmSequencer sequencer(PWM_PIN, PWM_TIMER, PWM_CHANNEL, config);

    // 3. 定義詳細的執行步驟 
    // 格式: {動作類型, 完成後延遲毫秒數}
    //  LARGE_CYCLE ：切換調整頻道
    //  SMALL_CYCLE ：調整參數
    std::vector<SequenceStep> sequence_to_run = {
        { LARGE_CYCLE, 1500 }, { LARGE_CYCLE, 1500 }, { LARGE_CYCLE, 1500 },
        { SMALL_CYCLE, 1500 }, { SMALL_CYCLE, 1500 }, { SMALL_CYCLE, 1500 },
        { LARGE_CYCLE, 0 }
    };

    // 4. 印出日誌，並命令序列器開始執行
    ESP_LOGI("main", "Starting non-blocking sequencer test...");
    sequencer.begin(sequence_to_run);

    // 5. 程式主迴圈，負責監督機器人直到任務完成
    while (!sequencer.isFinished()) {
        // 不斷呼叫 update() 來驅動狀態機
        sequencer.update();
        // 短暫休息，把 CPU 控制權讓給系統，非常重要！
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // 6. 序列執行完畢後的處理
    ESP_LOGI("main", "Task finished. Entering idle loop.");
    while(true) {
        // 讓主任務進入待機狀態
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
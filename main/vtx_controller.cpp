#include "vtx_controller.hpp"        // 引入我們自己定義的公開介面
#include "ComplexPwmSequencer.hpp" // 引入底層的 PWM 產生器工具
#include "freertos/task.h"             // 為了使用 vTaskDelay
#include "esp_log.h"
#include <vector>

// --- 內部定義 ---
// 這些定義只在此檔案內有效，將實作細節與外部完全隔離
static const char* TAG = "VTX_CONTROLLER";
#define PWM_PIN        GPIO_NUM_32
#define PWM_TIMER      LEDC_TIMER_0
#define PWM_CHANNEL    LEDC_CHANNEL_0

/**
 * @brief 實現 vtx_controller.hpp 中宣告的公開 API 函式
 * @param seq_type 要執行的序列類型
 */
void run_vtx_sequence(VtxSequenceType seq_type) {
    ESP_LOGI(TAG, "Executing VTX sequence...");

    // 1. 定義 PWM 硬體的基本行為參數。
    // 這些參數是根據您的硬體（圖傳）要求，經過實驗驗證的有效值。
    SequencerConfig config = {
        .speed_mode         = LEDC_HIGH_SPEED_MODE,
        .freqA_hz           = 270,
        .freqB_hz           = 400,
        .switch_interval_ms = 285 // 驗證有效的指令節奏
    };

    // 2. 建立底層的序列器物件。
    // 這個物件的生命週期僅限於本次函式呼叫，函式結束時會自動銷毀。
    ComplexPwmSequencer sequencer(PWM_PIN, PWM_TIMER, PWM_CHANNEL, config);

    // 3. 根據外部傳入的指令，準備對應的執行序列（食譜）。
    std::vector<SequenceStep> sequence_to_run;
    if (seq_type == SEQ_FULL_SETUP) {
        ESP_LOGI(TAG, "Sequence type: FULL_SETUP");
        // 這是您完整的「三大大循環 + 三小小循環 + 一大大循環」序列
        sequence_to_run = {
            { LARGE_CYCLE, 2000 }, { LARGE_CYCLE, 2000 }, { LARGE_CYCLE, 2000 },
            { SMALL_CYCLE, 2000 }, { SMALL_CYCLE, 2000 }, { SMALL_CYCLE, 2000 },
            { LARGE_CYCLE, 0 }
        }; // 使用 2000ms 作為驗證有效的延遲
    } else {
        // 如果傳入未知的指令類型，印出錯誤並直接返回。
        ESP_LOGE(TAG, "Unknown sequence type!");
        return; 
    }

    // 4. 執行完整的 PWM 序列產生流程。
    // 這個流程是阻塞式的，會直到序列全部完成才會繼續往下執行。
    sequencer.begin(sequence_to_run);
    while (!sequencer.isFinished()) {
        sequencer.update();
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    ESP_LOGI(TAG, "Sequence finished.");
    // 當函式執行到這裡結束時，sequencer 物件會被自動銷毀，
    // 其解構函式會呼叫 stop()，確保 PWM 被妥善關閉。
}
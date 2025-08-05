#include "vtx_controller.hpp"
#include "ComplexPwmSequencer.hpp"
#include "freertos/task.h"
#include "esp_log.h"
#include <vector>

static const char* TAG = "VTX_CONTROLLER";
#define PWM_PIN        GPIO_NUM_32
#define PWM_TIMER      LEDC_TIMER_0
#define PWM_CHANNEL    LEDC_CHANNEL_0

void run_vtx_sequence(VtxSequenceType seq_type) {
    ESP_LOGI(TAG, "Executing VTX sequence...");
    SequencerConfig config = { .speed_mode = LEDC_HIGH_SPEED_MODE, .freqA_hz = 270, .freqB_hz = 400, .switch_interval_ms = 285 };
    ComplexPwmSequencer sequencer(PWM_PIN, PWM_TIMER, PWM_CHANNEL, config);
    std::vector<SequenceStep> sequence_to_run;
    if (seq_type == SEQ_FULL_SETUP) {
        ESP_LOGI(TAG, "Sequence type: FULL_SETUP");
        sequence_to_run = {
            { LARGE_CYCLE, 2000 }, { LARGE_CYCLE, 2000 }, { LARGE_CYCLE, 2000 },
            { SMALL_CYCLE, 2000 }, { SMALL_CYCLE, 2000 }, { SMALL_CYCLE, 2000 },
            { LARGE_CYCLE, 0 }
        };
    } else {
        ESP_LOGE(TAG, "Unknown sequence type!");
        return; 
    }
    sequencer.begin(sequence_to_run);
    while (!sequencer.isFinished()) {
        sequencer.update();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    ESP_LOGI(TAG, "Sequence finished.");
}
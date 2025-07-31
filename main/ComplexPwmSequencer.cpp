#include "ComplexPwmSequencer.hpp"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "ComplexSequencer";

ComplexPwmSequencer::ComplexPwmSequencer(gpio_num_t pwmPin, ledc_timer_t timer, ledc_channel_t channel, const SequencerConfig& config)
: _pwmPin(pwmPin), _ledcTimer(timer), _ledcChannel(channel), _config(config) {
    _currentState = IDLE;
}

ComplexPwmSequencer::~ComplexPwmSequencer() {
    stop();
}

// 每次啟動一個循環前，都呼叫此函式，進行完整的硬體設定
void ComplexPwmSequencer::initialize_hardware() {
    ESP_LOGI(TAG, "Configuring LEDC hardware for new cycle...");
    ledc_timer_config_t timer_config = {
        .speed_mode = _config.speed_mode, .duty_resolution = LEDC_TIMER_13_BIT, .timer_num = _ledcTimer,
        .freq_hz = _config.freqA_hz, .clk_cfg = LEDC_AUTO_CLK, .deconfigure = false
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    ledc_channel_config_t channel_config = {
        .gpio_num = _pwmPin, .speed_mode = _config.speed_mode, .channel = _ledcChannel,
        .intr_type = LEDC_INTR_DISABLE, .timer_sel = _ledcTimer, .duty = 4096, .hpoint = 0,
        .flags = { .output_invert = 0 } 
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
}

void ComplexPwmSequencer::begin(const std::vector<SequenceStep>& sequence) {
    if (sequence.empty()) { 
        ESP_LOGW(TAG, "begin() called with an empty sequence."); 
        return; 
    }
    
    // 執行第一次的硬體初始化
    initialize_hardware();
    
    ESP_LOGI(TAG, "Starting sequence. Total steps: %zu", sequence.size());

    // 設定內部狀態以啟動第一個循環
    portENTER_CRITICAL(&_lock);
    _sequence = sequence;
    _currentStepIndex = 0;
    _currentState = RUNNING_PWM;
    _pwmToggleCount = 0;
    const auto& firstStep = _sequence[_currentStepIndex];
    _pwmToggleTarget = (firstStep.type == LARGE_CYCLE) ? 4 : 2;
    _lastChangeTime = _millis();
    portEXIT_CRITICAL(&_lock);

    // 讓硬體設定生效
    ledc_update_duty(_config.speed_mode, _ledcChannel);
    ESP_LOGI(TAG, "Step 1: Running %s", firstStep.type == LARGE_CYCLE ? "Large Cycle" : "Small Cycle");
}

void ComplexPwmSequencer::update() {
    portENTER_CRITICAL(&_lock);
    State local_currentState = _currentState;
    int64_t local_lastChangeTime = _lastChangeTime;
    int local_currentStepIndex = _currentStepIndex;
    int local_pwmToggleCount = _pwmToggleCount;
    int local_pwmToggleTarget = _pwmToggleTarget;
    portEXIT_CRITICAL(&_lock);

    if (local_currentState == IDLE) { return; }

    int64_t currentTime = _millis();

    switch (local_currentState) {
        case RUNNING_PWM:
            if (currentTime - local_lastChangeTime >= _config.switch_interval_ms) {
                int next_toggle_count = local_pwmToggleCount + 1;
                
                if (next_toggle_count >= local_pwmToggleTarget) {
                    // PWM表演結束，關閉訊號準備延遲
                    ledc_set_duty(_config.speed_mode, _ledcChannel, 0);
                    ledc_update_duty(_config.speed_mode, _ledcChannel);
                    ESP_LOGI(TAG, "    ↳ Signal OFF for delay period.");
                    ESP_LOGI(TAG, "Step %d PWM finished. Waiting for %lu ms...", local_currentStepIndex + 1, _sequence[local_currentStepIndex].post_delay_ms);
                    
                    portENTER_CRITICAL(&_lock);
                    _currentState = INTER_STEP_WAIT;
                    _lastChangeTime = currentTime;
                    portEXIT_CRITICAL(&_lock);
                } else {
                    // 繼續切換頻率
                    uint32_t nextFreq = ((next_toggle_count % 2) == 1) ? _config.freqB_hz : _config.freqA_hz;
                    ledc_set_freq(_config.speed_mode, _ledcTimer, nextFreq);
                    ESP_LOGI(TAG, "    ↳ Freq set to %d Hz", (int)nextFreq);
                    
                    portENTER_CRITICAL(&_lock);
                    _pwmToggleCount = next_toggle_count;
                    _lastChangeTime = currentTime;
                    portEXIT_CRITICAL(&_lock);
                }
            }
            break;

        case INTER_STEP_WAIT:
            if (currentTime - local_lastChangeTime >= _sequence[local_currentStepIndex].post_delay_ms) {
                int next_step_index = local_currentStepIndex + 1;

                if (next_step_index >= _sequence.size()) {
                    stop();
                } else {
                    // *** 關鍵解決方案：在開始下一步前，重新初始化整個硬體 ***
                    initialize_hardware();
                    
                    const auto& nextStep = _sequence[next_step_index];
                    ESP_LOGI(TAG, "Step %d: Running %s", next_step_index + 1, nextStep.type == LARGE_CYCLE ? "Large Cycle" : "Small Cycle");
                    
                    // 讓重新初始化的設定生效 (duty=4096)
                    ledc_update_duty(_config.speed_mode, _ledcChannel);
                    
                    // 更新內部狀態以開始下一個循環
                    portENTER_CRITICAL(&_lock);
                    _currentState = RUNNING_PWM;
                    _currentStepIndex = next_step_index;
                    _pwmToggleCount = 0;
                    _pwmToggleTarget = (nextStep.type == LARGE_CYCLE) ? 4 : 2;
                    _lastChangeTime = currentTime;
                    portEXIT_CRITICAL(&_lock);
                }
            }
            break;
            
        default:
            break;
    }
}

void ComplexPwmSequencer::stop() {
    portENTER_CRITICAL(&_lock);
    bool was_running = (_currentState != IDLE);
    _currentState = IDLE;
    portEXIT_CRITICAL(&_lock);

    if (was_running) {
        ESP_LOGI(TAG, "Sequence stopped.");
        ledc_stop(_config.speed_mode, _ledcChannel, 0);
    }
}

bool ComplexPwmSequencer::isFinished() {
    portENTER_CRITICAL(&_lock);
    bool finished = (_currentState == IDLE);
    portEXIT_CRITICAL(&_lock);
    return finished;
}

int64_t ComplexPwmSequencer::_millis() {
    return esp_timer_get_time() / 1000;
}
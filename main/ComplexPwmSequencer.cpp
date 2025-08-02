// 引入標頭檔，包含類別的宣告、日誌功能和計時器功能
#include "ComplexPwmSequencer.hpp"
#include "esp_log.h"
#include "esp_timer.h"

// 為本檔案的日誌訊息設定一個統一的標籤
static const char* TAG = "ComplexSequencer";

/**
 * @brief 建構函式 (Constructor)
 * @details 當一個新的 ComplexPwmSequencer 物件被建立時，此函式會被呼叫，
 * 用於初始化物件的內部變數。
 */
ComplexPwmSequencer::ComplexPwmSequencer(gpio_num_t pwmPin, ledc_timer_t timer, ledc_channel_t channel, const SequencerConfig& config)
: _pwmPin(pwmPin), _ledcTimer(timer), _ledcChannel(channel), _config(config) {
    // 將物件的初始狀態設定為 IDLE (閒置)
    _currentState = IDLE;
}

/**
 * @brief 解構函式 (Destructor)
 * @details 當物件被銷毀時（例如離開其作用域），此函式會自動被呼叫。
 */
ComplexPwmSequencer::~ComplexPwmSequencer() {
    // 確保在物件銷毀前，PWM訊號被妥善關閉
    stop();
}

/**
 * @brief 初始化LEDC硬體
 * @details 此函式負責設定ESP32的LEDC計時器和通道。
 * 這是解決硬體相容性問題的關鍵：在每個循環開始前都呼叫它，
 * 以進行一次最徹底的硬體「冷啟動」。
 */
void ComplexPwmSequencer::initialize_hardware() {
    ESP_LOGI(TAG, "Configuring LEDC hardware for new cycle...");
    // 1. 設定LEDC計時器
    ledc_timer_config_t timer_config = {
        .speed_mode = _config.speed_mode,
        .duty_resolution = LEDC_TIMER_13_BIT, // 13位元解析度 (0-8191)
        .timer_num = _ledcTimer,
        .freq_hz = _config.freqA_hz,          // 設定初始頻率
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    // 2. 設定LEDC通道
    ledc_channel_config_t channel_config = {
        .gpio_num = _pwmPin,
        .speed_mode = _config.speed_mode,
        .channel = _ledcChannel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = _ledcTimer,
        .duty = 4096, // 初始工作週期設定為50% (4096 / 8192)
        .hpoint = 0,
        .flags = { .output_invert = 0 } 
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
}

/**
 * @brief 開始執行序列
 * @param sequence 一個包含所有步驟的指令清單
 */
void ComplexPwmSequencer::begin(const std::vector<SequenceStep>& sequence) {
    if (sequence.empty()) { 
        ESP_LOGW(TAG, "begin() called with an empty sequence."); //無給定調整順序
        return; 
    }
    
    // 執行第一次的硬體初始化
    initialize_hardware();
    
    ESP_LOGI(TAG, "Starting sequence. Total steps: %zu", sequence.size());

    // 進入臨界區，安全地設定啟動狀態
    portENTER_CRITICAL(&_lock);
    _sequence = sequence;
    _currentStepIndex = 0;
    _currentState = RUNNING_PWM;
    _pwmToggleCount = 0;
    const auto& firstStep = _sequence[_currentStepIndex];
    _pwmToggleTarget = (firstStep.type == LARGE_CYCLE) ? 4 : 2; // 設定第一步的目標
    _lastChangeTime = _millis(); // 記錄開始時間
    portEXIT_CRITICAL(&_lock);

    // 讓硬體設定（主要是duty）生效，開始輸出訊號
    ledc_update_duty(_config.speed_mode, _ledcChannel);
    ESP_LOGI(TAG, "Step 1: Running %s", firstStep.type == LARGE_CYCLE ? "Large Cycle" : "Small Cycle");
}

/**
 * @brief 狀態機更新函式 (心跳)
 * @details 由主迴圈反覆呼叫，根據時間和當前狀態來驅動序列前進。
 */
void ComplexPwmSequencer::update() {
    // 快速、安全地讀取一份當前狀態的快照
    portENTER_CRITICAL(&_lock);
    State local_currentState = _currentState;
    int64_t local_lastChangeTime = _lastChangeTime;
    int local_currentStepIndex = _currentStepIndex;
    int local_pwmToggleCount = _pwmToggleCount;
    int local_pwmToggleTarget = _pwmToggleTarget;
    portEXIT_CRITICAL(&_lock);

    // 如果是閒置狀態，則直接返回
    if (local_currentState == IDLE) { return; }

    int64_t currentTime = _millis();

    // 根據當前狀態，執行不同的邏輯
    switch (local_currentState) {
        // --- 狀態：正在產生PWM訊號 ---
        case RUNNING_PWM:
            // 判斷是否到了該切換頻率的時間點
            if (currentTime - local_lastChangeTime >= _config.switch_interval_ms) {
                int next_toggle_count = local_pwmToggleCount + 1;
                
                // 判斷本輪PWM是否已達到目標切換次數
                if (next_toggle_count >= local_pwmToggleTarget) {
                    // 目標達成，關閉訊號，準備進入延遲等待
                    ledc_set_duty(_config.speed_mode, _ledcChannel, 0);
                    ledc_update_duty(_config.speed_mode, _ledcChannel);
                    ESP_LOGI(TAG, "    ↳ Signal OFF for delay period.");
                    ESP_LOGI(TAG, "Step %d PWM finished. Waiting for %lu ms...", local_currentStepIndex + 1, _sequence[local_currentStepIndex].post_delay_ms);
                    
                    // 更新狀態到「等待延遲」，並重設計時器
                    portENTER_CRITICAL(&_lock);
                    _currentState = INTER_STEP_WAIT;
                    _lastChangeTime = currentTime;
                    portEXIT_CRITICAL(&_lock);
                } else {
                    // 目標未達成，繼續在 A 和 B 頻率之間切換
                    uint32_t nextFreq = ((next_toggle_count % 2) == 1) ? _config.freqB_hz : _config.freqA_hz;
                    ledc_set_freq(_config.speed_mode, _ledcTimer, nextFreq);
                    ESP_LOGI(TAG, "    ↳ Freq set to %d Hz", (int)nextFreq);
                    
                    // 更新計數器和時間
                    portENTER_CRITICAL(&_lock);
                    _pwmToggleCount = next_toggle_count;
                    _lastChangeTime = currentTime;
                    portEXIT_CRITICAL(&_lock);
                }
            }
            break;

        // --- 狀態：步驟之間的延遲等待 ---
        case INTER_STEP_WAIT:
            // 判斷延遲時間是否已到
            if (currentTime - local_lastChangeTime >= _sequence[local_currentStepIndex].post_delay_ms) {
                int next_step_index = local_currentStepIndex + 1;

                // 判斷整個序列是否已結束
                if (next_step_index >= _sequence.size()) {
                    stop(); // 結束
                } else {
                    // *** 關鍵解決方案：在開始下一步前，重新初始化整個硬體 ***
                    // 這樣可以確保每次都產生最乾淨的訊號
                    initialize_hardware();
                    
                    const auto& nextStep = _sequence[next_step_index];
                    ESP_LOGI(TAG, "Step %d: Running %s", next_step_index + 1, nextStep.type == LARGE_CYCLE ? "Large Cycle" : "Small Cycle");
                    
                    // 讓重新初始化的設定(duty=4096)生效，重新開啟訊號
                    ledc_update_duty(_config.speed_mode, _ledcChannel);
                    
                    // 更新內部狀態，為下一個循環做準備
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
            // 處理其他未預期的狀態，滿足編譯器的嚴格檢查
            break;
    }
}

/**
 * @brief 停止序列
 */
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

/**
 * @brief 查詢序列是否已完成
 */
bool ComplexPwmSequencer::isFinished() {
    portENTER_CRITICAL(&_lock);
    bool finished = (_currentState == IDLE);
    portEXIT_CRITICAL(&_lock);
    return finished;
}

/**
 * @brief 獲取系統啟動至今的毫秒數
 */
int64_t ComplexPwmSequencer::_millis() {
    return esp_timer_get_time() / 1000;
}
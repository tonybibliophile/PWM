#ifndef COMPLEX_PWM_SEQUENCER_HPP
#define COMPLEX_PWM_SEQUENCER_HPP

#include "driver/ledc.h"
#include "esp_err.h"
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 定義指令類型
enum StepType {
    LARGE_CYCLE,
    SMALL_CYCLE
};

// 定義單一步驟的結構
struct SequenceStep {
    StepType type;
    uint32_t post_delay_ms;
};

// 定義序列器的設定
struct SequencerConfig {
    ledc_mode_t speed_mode;
    uint32_t    freqA_hz;
    uint32_t    freqB_hz;
    uint32_t    switch_interval_ms;
};

// 序列器類別宣告
class ComplexPwmSequencer {
public:
    // 公開介面
    ComplexPwmSequencer(gpio_num_t pwmPin, ledc_timer_t timer, ledc_channel_t channel, const SequencerConfig& config);
    ~ComplexPwmSequencer();
    void begin(const std::vector<SequenceStep>& sequence);
    void stop();
    void update();
    bool isFinished();

    // 禁止複製
    ComplexPwmSequencer(const ComplexPwmSequencer&) = delete;
    ComplexPwmSequencer& operator=(const ComplexPwmSequencer&) = delete;

private:
    // 私有函式
    void initialize_hardware();
    int64_t _millis();

    // 內部狀態
    enum State { IDLE, RUNNING_PWM, INTER_STEP_WAIT };
    State _currentState;

    // 硬體與設定
    const gpio_num_t _pwmPin;
    const ledc_timer_t _ledcTimer;
    const ledc_channel_t _ledcChannel;
    const SequencerConfig _config;

    // 執行狀態變數
    std::vector<SequenceStep> _sequence;
    int _currentStepIndex;
    int _pwmToggleCount;
    int _pwmToggleTarget;
    int64_t _lastChangeTime;

    // 執行緒安全鎖
    portMUX_TYPE _lock = portMUX_INITIALIZER_UNLOCKED;
};

#endif // COMPLEX_PWM_SEQUENCER_HPP
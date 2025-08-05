#ifndef COMPLEX_PWM_SEQUENCER_HPP
#define COMPLEX_PWM_SEQUENCER_HPP

// -----------------------------------------------------------------------------
// 標頭檔守衛 (Header Guard)
// 作用：防止這個檔案在編譯過程中被重複引用，從而避免重複定義的編譯錯誤。
// 這是所有 .hpp/.h 檔案都應該有的標準寫法。
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// 引入 (include) 外部函式庫
// 作用：引入本檔案需要用到的其他工具或功能的「說明書」。
// -----------------------------------------------------------------------------
#include "driver/ledc.h"      // 引入 ESP32 的 LEDC (PWM) 硬體驅動功能。
#include "esp_err.h"          // 引入 ESP-IDF 的錯誤代碼處理功能 (例如 ESP_ERROR_CHECK)。
#include <vector>             // 引入 C++ 標準函式庫的 `std::vector` (一種更安全、更方便的陣列/清單)。
#include "freertos/FreeRTOS.h"  // 引入 FreeRTOS 作業系統的核心定義。
#include "freertos/task.h"      // 引入 FreeRTOS 的任務相關功能，我們這裡主要是為了 `portMUX_TYPE` 這個鎖的型別。


// -----------------------------------------------------------------------------
// 資料結構定義
// 作用：定義專為本功能設計的自訂資料型別，讓程式碼更清晰易懂。
// -----------------------------------------------------------------------------

/**
 * @brief 定義了指令的「類型」。
 * 這是一個列舉 (enum)，用有意義的名稱來代表不同的整數值。
 */
enum StepType {
    LARGE_CYCLE, // 大循環
    SMALL_CYCLE  // 小循環
};

/**
 * @brief 定義了「單一步驟」的結構 (struct)。
 * 就像一張資料卡片，把單一步驟所需的資訊打包在一起。
 */
struct SequenceStep {
    StepType type;          // 卡片欄位1: 步驟的類型 (來自上面的 StepType)。
    uint32_t post_delay_ms; // 卡片欄位2: 步驟的 PWM 部分完成後，要延遲等待的毫秒數。
};

/**
 * @brief 定義了「序列器整體設定」的結構。
 * 將所有影響 PWM 行為的參數打包在一起，方便統一管理和傳遞。
 */
struct SequencerConfig {
    ledc_mode_t speed_mode;       // PWM 速度模式 (高速/低速)。
    uint32_t    freqA_hz;           // 頻率 A 的赫茲值。
    uint32_t    freqB_hz;           // 頻率 B 的赫茲值。
    uint32_t    switch_interval_ms; // 在 A、B 頻率之間每次切換所需的時間間隔。
};


// -----------------------------------------------------------------------------
// 類別宣告 (Class Declaration)
// 作用：定義 ComplexPwmSequencer 這個類別的完整藍圖。
// -----------------------------------------------------------------------------
class ComplexPwmSequencer {
public: // 「公開區」：定義了外部程式 (如 main.cpp) 可以存取和呼叫的介面 (Interface)。

    /**
     * @brief 建構函式 (Constructor)。
     * @details 建立 ComplexPwmSequencer 物件時呼叫，傳入硬體資源和設定。
     */
    ComplexPwmSequencer(gpio_num_t pwmPin, ledc_timer_t timer, ledc_channel_t channel, const SequencerConfig& config);
    
    /**
     * @brief 解構函式 (Destructor)。
     * @details 物件被銷毀時自動呼叫，用於釋放資源。
     */
    ~ComplexPwmSequencer();

    /**
     * @brief 開始執行一個序列。
     * @param sequence 一個包含所有步驟指令的 `std::vector` 清單。
     */
    void begin(const std::vector<SequenceStep>& sequence);
    
    /**
     * @brief 手動強制停止序列。
     */
    void stop();

    /**
     * @brief 狀態更新函式 (心跳函式)。
     * @details 由主迴圈反覆呼叫，用來推進序列的執行進度。
     */
    void update();

    /**
     * @brief 查詢序列是否已經全部完成。
     * @return true 如果序列已結束 (處於 IDLE 狀態)，否則為 false。
     */
    bool isFinished();

    // --- 安全性設計 ---
    // 透過 `= delete;` 明確禁止物件被複製或賦值，
    // 以防止兩個軟體物件嘗試控制同一組硬體資源而產生衝突。
    ComplexPwmSequencer(const ComplexPwmSequencer&) = delete;
    ComplexPwmSequencer& operator=(const ComplexPwmSequencer&) = delete;

private: // 「私有區」：定義了類別內部的實作細節，外部無法存取。

    // --- 私有輔助函式 ---
    void initialize_hardware(); // 專門用來設定 LEDC 硬體。
    int64_t _millis();          // 專門用來獲取系統的毫秒時間。

    // --- 內部狀態 ---
    // 定義了狀態機的三種狀態
    enum State { IDLE, RUNNING_PWM, INTER_STEP_WAIT };
    State _currentState; // 儲存當前的狀態 (閒置、運行中、等待中)。

    // --- 硬體與設定 (唯讀) ---
    // 這些變數在物件建立時被初始化，之後不再改變。
    const gpio_num_t _pwmPin;
    const ledc_timer_t _ledcTimer;
    const ledc_channel_t _ledcChannel;
    const SequencerConfig _config;

    // --- 執行狀態變數 ---
    // 這些是類別在運行時會不斷改變的內部狀態。
    std::vector<SequenceStep> _sequence; // 儲存當前正在執行的「劇本」。
    int _currentStepIndex;   // 記錄當前執行到劇本的第幾幕。
    int _pwmToggleCount;     // 記錄當前這幕的頻率已經切換了幾次。
    int _pwmToggleTarget;    // 記錄當前這幕總共需要切換幾次。
    int64_t _lastChangeTime; // 記錄上一次狀態改變的時間戳。

    // --- 同步機制 ---
    // 用於保護內部狀態的「安全鎖」，確保在多工環境下的資料安全。
    portMUX_TYPE _lock = portMUX_INITIALIZER_UNLOCKED;
};

#endif // COMPLEX_PWM_SEQUENCER_HPP
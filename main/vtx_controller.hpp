#ifndef VTX_CONTROLLER_HPP
#define VTX_CONTROLLER_HPP


/**
 * @brief 定義了可以傳遞給控制器的「指令」類型。
 * @details 使用 enum (列舉) 可以讓呼叫者使用有意義的名稱（如 SEQ_FULL_SETUP）
 * 而不是難以理解的數字（如 0）。
 */
enum VtxSequenceType {
    SEQ_FULL_SETUP, // 指令：執行一次完整的設定序列
    // 未來可以增加更多指令，例如：
    // SEQ_SET_POWER_LEVEL_1,
    // SEQ_REBOOT_VTX,
};

/**
 * @brief 執行一個指定的VTX控制序列。
 * @details 這是暴露給外部世界的唯一控制函式，它封裝了所有底層的複雜性。
 * * @param seq_type 您想要執行的序列類型（從上面的 VtxSequenceType enum 中選擇一個）。
 * * @note 這是一個「阻塞式 (Blocking)」函式。程式的執行流程會停在
 * 這個函式上，直到整個PWM序列（包含所有延遲）都播放完畢後，才會返回並繼續執行後續程式碼。
 */
void run_vtx_sequence(VtxSequenceType seq_type);

#endif // VTX_CONTROLLER_HPP
#include "timer.h"

timer::timer(uint32_t id, MilliSec sec, bool loop, 
    const TimePoint& start_time, std::array<uint8_t, 5> cur_seq, 
    callback & handler, void * param) 
    : id(id), timeout(sec), loop(loop), 
    t(std::chrono::steady_clock::now() + sec), start_time(start_time),
    task(std::move(handler)), callback_param(std::move(param)) {
        init_slot_seq_wheel(std::move(cur_seq));
    }
void timer::init_slot_seq_wheel(std::array<uint8_t, 5> cur_seq) {
    TimePoint end_time = timeout + std::chrono::steady_clock::now();
    uint64_t gap = std::chrono::duration_cast<MilliSec>(end_time - start_time).count() / TIME_STEP;
    slot_val[0] = HEAD_INDEX(gap);
    for (int i = 0; i < 4; ++i) {
        slot_val[i] = NTH_INDEX(gap, i + 1);
    }
    for (int i = 4; i >= 0; --i) {
        if (slot_val[i] != 0 && slot_val[i] != cur_seq[i]) {
            cur_now = i;
            break;
        }
    }
}
std::pair<uint8_t, uint8_t> timer::next_wheel() {
    if (cur_now == 0) return std::make_pair(cur_now, slot_val[cur_now]);
    uint8_t next_val = 0;
    for (int i = cur_now - 1; i >= 0; ++i) {
        if (slot_val[i] != 0) {
            next_val = i;
            break;
        }
    }
    cur_now = next_val;
    return std::make_pair(cur_now, slot_val[cur_now]);
}
void timer::execute() {
    task(callback_param);
}


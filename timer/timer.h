#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <chrono>
#include <functional>
#include <thread>

#define HEAD_SIZE 8
#define FOLLOW_SIZE 6
#define TIMER_HEAD (1 << HEAD_SIZE)
#define TIMER_FOLLOW (1 << FOLLOW_SIZE)
#define HEAD_MASK (TIMER_HEAD - 1)
#define FOLLOW_MASK (TIMER_FOLLOW - 1)
#define TIME_STEP (50)

#define HEAD_INDEX(now) ((now) & (HEAD_MASK))
#define NTH_INDEX(now, n) (((now) >> (HEAD_SIZE + (n - 1) * FOLLOW_SIZE)) & FOLLOW_MASK)

typedef std::function<void(void *)> callback;
using MilliSec = std::chrono::milliseconds;
using TimePoint = std::chrono::steady_clock::time_point;

class timer{
public:
    timer(uint32_t id, MilliSec sec, bool loop, 
        const TimePoint& start_time, std::array<uint8_t, 5> cur_seq, 
        callback & handler, void * param);
    ~timer();
    bool is_loop() const {
        return loop;
    }
    std::pair<uint8_t, uint8_t> get_wheel_slot() {
        return std::make_pair(cur_now, slot_val[cur_now]);
    }
    std::pair<uint8_t, uint8_t> next_wheel();
    void execute(); // 执行回调函数
private:
    std::chrono::steady_clock::time_point t;
    uint32_t id;
    const TimePoint & start_time; // wheel start time
    bool loop;       // 是否循环函数
    callback task;      // 任务执行
    uint8_t cur_now;    // 当前所在环数
    MilliSec timeout;
    std::array<uint8_t, 5> slot_val; //记录在五个轮中所处位置
    void init_slot_seq_wheel(std::array<uint8_t, 5> cur_seq);
    void * callback_param; // call_back参数
};

#endif

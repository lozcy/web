#ifndef TIMER_WHEEL_H
#define TIMER_WHEEL_H

#include <time.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "../lock/locker.h"
#include "timer.h"

typedef shared_ptr<timer> timerptr;
/* single wheel */
template<int SLOT>
class single_wheel {
public:
    single_wheel();
    
private:
    void step();
    std::array<timerptr, SLOT> list;

};


class timer_wheel {
public:
    
    /* don't copy move */
    timer_wheel(const timer_wheel &) = delete;
    timer_wheel & operator = (const timer_wheel &) = delete;
    timer_wheel(timer_wheel & ) = delete;
    timer_wheel & operator = (timer_wheel &&) = delete;

    void destory();

private:
    locker              m_timerwheellocker;
    std::atomic_bool    alive;
    TimePoint           start_time;
    std::unique_ptr<single_wheel<HEAD_SIZE>>                    head_wheel;
    std::array<std::unique_ptr<single_wheel<FOLLOW_SIZE>>, 4>   follow_wheel;
    std::unordered_map<uint32_t, std::shared_ptr<TwTimer> >     task_set;

};

#endif // TIMER_WHEEL_H
#include "timer_wheel.h"

void timer_wheel::destory() {
    alive = 0;
    task_set.clear();
}
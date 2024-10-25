#include <bits/stdc++.h>
#include "../lock/locker.h"
#include "../threadpool/threadpool.h"
// #include "timer.h
using namespace std;
typedef function<void(int)> callback;
class timer {
public:
    timer(int param, callback task): param(param), task(move(task)) 
    {}
    void execute() {
        this->task(param);
    }
private:
    int param;
    callback task;
};
struct node0{
    int val;
};
struct node1{
    int val;
};
void print(int t) {
    cout << t << endl;
}
int main() {
    // as s;
    threadpool<timer> * pool = nullptr;
    try{
        pool = new threadpool<timer>;
    }
    catch (...) {
        printf("creat thread pool is failed\n");
        return 1;
    }
    timer * wheel[10];
    for (int i = 0; i < 10; i += 2) {
        // node0 * a = new node0;
        // a->val = i;
        callback task = print;
        wheel[i] = new timer(i, task);
        pool->append(wheel[i]);
    }
    for (int i = 1; i < 10; i += 2) {
        // node1 * a = new node1;
        // a->val = i;
        callback task = print;
        wheel[i] = new timer(i, task);
        pool->append(wheel[i]);
    }
    cout << "tst" << endl;
    while (true) {

    }
    return 0;
}
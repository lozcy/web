#pragma once
#include <thread>
#include <chrono>
#include <memory>
#include <array>
#include <list>
#include <functional>
#include <mutex>
#include <algorithm>
#include <unordered_map>

#ifndef USE_BOOST_FUTURE
#include <future>
#else
//#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_VERSION 4
#include "boost/thread/future.hpp"
#endif

#include "asyncthreadpool.hpp"
#include "time_holder.hpp"

namespace monitor 
{
    class AsyncThreadPool;
    // 第1个轮占的位数
    #define TVR_BITS 8
    // 第1个轮的长度
    #define TVR_SIZE (1 << TVR_BITS)
    // 第n个轮占的位数
    #define TVN_BITS 6
    // 第n个轮的长度
    #define TVN_SIZE (1 << TVN_BITS)
    // 掩码 取模或整除用
    #define TVR_MASK (TVR_SIZE - 1)
    #define TVN_MASK (TVN_SIZE - 1)
    // 第1个圈的当前指针位置
    #define FIRST_INDEX(v) ((v) & TVR_MASK)
    // 后面第n个圈的当前指针位置
    #define NTH_INDEX(v, n) (((v) >> (TVR_BITS + (n - 1) * TVN_BITS)) & TVN_MASK)
    // 最小刻度单位的指数
    #define TIME_UNIT       50

    class TwTimer;

    using MilliSec  = std::chrono::milliseconds;
    using TaskRB    = std::function<bool(void)>;
    using TmPoint   = std::chrono::steady_clock::time_point;
    using ListShptr = std::list<std::shared_ptr<TwTimer>>;
    using DealFunc  = std::function<void(future<bool>&&)>;
    
    const MilliSec  unit(TIME_UNIT);       // 定时器槽之间时间间隔，即最小刻度，单位：毫秒

#ifndef USE_BOOST_FUTURE
	using namespace std;
#else
	using namespace boost;
#endif
    // 任务类，定时任务
    // 只移不可复制类型
    // 复制主要考虑到
    class TwTimer {
    public:
        TwTimer(const MilliSec& msec, TaskRB&& func, bool _loop, 
                const TmPoint& epoch, std::array<uint8_t, 5>&& cur_slots) : ahp(AsyncThreadPool::GetInstance()),
                                            callback(std::move(func)),
                                            t(std::chrono::steady_clock::now() + msec),
                                            timeout(msec),
                                            loop_flag(_loop),
                                            cur_wheel(0),
                                            start_time(epoch)
        {
            init_slot_seq_cur_wheel(std::move(cur_slots));
        }
        // 任务执行函数
        void execute()
        {
            future_bool = ahp.append(callback);
        }
        future<bool>& get() { return future_bool;}
        // 超时标记点往后挪一个timeout
        void do_loop(std::array<uint8_t, 5>&& cur_slots) 
        { 
            init_slot_seq_cur_wheel(std::move(cur_slots)); 
        }
        // 返回此任务是否是循环任务
        bool is_loop()  const {return loop_flag;}
        // 获取此任务的到期时刻
        std::pair<uint8_t, uint8_t> get_wheel_slot() const
        {
            return std::make_pair(cur_wheel, slot_seq[cur_wheel]);
        }
        // 返回tick后需要跳到的轮子序号和对应轮子的槽号
        std::pair<uint8_t, uint8_t> tick() 
        {
            if (cur_wheel == 0)
                return std::make_pair(0, slot_seq[0]);
            uint8_t next_wheel = 0;
            for (int i = cur_wheel - 1; i >= 0; i--)
            {
                if (slot_seq[i] != 0) {
                    next_wheel = i;
                    break;
                } //如果一个if都没进去，说明cur_wheel在最低环，和初值0一致，并且是整数倍，应该是马上要执行
            }      
            cur_wheel = next_wheel;
            return std::make_pair(next_wheel, slot_seq[next_wheel]);
        }
        uint32_t  get_loop_time() const {return timeout.count();}
        // 只移不可复制类型
        TwTimer(const TwTimer& orgin) = delete;
        TwTimer& operator=(const TwTimer & orgin) = delete;
        TwTimer(TwTimer&&) = default;
        TwTimer& operator=(TwTimer&&) = default;
    private:
        void init_slot_seq_cur_wheel(const std::array<uint8_t, 5>& cur_slots)
        {
            TmPoint end_epoch = timeout + std::chrono::steady_clock::now();
            uint32_t gap = std::chrono::duration_cast<MilliSec>(end_epoch - start_time).count() / TIME_UNIT ;
            slot_seq[0] = FIRST_INDEX(gap);
            for (int i = 1; i < 5 ; i++)
                slot_seq[i] = NTH_INDEX(gap, i);
            for (int i = 4; i >= 0; i--)
            {
                if (slot_seq[i] != 0 && cur_slots[i] != slot_seq[i]) {
                    cur_wheel = i;
                    break;
                } //如果一个if都没进去，说明cur_wheel在最低环，和初值0一致，并且是整数倍，应该是马上要执行
            } 
        }
        AsyncThreadPool&        ahp;        //异步线程池句柄
        TaskRB                  callback;   //超时后的处理函数
        TmPoint                 t;          //任务到期应该执行时候的时刻点
        const MilliSec          timeout;    //超时时长
        const bool              loop_flag;  //是否循环任务
        std::array<uint8_t, 5>  slot_seq;   //记录此任务在5个环中的位置值
        uint8_t                 cur_wheel;  //记录当前处于的环数
        const TmPoint&          start_time; //总时间轮开始时间
        future<bool>            future_bool;
    };

    
    class Task_Result
    {
    public:
        explicit Task_Result(std::shared_ptr<TwTimer> _tw) : is_loop(_tw->is_loop())
        {
            if (is_loop)     //如果是循环任务，则保留弱指针，以防循环任务后续被删除的时候，这里还有备份
                w_tw = _tw;
            else             //如果是非循环任务，则保留shared指针，确保任务被删除后结果仍旧可以保留
                s_tw = _tw;
        }

        bool get_result(uint32_t miiliseconds) const
        { 
            auto span = chrono::milliseconds(miiliseconds);
            std::shared_ptr<TwTimer> sp;
            if (is_loop) {
                sp = w_tw.lock();
            } else {
                sp = s_tw;
            }
            if (sp) {
                future<bool>& ret = sp->get();
                if (ret.valid())
                {
                    if (ret.wait_for(span) != future_status::timeout) {
                        try {
                            return ret.get();
                        }
                        catch (std::exception&) {
                            // printf("[exception caught]");
                            return false;
                        }
                    }
                }
            }
            return false;
        }

        // 禁止复制，赋值，移动，移动赋值构造
        // 由于C++11不支持lambda移动捕获（C++14才支持），为了模拟移动捕获，只能开启拷贝构造函数可用
        // 但此类的意图并不是允许拷贝的
		Task_Result(const Task_Result &) = default;
		Task_Result& operator=(const Task_Result &) = default;
		Task_Result(Task_Result&&) = default;
		Task_Result& operator=(Task_Result&&) = default;
    private:
        std::weak_ptr<TwTimer>      w_tw;
        std::shared_ptr<TwTimer>    s_tw;        
        bool                        is_loop;
    };
    

    template<int SLOT>
    class SingleWheel
    {
    public:
        SingleWheel() : cur_slot(0) {}
        // 有新任务加入时的接口
        void add(uint8_t insert_slot_seq, std::shared_ptr<TwTimer>&& sp_tw)
        {
            if (insert_slot_seq >= SLOT)
                return;
            task_list[insert_slot_seq].emplace_front(std::move(sp_tw));
        }
        // 任务弹出
        // @return first：弹出的任务列表
        //         second：当前轮是否触发下一个轮的tick，即是否进位
        std::pair<ListShptr, bool> tick()
        {
            auto&& list = cascade(cur_slot);
            cur_slot = cur_slot + 1 == SLOT ?  0 : cur_slot + 1;
            return std::make_pair(list, cur_slot == 0);
        }
        std::pair<ListShptr, bool> degrade()
        {
            cur_slot = cur_slot + 1 == SLOT ?  0 : cur_slot + 1;
            auto&& list = cascade(cur_slot);
            return std::make_pair(list, cur_slot == 0);            
        }

        // @return 返回此轮当前slot槽号，从0开始
        uint8_t curslot() const { return cur_slot;}
    private:
        // @param remove_slot_seq 需要弹出任务的槽
        // @return 弹出的槽中任务列表
        ListShptr cascade(uint8_t remove_slot_seq)
        {
            ListShptr tmp;
            if (remove_slot_seq >= SLOT)
                return tmp;
            task_list[remove_slot_seq].swap(tmp);
            return tmp;
        }    
        uint8_t                     cur_slot;
        std::array<ListShptr, SLOT> task_list;
    };

    //实现5级时间轮 范围为0~ (2^8 * 2^6 * 2^6 * 2^6 *2^6)=2^32
    class TimeWheel {
    public:
        static TimeWheel& GetInstance()
        {
            static TimeWheel t;
            return t;
        }

        // 禁止复制，赋值，移动，移动赋值构造
		TimeWheel(const TimeWheel &) = delete;
		TimeWheel& operator=(const TimeWheel &) = delete;
		TimeWheel(TimeWheel&&) = delete;
		TimeWheel& operator=(TimeWheel&&) = delete;

        void destory() {
            alive = false;
            task_set.clear();
        }

        ~TimeWheel() {
            alive = false;
        }
        // 根据定时值创建定时器，并插入槽中
        Task_Result add_loop_timer(const std::string& name, uint32_t milliseconds_timeout, std::function<bool()>&& func)
        {
            return add_timer(name, milliseconds_timeout, std::move(func), true);
        }

        Task_Result add_once_timer(const std::string& name, uint32_t milliseconds_timeout, std::function<bool()>&& func)
        {
            return add_timer(name, milliseconds_timeout, std::move(func), false);
        }
        
        void del_timer(const std::string& name)
        {
            std::lock_guard<std::recursive_mutex>  g(mtx);
            task_set.erase(name);
        }
        
    private: 
        TimeWheel() :   ahp(AsyncThreadPool::GetInstance()), 
                        epoch(std::chrono::steady_clock::now()),
                        alive(true),
                        first_tw(new SingleWheel<TVR_SIZE>()),
                        last_tws()
        {
            std::thread th([this](){
                time_holder holder;
                uint32_t times = 1;    
                while (alive) {
                    if (times == 0) {  
                         // uint32次后再重新定位原点，频繁定位起点会造成累计误差
                         // 重定位理论上越少越好，这里不用uint64是因为uint64加法
                         // 32位板子上指令较多，如果是64位cpu，可以采用uint64
                        holder.reset();
                        times = 1;
                    }
                    tick();
                    holder.hold(unit * times++);
                }
            });
            th.detach();

            for (auto& item : last_tws)
            {
                item = std::unique_ptr<SingleWheel<TVN_SIZE>>(new SingleWheel<TVN_SIZE>());
            }
        }

        Task_Result add_timer(const std::string& name, uint32_t milliseconds_timeout, std::function<bool()>&& func, bool is_loop)
        {
            std::lock_guard<std::recursive_mutex>  g(mtx);
            auto tw = std::make_shared<TwTimer>(MilliSec(milliseconds_timeout), std::move(func), is_loop, epoch, cur_slots());
            if (is_loop)
                task_set.emplace(name, tw);
            Task_Result ret = Task_Result(tw);
            load_timer(std::move(tw));
            return ret;
        }

        // 此接口给循环任务使用，循环任务不必进行重新构建任务，只需要修改重新算slot和wheel即可
        void add_timer(std::shared_ptr<TwTimer>&& tw)
        {
            tw->do_loop(cur_slots());
            // printf( "tw:%d, first_tw->curslot():%d, last_tws[i]->curslot():%d\n", tw->get_loop_time(), first_tw->curslot(), last_tws[0]->curslot());
            load_timer(std::move(tw));
        }
        // 此接口为将任务装在入对应时间轮
        void load_timer(std::shared_ptr<TwTimer>&& p_tw)
        {
            // first: wheel数，从0开始，second，slot数，从0开始
            std::pair<uint8_t, uint8_t>&& wheel_slot = p_tw->get_wheel_slot(); 
            if (wheel_slot.first > 4)   //大于4理论上不可能出现，退出以防下面array越界
                return;
            // printf( "p_tw:%d, wheel_slot.first:%d, wheel_slot.second:%d\n", p_tw->get_loop_time(), wheel_slot.first, wheel_slot.second);
            if (wheel_slot.first == 0)
                first_tw->add(wheel_slot.second, std::move(p_tw));
            else
                last_tws[wheel_slot.first - 1]->add(wheel_slot.second, std::move(p_tw));
        }
        std::array<uint8_t, 5> cur_slots() const
        {
            std::array<uint8_t, 5> cur_slot;
            cur_slot[0] = first_tw->curslot();
            for (int i = 0; i < 4; i++)
            {
                cur_slot[i + 1] = last_tws[i]->curslot();
            }
            return cur_slot;
        }
        void tick()
        {
            std::lock_guard<std::recursive_mutex>  g(mtx);
            auto&& result = first_tw->tick();
            auto& task_list = result.first;
            for (auto& p_tw : task_list)
            {
                // 这里不会出现判断了unique后，引用次数变化的情况
                // 外层的add_timer和del_timer都被阻塞住
                bool is_loop = p_tw->is_loop();
                if (is_loop && p_tw.unique())  
                    continue;
                p_tw->execute();
                if (is_loop)
                    add_timer(std::move(p_tw));
            }
            if (result.second) // 如果低级轮子进位了
            {
                // printf("jinweile!\n");
                for (int i = 0; i < 4; i++)
                {
                    auto&& res = last_tws[i]->degrade();
                    ListShptr& tl = res.first;
                    for (auto & tw : tl)
                    {
                        std::pair<uint8_t, uint8_t> tik = tw->tick();
                        // printf("进位到哪个轮子：%d, 第几个槽：%d", tik.first, tik.second);
                        if (tik.first > 4)   //大于4理论上不可能出现，退出以防下面array越界
                            continue; 
                        if (tik.first == 0)
                            first_tw->add(tik.second, std::move(tw));
                        else
                            last_tws[tik.first - 1]->add(tik.second, std::move(tw));
                    }
                    if (!res.second)
                        break;
                }
            }
        }
        #if 0
        // 将计时器重新分配轮上的位置
        void realloc(ListShptr&& tl)
        {
            for (auto & tw : tl)
            {
                std::pair<uint8_t, uint8_t> tik = tw->tick();
                if (tik.first > 4)   //大于4理论上不可能出现，退出以防下面array越界
                    continue; 
                if (tik.first == 0)
                    first_tw->add(tik.second, std::move(tw));
                else
                    last_tws[tik.first - 1]->add(tik.second, std::move(tw));
            }
        }
        #endif
        AsyncThreadPool&                        ahp;        
        const TmPoint                           epoch;      // 类开始时候的时间标记点
        std::atomic_bool                        alive;      // tick线程结束标志位
        std::unique_ptr<SingleWheel<TVR_SIZE>>  first_tw;   // 第一个轮 
        std::array<std::unique_ptr<SingleWheel<TVN_SIZE>>, 4>    last_tws;   // 后四个轮 
        // TwTimer的二次计数，使用TwTimer之前，判断unique，如果为真，表示这里没有了，要删除。
        // 这里不建议适用map，unordered_map查找效率比map高好几个数量级
        std::unordered_map<std::string, std::shared_ptr<TwTimer>>      task_set;
        std::recursive_mutex                               mtx;
    };
}

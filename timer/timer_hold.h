#pragma once
#include <thread>
#include <chrono>

namespace monitor 
{
	class time_holder
	{
	public:
		time_holder() : t(std::chrono::steady_clock::now()) {}
        // 自类创建开始，需要保持多久时间，如果不足保持时间，则阻塞等待到期，若已经超时，不阻塞直接退出
		void hold(std::chrono::milliseconds msec) {
			std::this_thread::sleep_until(msec + t);
		}
		// 重置time_holder计时时间
		void reset(){
			t = std::chrono::steady_clock::now();
		}
	private:
		std::chrono::steady_clock::time_point t;
	};
}

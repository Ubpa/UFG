#pragma once

#include <mutex>
#include <thread>
#include <deque>
#include <functional>

class ThreadPool {
public:
	ThreadPool() = default;
	~ThreadPool() {
		Shutdown();
	}

	void Init(size_t n) {
		for (size_t i = 0; i < n; i++) {
			workers.emplace_back([this]() {
				while (!shutdown) {
					std::function<void()> work;
					{ // get work
						std::unique_lock<std::mutex> lk(m);
						if (works.empty())
							cv.wait(lk);
						if (works.empty())
							break;
						work = std::move(works.back());
						works.pop_back();
					}
					work();
				}
			});
		}
	}

	void Shutdown() {
		shutdown = true;
		cv.notify_all();
		for (auto& worker : workers)
			worker.join();
		workers.clear();
		shutdown = false;
	}

	void Summit(std::function<void()> work) {
		std::unique_lock<std::mutex> lk(m);
		works.push_back(std::move(work));
		cv.notify_one();
	}

	ThreadPool& operator=(ThreadPool&&) noexcept = delete;
	ThreadPool(ThreadPool&&) noexcept = delete;
private:
	std::condition_variable cv;
	std::mutex m;
	std::deque<std::function<void()>> works;
	std::vector<std::thread> workers;
	bool shutdown{ false };
};

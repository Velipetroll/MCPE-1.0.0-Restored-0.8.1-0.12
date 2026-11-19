#pragma once
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

enum class WorkerRole
{
	Streaming,
	Disk,
	MainThread,
	NumWorkerRoles,
};

#ifndef UNSTABLE_THREADING
class BackgroundWorker
{
public:

	static bool NOP()
	{
		return true;
	}

	template<typename F>
	void queue(F&& task)
	{
		task();
	}

	void sync()
	{

	}

	void flush()
	{

	}

	template<typename F, typename F2>
	void queue(F&& task, F2&& task2, int priority)
	{
		task(); task2();
	}


};

class WorkerPool
{
public:

	static std::vector<BackgroundWorker*> getWorkersFor(WorkerRole role)
	{
		return std::vector<BackgroundWorker*>();
	}

	static BackgroundWorker& getFor(WorkerRole role)
	{
		static BackgroundWorker worker;//wont matter as this is single threaded now
		return worker;

	}

	static void setMainThreadPumpActive(bool) {}
	static bool isMainThreadPumpActive() { return false; }
	static void enqueueMainThreadCallback(std::function<void()> cb) { if (cb) cb(); }
	static void pumpMainThreadCallbacks() {}
	static bool hasPendingMainThreadCallbacks() { return false; }
	static void drainWorkerPool(WorkerRole) {}

	static std::unique_ptr<WorkerPool> mInstance;

};
#else

class BackgroundWorker;

class WorkerPool
{
public:
	static std::vector<std::vector<BackgroundWorker*>> workers;

	static std::vector<BackgroundWorker*>& getWorkersFor(WorkerRole role)
	{
		return workers[(unsigned int)role];
	}

	static BackgroundWorker& getFor(WorkerRole role);

	static void setMainThreadPumpActive(bool active)
	{
		_pumpActiveFlag().store(active, std::memory_order_release);
	}

	static bool isMainThreadPumpActive()
	{
		return _pumpActiveFlag().load(std::memory_order_acquire);
	}

	static void enqueueMainThreadCallback(std::function<void()> cb)
	{
		if (!cb) return;
		std::lock_guard<std::mutex> lock(_callbackMutex());
		_callbackQueue().push_back(std::move(cb));
	}

	static bool hasPendingMainThreadCallbacks()
	{
		std::lock_guard<std::mutex> lock(_callbackMutex());
		return !_callbackQueue().empty();
	}

	static void pumpMainThreadCallbacks()
	{
		for (;;) {
			std::vector<std::function<void()>> batch;
			{
				std::lock_guard<std::mutex> lock(_callbackMutex());
				if (_callbackQueue().empty()) {
					break;
				}
				batch.swap(_callbackQueue());
			}
			for (auto& cb : batch) {
				if (cb) {
					cb();
				}
			}
		}
	}

	static void drainWorkerPool(WorkerRole role);

	static std::unique_ptr<WorkerPool> mInstance;

private:
	static std::atomic<bool>& _pumpActiveFlag()
	{
		static std::atomic<bool> flag{false};
		return flag;
	}

	static std::mutex& _callbackMutex()
	{
		static std::mutex m;
		return m;
	}

	static std::vector<std::function<void()>>& _callbackQueue()
	{
		static std::vector<std::function<void()>> q;
		return q;
	}
};

class BackgroundWorker
{
public:
	std::mutex tmutex;
	WorkerRole role;
	std::thread t;
	std::condition_variable vv;
	std::condition_variable taskFinished;
	bool running = true;
	bool busy = false;
	static void NOP()
	{
		//return true;
	}

	void sync()
	{
		std::unique_lock lock(tmutex);
		taskFinished.wait(lock, [this] {
			return tasks.empty() && !busy;
			});
	}

	void flush()
	{
		sync();
	}

	void workerThread()
	{
		while (true)
		{
			Task t;

			{
				std::unique_lock lock(tmutex);

				vv.wait(lock, [this] {
					return !tasks.empty() || !running;
					});

				if (!running && tasks.empty())
					return;

				t = std::move(tasks.back());
				tasks.pop_back();
				busy = true;
			}

			bool completed = t.t1();
			if (completed) {
				if (WorkerPool::isMainThreadPumpActive()) {
					WorkerPool::enqueueMainThreadCallback(std::move(t.t2));
				} else {
					t.t2();
				}
			} else {
				// Reschedule deferred work (e.g. postProcess waiting for neighbor lock) instead of dropping it
				bool stillRunning = true;
				{
					std::lock_guard lock(tmutex);
					stillRunning = running;
					if (stillRunning) {
						t.priority -= 1;
						tasks.emplace_back(std::move(t));
						std::sort(tasks.begin(), tasks.end(),
							[](const Task& a, const Task& b) {
								return a.priority > b.priority;
							});
					}
				}
				if (stillRunning) {
					vv.notify_one();
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				} else {
					if (WorkerPool::isMainThreadPumpActive()) {
						WorkerPool::enqueueMainThreadCallback(std::move(t.t2));
					} else {
						t.t2();
					}
				}
			}

			{
				std::lock_guard lock(tmutex);
				busy = false;
			}
			taskFinished.notify_all();
		}
	}

	BackgroundWorker() : t(&BackgroundWorker::workerThread, this)
	{

	}

	~BackgroundWorker() 
	{
		{//need to destroy this lock before the worker is asked to join so it can read its stopped
			std::lock_guard lock(tmutex);
			running = false;
		}

		vv.notify_one();
		t.join();
	}

	//template<typename F, typename F2>
	void queue(std::function<bool()> task, std::function<void()> task2 = NOP, int priority = 0)
	{
		std::lock_guard lock(tmutex);
		tasks.emplace_back(task, task2, priority);
		std::sort(tasks.begin(), tasks.end(),
			[](const Task& a, const Task& b) {
				return a.priority > b.priority;
			});
			vv.notify_one();//notify we have a new task
		//task(); task2();
	}

	int numtasks()
	{
		std::lock_guard lock(tmutex);
		return tasks.size();
	}

	struct Task
	{
		Task() {};
		Task(std::function<bool()>& f, std::function<void()>& f2, int pr)
		{
			t1 = f;
			t2 = f2;
			priority = pr;
		}
		std::function<bool()> t1;
		std::function<void()> t2;
		int priority;
	};

	std::vector<Task> tasks;
};

inline BackgroundWorker& WorkerPool::getFor(WorkerRole role)
{
	//return worker with least tasks
	int min = INT_MAX;
	int minO = -1;
	for (int i = 0; i < workers.at((unsigned int)role).size(); i++)
	{
		int todoTasks = workers.at((unsigned int)role).at(i)->numtasks();
		if (todoTasks < min)
		{
			min = todoTasks;
			minO = i;
		}
	}
	if (minO == -1)
		assert(0);
	else
		return *workers.at((unsigned int)role).at(minO);
}

inline void WorkerPool::drainWorkerPool(WorkerRole role)
{
	for (;;) {
		for (auto* w : getWorkersFor(role)) {
			w->sync();
		}
		if (isMainThreadPumpActive()) {
			pumpMainThreadCallbacks();
		}
		bool anyPending = false;
		for (auto* w : getWorkersFor(role)) {
			if (w->numtasks() > 0) {
				anyPending = true;
				break;
			}
		}
		if (!anyPending && (!isMainThreadPumpActive() || !hasPendingMainThreadCallbacks())) {
			break;
		}
	}
}

#endif
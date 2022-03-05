#include <iostream>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <queue>
#include <mutex>
#include <thread>

template<typename T>
class ThreadSafeQueue
{
	mutable std::mutex mMutex;
	std::queue<T> mQueue;
	std::condition_variable mDataCond;

public:
	ThreadSafeQueue(const std::queue<T> &mQueue)
		: mQueue{mQueue}
	{}

	ThreadSafeQueue()
		: mQueue{}
	{}

	void push(T newValue)
	{
		std::unique_lock<std::mutex> lk{mMutex};
		mQueue.push(newValue);
		mDataCond.notify_one();
	}

	void waitAndPop(T &value)
	{
		std::unique_lock<std::mutex> lk{mMutex};
		mDataCond.wait(lk, [this]()
		{
			return !mQueue.empty();
		});
		value = std::move(mQueue.front());
		mQueue.pop();
	}

	std::shared_ptr<T> waitAndPop()
	{
		std::unique_lock<std::mutex> lk{mMutex};
		mDataCond.wait(lk, [this]()
		{
			return !mQueue.empty();
		});

		auto res{std::make_shared<T>(std::move(mQueue.front()))};
		mQueue.pop();
		return res;
	}

	bool tryPop(T &value)
	{
		std::unique_lock<std::mutex> lk{mMutex};
		if (mQueue.empty()) {
			return false;
		}
		value = std::move(mQueue.front());
		mQueue.pop();
		return true;
	}

	std::shared_ptr<T> tryPop()
	{
		std::unique_lock<std::mutex> lk{mMutex};
		if (mQueue.empty()) {
			return nullptr;
		}
		auto res{std::make_shared<T>(std::move(mQueue.front()))};
		mQueue.pop();
		return res;
	}

	bool empty() const
	{
		std::unique_lock<std::mutex> lk{mMutex};
		return mQueue.empty();
	}
};

class ThreadPool
{
public:
	ThreadPool()
		:
		mDone{false}
	{
		const auto cThreadCount{std::thread::hardware_concurrency()};
		try {
			for (unsigned i = 0; i != cThreadCount; ++i) {
				mThreads.push_back(std::thread(&ThreadPool::workerThread, this));
			}
		}
		catch (...) {
			mDone.store(true);
			throw;
		}
	}

	template<typename FunctionType>
	void submit(FunctionType f)
	{
		mWorkQueue.push(std::function<void()>(f));
	}

	~ThreadPool()
	{
		mDone.store(true);
		for (auto &t: mThreads) {
			if (t.joinable()) {
				t.join();
			}
		}
	}

private:
	void workerThread()
	{
		while (!mDone) {
			std::function<void()> task;
			if (mWorkQueue.tryPop(task)) {
				task();
			}
			else {
				std::this_thread::yield();
			}
		}
	}

	std::atomic_bool mDone;
	ThreadSafeQueue<std::function<void()>> mWorkQueue;
	std::vector<std::thread> mThreads;
};

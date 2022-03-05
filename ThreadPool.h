#include <iostream>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <queue>
#include <mutex>
#include <thread>
#include <future>

class FunctionWrapper
{
	struct ImplBase
	{
		virtual void call() = 0;
		virtual ~ImplBase()
		{}
	};
	std::unique_ptr<ImplBase> mImpl;

	template<typename F>
	struct ImplType: ImplBase
	{
		F mFunc;
		ImplType(F &&func)
			: mFunc{std::move(func)}
		{}
		void call()
		{ mFunc(); }
	};

public:
	template<typename F>
	FunctionWrapper(F &&func)
		: mImpl{new ImplType<F>(std::move(func))}
	{}

	void operator()()
	{ mImpl->call(); }
	FunctionWrapper() = default;
	FunctionWrapper(FunctionWrapper &&rhs)
		: mImpl{std::move(rhs.mImpl)}
	{}

	FunctionWrapper &operator=(FunctionWrapper &&rhs)
	{
		mImpl = std::move(rhs.mImpl);
		return *this;
	}

	FunctionWrapper(const FunctionWrapper &) = delete;
	FunctionWrapper(FunctionWrapper &) = delete;
	FunctionWrapper &operator=(const FunctionWrapper &) = delete;
	~FunctionWrapper()
	{}
};

class ThreadPool
{
public:
	ThreadPool(unsigned numThreads = std::thread::hardware_concurrency(), bool waitForAllTasksToFinish = false)
		:
		mDone{false},
		mWaitForAllTasksToFinish{waitForAllTasksToFinish}
	{
		mThreads.reserve(numThreads);
		try {
			for (unsigned i = 0; i != numThreads; ++i) {
				mThreads.push_back(std::thread(&ThreadPool::workerThread, this));
			}
		}
		catch (...) {
			std::unique_lock lk{mMutex};
			mDone = true;
			mWaitForTasks.notify_all();
			throw;
		}
	}

	template<typename FunctionType>
	std::future<typename std::result_of<FunctionType()>::type> submit(FunctionType f)
	{
		typedef typename std::result_of<FunctionType()>::type ResultType;
		std::packaged_task<ResultType()> task{std::move(f)};
		std::future<ResultType> res{task.get_future()};
		std::unique_lock lk{mMutex};
		mWorkQueue.push(std::move(task));
		mWaitForTasks.notify_one();
		return res;
	}

	~ThreadPool()
	{
		std::unique_lock lk{mMutex};
		mDone = true;
		lk.unlock();

		mWaitForTasks.notify_one();
		for (auto &t: mThreads) {
			if (t.joinable()) {
				t.join();
			}
		}
	}
private:

	bool tryPop(FunctionWrapper &value)
	{
		//std::unique_lock<std::mutex> lk{mMutex};
		if (mWorkQueue.empty()) {
			return false;
		}
		value = std::move(mWorkQueue.front());
		mWorkQueue.pop();
		return true;
	}

	void workerThread()
	{
		unsigned yieldCounter;
		while (true) {
			FunctionWrapper task;
			std::unique_lock lk{mMutex};
			if (tryPop(task)) {
				lk.unlock();
				task();
				lk.lock();
			}
			else {
				if (yieldCounter >= cMaxYielding) {
					mWaitForTasks.wait(lk, [this]() -> bool
					{
						return !mWorkQueue.empty() || mDone;
					});
					if (mDone && (mWorkQueue.empty() || !mWaitForAllTasksToFinish)) {
						break;
					}
				}
				else {
					++yieldCounter;
					std::this_thread::yield();
				}
			}
		}
		mWaitForTasks.notify_all();
	}

	bool mDone;
	bool mWaitForAllTasksToFinish;
	std::queue<FunctionWrapper> mWorkQueue;
	std::vector<std::thread> mThreads;
	mutable std::mutex mMutex;
	std::condition_variable mWaitForTasks;
	static constexpr auto cMaxYielding{500};
};

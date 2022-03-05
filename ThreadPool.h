#include <iostream>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <queue>
#include <mutex>
#include <thread>
#include <future>

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
		mQueue.push(std::move(newValue));
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
	ThreadPool(unsigned numThreads = std::thread::hardware_concurrency())
		:
		mDone{false}
	{
		const auto cThreadCount{numThreads};
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
	std::future<typename std::result_of<FunctionType()>::type> submit(FunctionType f)
	{
		typedef typename std::result_of<FunctionType()>::type ResultType;
		std::packaged_task<ResultType()> task{std::move(f)};
		std::future<ResultType> res{task.get_future()};
		mWorkQueue.push(std::move(task));
		return res;
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
			FunctionWrapper task;
			if (mWorkQueue.tryPop(task)) {
				task();
			}
			else {
				std::this_thread::yield();
			}
		}
	}

	std::atomic_bool mDone;
	ThreadSafeQueue<FunctionWrapper> mWorkQueue;
	std::vector<std::thread> mThreads;
};

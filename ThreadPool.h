#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

class FunctionWrapper
{
  struct ImplBase
  {
    virtual void call() = 0;
    virtual ~ImplBase() = default;
  };
  std::unique_ptr<ImplBase> mImpl;

  template<typename F>
  struct ImplType : ImplBase
  {
    F mFunc;
    explicit ImplType(F&& func)
      : mFunc{ std::move(func) }
    {}
    void call() override { mFunc(); }
  };

public:
  template<typename F>
  FunctionWrapper(F&& func)
    : mImpl{ new ImplType<F>(std::forward<F>(func)) }
  {}

  void operator()() { mImpl->call(); }
  FunctionWrapper() = default;
  FunctionWrapper(FunctionWrapper&& rhs) noexcept
    : mImpl{ std::move(rhs.mImpl) }
  {}

  FunctionWrapper& operator=(FunctionWrapper&& rhs) noexcept
  {
    mImpl = std::move(rhs.mImpl);
    return *this;
  }

  FunctionWrapper(const FunctionWrapper&) = delete;
  FunctionWrapper(FunctionWrapper&) = delete;
  FunctionWrapper& operator=(const FunctionWrapper&) = delete;
  ~FunctionWrapper() = default;
};

/**
 * This thread pool starts by working in "fast mode", if a thread spends too
 * much time without getting work it will sleep on a condition variable and
 * reawaken to "fast mode" again when it gets more work
 */
class ThreadPool
{
public:
  /**
   *
   * @param numThreads Number of threads in the thread pool. By default it takes
   * a hint of from the implementation, if the value isn't well defined or
   * computable, it defaults to 2.
   * @param waitForAllTasksToFinish The constructor will wait until all the work
   * on the queue has been processed before it shuts down
   */
  explicit ThreadPool(unsigned numThreads = std::thread::hardware_concurrency(),
                      bool waitForAllTasksToFinish = false)
    : mDone{ false }
    , mWaitForAllTasksToFinish{ waitForAllTasksToFinish }
  {
    if (numThreads == 0) {
      numThreads = 2;
    }

    mThreads.reserve(numThreads);
    try {
      for (unsigned i = 0; i != numThreads; ++i) {
        mThreads.emplace_back(std::thread(&ThreadPool::workerThread, this));
      }
    } catch (...) {
      std::unique_lock lk{ mMutex };
      mDone = true;
      mWaitForTasks.notify_all();
      throw;
    }
  }

  /**
   * @brief Submits fn to the queue
   * @param fn Function to be executed on the thread queue
   * @return A future that can be used to check the value once it's been
   * processed by the thread queue
   */
  template<typename FunctionType>
  std::future<typename std::result_of<FunctionType()>::type> submit(
    FunctionType fn)
  {
    typedef typename std::result_of<FunctionType()>::type ResultType;
    std::packaged_task<ResultType()> task{ std::move(fn) };
    std::future<ResultType> res{ task.get_future() };
    std::unique_lock lk{ mMutex };
    mWorkQueue.push(std::move(task));
    mWaitForTasks.notify_one();
    return res;
  }

  ~ThreadPool()
  {
    std::unique_lock lk{ mMutex };
    mDone = true;
    mWaitForTasks.notify_one();
    lk.unlock();
    for (auto& thread : mThreads) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }

private:
  bool tryPop(FunctionWrapper& value)
  {
    if (mWorkQueue.empty()) {
      return false;
    }
    value = std::move(mWorkQueue.front());
    mWorkQueue.pop();
    return true;
  }

  void workerThread()
  {
    unsigned yieldCounter{ 0 };
    while (true) {
      FunctionWrapper task;
      std::unique_lock lk{ mMutex };
      mWaitForTasks.wait(
        lk, [this]() -> bool { return !mWorkQueue.empty() || mDone; });

      if (mDone && (mWorkQueue.empty() || !mWaitForAllTasksToFinish)) {
        break;
      }

      while (tryPop(task)) {
        lk.unlock();
        task();
        lk.lock();
      }

      if (yieldCounter >= cMaxYielding) {
        yieldCounter = 0;
      } else {
        ++yieldCounter;
        std::this_thread::yield();
      }
    }
    mWaitForTasks.notify_all();
  }

  /// True when the queue is finished and the worker threads should halt
  bool mDone;
  /// True if the worker threads should wait until all the tasks have been
  /// processed
  bool mWaitForAllTasksToFinish;
  /// Contains all the work to be done
  std::queue<FunctionWrapper> mWorkQueue;
  /// Spawned thread objects are stored here
  std::vector<std::thread> mThreads;
  /// A mutex
  mutable std::mutex mMutex;
  /// This condition is used to signal to the worker threads if an event has
  /// happened
  std::condition_variable mWaitForTasks;
  /// The thread loop will yield execution until this maximum was reached, then
  /// it "sleeps" by waiting on a CV
  static constexpr auto cMaxYielding{ 500 };
};

#endif

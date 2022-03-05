#include "ThreadPool.h"
#include <gtest/gtest.h>
#include <unordered_set>

class ThreadPoolTest : public testing::Test
{
public:
  ThreadPoolTest()
  {
    mThreadPool =
      std::make_unique<ThreadPool>(std::thread::hardware_concurrency(), true);
  }

  ~ThreadPoolTest() {}
  void SetUp() override {}

  void TearDown() override {}

protected:
  std::unique_ptr<ThreadPool> mThreadPool;
};

/*
 * Submit a couple of tasks and check if the counter was incremented to the
 * number of tasks submitted
 */
TEST_F(ThreadPoolTest, ThreadsCanIncrementCounterSimultaneously)
{
  std::atomic<int> mCounter{ 0 };

  const auto threadsToSpawn{ 200 };
  for (int i = 0; i != threadsToSpawn; ++i) {
    mThreadPool->submit([&]() { ++mCounter; });
  }

  mThreadPool = nullptr;
  ASSERT_EQ(mCounter, threadsToSpawn);
}

/*
 * Submit one task, check if the future holds the correct value
 */
TEST_F(ThreadPoolTest, RetunsCorrectResultsOnTheFuture)
{
  const auto expectedValue{ 666 };
  auto fut = mThreadPool->submit([]() -> int { return expectedValue; });
  mThreadPool = nullptr;

  ASSERT_EQ(fut.get(), expectedValue);
}

/*
 * Spawn a lot of tasks, get their thread IDs and count how many thread IDs we
 * got If it's the same as the threads running on the queue, then the thread
 * queue is using all the threads to process the tasks that are submitted
 * Although unlikely, this test can spuriously fail
 */
TEST(NoFixture_TestThreadPool, ThreadQueueUsesAllThreadsToProcessTasks)
{
  constexpr auto numTasks{ 1000 };
  constexpr auto numThreads{ 3 };
  std::vector<std::future<std::thread::id>> results;
  results.reserve(numTasks);
  auto threadPool = std::make_unique<ThreadPool>(numThreads, true);

  for (int i = 0; i != numTasks; ++i) {
    auto fut =
      threadPool->submit([]() -> auto { return std::this_thread::get_id(); });
    results.push_back(std::move(fut));
  }

  threadPool = nullptr;

  std::unordered_set<std::thread::id> counter;
  for (auto& fut : results) {
    counter.emplace(fut.get());
  }

  ASSERT_EQ(counter.size(), numThreads);
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  auto ret = RUN_ALL_TESTS();
  return ret;
}

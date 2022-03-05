#include "ParallelAccumulate.h"
#include "ThreadPool.h"
#include <gtest/gtest.h>

class ParallelAccumulate : public testing::Test
{
public:
  ParallelAccumulate() { mThreadPool = std::make_unique<ThreadPool>(24, true); }
  ~ParallelAccumulate() {}

  void SetUp() override {}
  void TearDown() override {}

protected:
  std::unique_ptr<ThreadPool> mThreadPool;
};

/*
 * Performs parallel accumulation
 */
TEST_F(ParallelAccumulate, ParrallelAccumulation)
{
  std::vector<int> numbers{ 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  auto res =
    parallelAccumulate(numbers.cbegin(), numbers.cend(), 0, *mThreadPool);
  ASSERT_EQ(res, 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9);
}

/*
 * Performs parallel accumulation with a lot of stuff
 */
TEST_F(ParallelAccumulate, BigParrallelAccumulation)
{
  constexpr auto cElements{ 9999999 };
  std::vector<unsigned> numbers;
  numbers.reserve(cElements);
  for (unsigned i = 0; i != cElements; ++i) {
    numbers.push_back(i);
  }
  auto res =
    parallelAccumulate(numbers.cbegin(), numbers.cend(), 0, *mThreadPool);
  ASSERT_EQ(std::accumulate(numbers.cbegin(), numbers.cend(), 0), res);
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  auto ret = RUN_ALL_TESTS();
  return ret;
}

#include <gtest/gtest.h>
#include "ThreadPool.h"

class TestThreadPool: public testing::Test
{
public:
	TestThreadPool() {

	}

	~TestThreadPool() {

	}
	void SetUp() override
	{}

	void TearDown() override
	{}

protected:

};

TEST_F(TestThreadPool, IncrementsALot)
{
	auto threadPool = std::make_unique<ThreadPool>();
	std::atomic<int> mCounter;

	const auto threadsToSpawn {200};
		for(int i = 0; i!=threadsToSpawn; ++i) {
			threadPool->submit([&](){
				++mCounter;
			});
		}

	threadPool=nullptr;
	ASSERT_EQ(mCounter, threadsToSpawn);
}

int main(int argc, char **argv)
{
	testing::InitGoogleTest(&argc, argv);
	auto ret = RUN_ALL_TESTS();
	return ret;
}

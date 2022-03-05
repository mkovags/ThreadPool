#include "ThreadPool.h"
#include <numeric>

#ifndef PARALLEL_ACCUMULATE_H
#define PARALLEL_ACCUMULATE_H

template<typename Iterator, typename T>
struct AccumulateBlock
{
  AccumulateBlock(Iterator first, Iterator last)
    : first{ first }
    , last{ last } {};

  T operator()() { return std::accumulate(first, last, T()); }

  Iterator first, last;
};

template<typename Iterator, typename T>
T parallelAccumulate(Iterator first,
                     Iterator last,
                     T init,
                     ThreadPool& threadPool)
{
  unsigned long const length = std::distance(first, last);
  if (!length)
    return init;
  unsigned long const block_size = 25;
  unsigned long const num_blocks = (length + block_size - 1) / block_size;
  std::vector<std::future<T>> futures(num_blocks - 1);
  Iterator block_start = first;

  for (unsigned long i = 0; i < (num_blocks - 1); ++i) {
    Iterator block_end = block_start;
    std::advance(block_end, block_size);
    futures[i] =
      threadPool.submit(AccumulateBlock<Iterator, T>(block_start, block_end));

    block_start = block_end;
  }
  T last_result = AccumulateBlock<Iterator, T>(block_start, last)();
  T result = init;
  for (unsigned long i = 0; i < (num_blocks - 1); ++i) {
    result += futures[i].get();
  }
  result += last_result;
  return result;
}

#endif

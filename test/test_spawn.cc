//
// spawn.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (c) 2019 Casey Bodley (cbodley at redhat dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <boost/asio/spawn.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/system_timer.hpp>
#include <gtest/gtest.h>


boost::coroutines::attributes with_attributes()
{
  return boost::coroutines::attributes(65536);
}

struct counting_handler {
  int& count;
  counting_handler(int& count) : count(count) {}
  void operator()() { ++count; }
  template <typename T>
  void operator()(boost::asio::basic_yield_context<T>) { ++count; }
};

TEST(Spawn, SpawnFunction)
{
  boost::asio::io_context ioc;
  int called = 0;
  boost::asio::spawn(counting_handler(called));
  ASSERT_EQ(0, ioc.run()); // runs in system executor
  ASSERT_TRUE(ioc.stopped());
  ASSERT_EQ(1, called);
}
#if 0 // XXX: spawn(Function&&, attributes) calls spawn(Handler&&, Function&&)
TEST(Spawn, SpawnFunctionAttributes)
{
  boost::asio::io_context ioc;
  int called = 0;
  boost::asio::spawn(counting_handler(called),
                     with_attributes());
  ASSERT_EQ(0, ioc.run());
  ASSERT_TRUE(ioc.stopped());
  ASSERT_EQ(1, called);
}
#endif
TEST(Spawn, SpawnHandler)
{
  boost::asio::io_context ioc;
  boost::asio::strand<boost::asio::io_context::executor_type> strand(ioc.get_executor());
  int called = 0;
  boost::asio::spawn(bind_executor(strand, counting_handler(called)),
                     counting_handler(called));
  ASSERT_EQ(1, ioc.run());
  ASSERT_TRUE(ioc.stopped());
  ASSERT_EQ(2, called);
}

TEST(Spawn, SpawnHandlerAttributes)
{
  boost::asio::io_context ioc;
  typedef boost::asio::io_context::executor_type executor_type;
  boost::asio::strand<executor_type> strand(ioc.get_executor());
  int called = 0;
  boost::asio::spawn(bind_executor(strand, counting_handler(called)),
                     counting_handler(called),
                     with_attributes());
  ASSERT_EQ(1, ioc.run());
  ASSERT_TRUE(ioc.stopped());
  ASSERT_EQ(2, called);
}

struct spawn_counting_handler {
  int& count;
  spawn_counting_handler(int& count) : count(count) {}
  template <typename T>
  void operator()(boost::asio::basic_yield_context<T> y)
  {
    boost::asio::spawn(y, counting_handler(count));
    ++count;
  }
};

TEST(Spawn, SpawnYieldContext)
{
  boost::asio::io_context ioc;
  int called = 0;
  boost::asio::spawn(bind_executor(ioc.get_executor(),
                                   counting_handler(called)),
                     spawn_counting_handler(called));
  ASSERT_EQ(1, ioc.run());
  ASSERT_TRUE(ioc.stopped());
  ASSERT_EQ(3, called);
}

struct spawn_alloc_counting_handler {
  int& count;
  spawn_alloc_counting_handler(int& count) : count(count) {}
  template <typename T>
  void operator()(boost::asio::basic_yield_context<T> y)
  {
    boost::asio::spawn(y, counting_handler(count),
                       with_attributes());
    ++count;
  }
};

TEST(Spawn, SpawnYieldContextAttributes)
{
  boost::asio::io_context ioc;
  int called = 0;
  boost::asio::spawn(bind_executor(ioc.get_executor(),
                                   counting_handler(called)),
                     spawn_alloc_counting_handler(called));
  ASSERT_EQ(1, ioc.run());
  ASSERT_TRUE(ioc.stopped());
  ASSERT_EQ(3, called);
}

TEST(Spawn, SpawnExecutor)
{
  boost::asio::io_context ioc;
  int called = 0;
  boost::asio::spawn(ioc.get_executor(), counting_handler(called));
  ASSERT_EQ(1, ioc.run());
  ASSERT_TRUE(ioc.stopped());
  ASSERT_EQ(1, called);
}

TEST(Spawn, SpawnExecutorAttributes)
{
  boost::asio::io_context ioc;
  int called = 0;
  boost::asio::spawn(ioc.get_executor(),
                     counting_handler(called),
                     with_attributes());
  ASSERT_EQ(1, ioc.run());
  ASSERT_TRUE(ioc.stopped());
  ASSERT_EQ(1, called);
}

TEST(Spawn, SpawnStrand)
{
  boost::asio::io_context ioc;
  typedef boost::asio::io_context::executor_type executor_type;
  int called = 0;
  boost::asio::spawn(boost::asio::strand<executor_type>(ioc.get_executor()),
                     counting_handler(called));
  ASSERT_EQ(1, ioc.run());
  ASSERT_TRUE(ioc.stopped());
  ASSERT_EQ(1, called);
}

TEST(Spawn, SpawnStrandAttributes)
{
  boost::asio::io_context ioc;
  typedef boost::asio::io_context::executor_type executor_type;
  int called = 0;
  boost::asio::spawn(boost::asio::strand<executor_type>(ioc.get_executor()),
                     counting_handler(called),
                     with_attributes());
  ASSERT_EQ(1, ioc.run());
  ASSERT_TRUE(ioc.stopped());
  ASSERT_EQ(1, called);
}

TEST(Spawn, SpawnExecutionContext)
{
  boost::asio::io_context ioc;
  int called = 0;
  boost::asio::spawn(ioc, counting_handler(called));
  ASSERT_EQ(1, ioc.run());
  ASSERT_TRUE(ioc.stopped());
  ASSERT_EQ(1, called);
}

TEST(Spawn, SpawnExecutionContextAttributes)
{
  boost::asio::io_context ioc;
  int called = 0;
  boost::asio::spawn(ioc, counting_handler(called),
                     with_attributes());
  ASSERT_EQ(1, ioc.run());
  ASSERT_TRUE(ioc.stopped());
  ASSERT_EQ(1, called);
}

typedef boost::asio::system_timer timer_type;

struct spawn_wait_handler {
  timer_type& timer;
  spawn_wait_handler(timer_type& timer) : timer(timer) {}
  template <typename T>
  void operator()(boost::asio::basic_yield_context<T> yield)
  {
    timer.async_wait(yield);
  }
};

TEST(Spawn, SpawnTimer)
{
  int called = 0;
  {
    boost::asio::io_context ioc;
    timer_type timer(ioc, boost::asio::chrono::hours(0));

    boost::asio::spawn(bind_executor(ioc.get_executor(),
                                     counting_handler(called)),
                       spawn_wait_handler(timer));
    ASSERT_EQ(2, ioc.run());
    ASSERT_TRUE(ioc.stopped());
  }
  ASSERT_EQ(1, called);
}

TEST(Spawn, SpawnTimerDestruct)
{
  int called = 0;
  {
    boost::asio::io_context ioc;
    timer_type timer(ioc, boost::asio::chrono::hours(65536));

    boost::asio::spawn(bind_executor(ioc.get_executor(),
                                     counting_handler(called)),
                       spawn_wait_handler(timer));
    ASSERT_EQ(1, ioc.run_one());
    ASSERT_TRUE(!ioc.stopped());
  }
  ASSERT_EQ(0, called);
}
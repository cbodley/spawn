//
// impl/spawn.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (c) 2017 Oliver Kowalke (oliver dot kowalke at gmail dot com)
// Copyright (c) 2019 Casey Bodley (cbodley at redhat dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_IMPL_SPAWN_HPP
#define BOOST_ASIO_IMPL_SPAWN_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <memory>

#include <boost/asio/detail/config.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/detail/atomic_count.hpp>
#include <boost/asio/detail/handler_alloc_helpers.hpp>
#include <boost/asio/detail/handler_cont_helpers.hpp>
#include <boost/asio/detail/handler_invoke_helpers.hpp>
#include <boost/asio/detail/type_traits.hpp>
#include <boost/system/system_error.hpp>
#include <boost/context/continuation.hpp>

#include <spawn/detail/is_stack_allocator.hpp>

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
namespace detail {

  class continuation_context
  {
  public:
    boost::context::continuation context_;

    continuation_context()
      : context_()
    {
    }

    void resume()
    {
      context_ = context_.resume();
    }
  };

  template <typename Handler, typename T>
  class coro_handler
  {
  public:
    coro_handler(basic_yield_context<Handler> ctx)
      : callee_(ctx.callee_.lock()),
        caller_(ctx.caller_),
        handler_(ctx.handler_),
        ready_(0),
        ec_(ctx.ec_),
        value_(0)
    {
    }

    void operator()(T value)
    {
      *ec_ = boost::system::error_code();
      *value_ = std::move(value);
      if (--*ready_ == 0)
        callee_->resume();
    }

    void operator()(boost::system::error_code ec, T value)
    {
      *ec_ = ec;
      *value_ = std::move(value);
      if (--*ready_ == 0)
        callee_->resume();
    }

  //private:
    std::shared_ptr<continuation_context> callee_;
    continuation_context& caller_;
    Handler handler_;
    atomic_count* ready_;
    boost::system::error_code* ec_;
    T* value_;
  };

  template <typename Handler>
  class coro_handler<Handler, void>
  {
  public:
    coro_handler(basic_yield_context<Handler> ctx)
      : callee_(ctx.callee_.lock()),
        caller_(ctx.caller_),
        handler_(ctx.handler_),
        ready_(0),
        ec_(ctx.ec_)
    {
    }

    void operator()()
    {
      *ec_ = boost::system::error_code();
      if (--*ready_ == 0)
        callee_->resume();
    }

    void operator()(boost::system::error_code ec)
    {
      *ec_ = ec;
      if (--*ready_ == 0)
        callee_->resume();
    }

  //private:
    std::shared_ptr<continuation_context> callee_;
    continuation_context& caller_;
    Handler handler_;
    atomic_count* ready_;
    boost::system::error_code* ec_;
  };

  template <typename Handler, typename T>
  inline void* asio_handler_allocate(std::size_t size,
      coro_handler<Handler, T>* this_handler)
  {
    return boost_asio_handler_alloc_helpers::allocate(
        size, this_handler->handler_);
  }

  template <typename Handler, typename T>
  inline void asio_handler_deallocate(void* pointer, std::size_t size,
      coro_handler<Handler, T>* this_handler)
  {
    boost_asio_handler_alloc_helpers::deallocate(
        pointer, size, this_handler->handler_);
  }

  template <typename Handler, typename T>
  inline bool asio_handler_is_continuation(coro_handler<Handler, T>*)
  {
    return true;
  }

  template <typename Function, typename Handler, typename T>
  inline void asio_handler_invoke(Function& function,
      coro_handler<Handler, T>* this_handler)
  {
    boost_asio_handler_invoke_helpers::invoke(
        function, this_handler->handler_);
  }

  template <typename Function, typename Handler, typename T>
  inline void asio_handler_invoke(const Function& function,
      coro_handler<Handler, T>* this_handler)
  {
    boost_asio_handler_invoke_helpers::invoke(
        function, this_handler->handler_);
  }

  template <typename Handler, typename T>
  class coro_async_result
  {
  public:
    typedef coro_handler<Handler, T> completion_handler_type;
    typedef T return_type;

    explicit coro_async_result(completion_handler_type& h)
      : handler_(h),
        caller_(h.caller_),
        ready_(2)
    {
      h.ready_ = &ready_;
      out_ec_ = h.ec_;
      if (!out_ec_) h.ec_ = &ec_;
      h.value_ = &value_;
    }

    return_type get()
    {
      // Must not hold shared_ptr while suspended.
      handler_.callee_.reset();

      if (--ready_ != 0)
        caller_.resume(); // suspend caller
      if (!out_ec_ && ec_) throw boost::system::system_error(ec_);
      return std::move(value_);
    }

  private:
    completion_handler_type& handler_;
    continuation_context& caller_;
    atomic_count ready_;
    boost::system::error_code* out_ec_;
    boost::system::error_code ec_;
    return_type value_;
  };

  template <typename Handler>
  class coro_async_result<Handler, void>
  {
  public:
    typedef coro_handler<Handler, void> completion_handler_type;
    typedef void return_type;

    explicit coro_async_result(completion_handler_type& h)
      : handler_(h),
        caller_(h.caller_),
        ready_(2)
    {
      h.ready_ = &ready_;
      out_ec_ = h.ec_;
      if (!out_ec_) h.ec_ = &ec_;
    }

    void get()
    {
      // Must not hold shared_ptr while suspended.
      handler_.callee_.reset();

      if (--ready_ != 0)
        caller_.resume(); // suspend caller
      if (!out_ec_ && ec_) throw boost::system::system_error(ec_);
    }

  private:
    completion_handler_type& handler_;
    continuation_context& caller_;
    atomic_count ready_;
    boost::system::error_code* out_ec_;
    boost::system::error_code ec_;
  };

} // namespace detail

#if !defined(GENERATING_DOCUMENTATION)

template <typename Handler, typename ReturnType>
class async_result<basic_yield_context<Handler>, ReturnType()>
  : public detail::coro_async_result<Handler, void>
{
public:
  explicit async_result(
    typename detail::coro_async_result<Handler,
      void>::completion_handler_type& h)
    : detail::coro_async_result<Handler, void>(h)
  {
  }
};

template <typename Handler, typename ReturnType, typename Arg1>
class async_result<basic_yield_context<Handler>, ReturnType(Arg1)>
  : public detail::coro_async_result<Handler, typename decay<Arg1>::type>
{
public:
  explicit async_result(
    typename detail::coro_async_result<Handler,
      typename decay<Arg1>::type>::completion_handler_type& h)
    : detail::coro_async_result<Handler, typename decay<Arg1>::type>(h)
  {
  }
};

template <typename Handler, typename ReturnType>
class async_result<basic_yield_context<Handler>,
    ReturnType(boost::system::error_code)>
  : public detail::coro_async_result<Handler, void>
{
public:
  explicit async_result(
    typename detail::coro_async_result<Handler,
      void>::completion_handler_type& h)
    : detail::coro_async_result<Handler, void>(h)
  {
  }
};

template <typename Handler, typename ReturnType, typename Arg2>
class async_result<basic_yield_context<Handler>,
    ReturnType(boost::system::error_code, Arg2)>
  : public detail::coro_async_result<Handler, typename decay<Arg2>::type>
{
public:
  explicit async_result(
    typename detail::coro_async_result<Handler,
      typename decay<Arg2>::type>::completion_handler_type& h)
    : detail::coro_async_result<Handler, typename decay<Arg2>::type>(h)
  {
  }
};

template <typename Handler, typename T, typename Allocator>
struct associated_allocator<detail::coro_handler<Handler, T>, Allocator>
{
  typedef typename associated_allocator<Handler, Allocator>::type type;

  static type get(const detail::coro_handler<Handler, T>& h,
      const Allocator& a = Allocator()) BOOST_ASIO_NOEXCEPT
  {
    return associated_allocator<Handler, Allocator>::get(h.handler_, a);
  }
};

template <typename Handler, typename T, typename Executor>
struct associated_executor<detail::coro_handler<Handler, T>, Executor>
{
  typedef typename associated_executor<Handler, Executor>::type type;

  static type get(const detail::coro_handler<Handler, T>& h,
      const Executor& ex = Executor()) BOOST_ASIO_NOEXCEPT
  {
    return associated_executor<Handler, Executor>::get(h.handler_, ex);
  }
};

namespace detail {

  template <typename Handler, typename Function, typename StackAllocator>
  struct spawn_data
  {
    template <typename Hand, typename Func, typename Stack>
    spawn_data(Hand&& handler, bool call_handler, Func&& function, Stack&& salloc)
      : handler_(std::forward<Hand>(handler)),
        call_handler_(call_handler),
        function_(std::forward<Func>(function)),
        salloc_(std::forward<Stack>(salloc))
    {
    }
    spawn_data(const spawn_data&) = delete;
    spawn_data& operator=(const spawn_data&) = delete;

    Handler handler_;
    bool call_handler_;
    Function function_;
    StackAllocator salloc_;
    continuation_context caller_;
  };

  template <typename Handler, typename Function, typename StackAllocator>
  struct spawn_helper
  {
    void operator()()
    {
      callee_.reset(new continuation_context());
      callee_->context_ = boost::context::callcc(
          std::allocator_arg, std::move(data_->salloc_),
          [this] (boost::context::continuation&& c)
          {
            std::shared_ptr<spawn_data<Handler, Function, StackAllocator> > data = data_;
            data->caller_.context_ = std::move(c);
            const basic_yield_context<Handler> yh(callee_, data->caller_, data->handler_);
            (data->function_)(yh);
            if (data->call_handler_)
            {
              (data->handler_)();
            }
            boost::context::continuation caller = std::move(data->caller_.context_);
            data.reset();
            return std::move(caller);
          });
    }

    std::shared_ptr<continuation_context> callee_;
    std::shared_ptr<spawn_data<Handler, Function, StackAllocator> > data_;
  };

  template <typename Function, typename Handler,
      typename Function1, typename StackAllocator>
  inline void asio_handler_invoke(Function& function,
      spawn_helper<Handler, Function1, StackAllocator>* this_handler)
  {
    boost_asio_handler_invoke_helpers::invoke(
        function, this_handler->data_->handler_);
  }

  template <typename Function, typename Handler,
      typename Function1, typename StackAllocator>
  inline void asio_handler_invoke(const Function& function,
      spawn_helper<Handler, Function1, StackAllocator>* this_handler)
  {
    boost_asio_handler_invoke_helpers::invoke(
        function, this_handler->data_->handler_);
  }

  inline void default_spawn_handler() {}

} // namespace detail

template <typename Function, typename StackAllocator>
inline void spawn(Function&& function, StackAllocator&& salloc,
    typename enable_if<detail::is_stack_allocator<
      typename decay<StackAllocator>::type>::value>::type*)
{
  typedef typename decay<Function>::type function_type;

  typename associated_executor<function_type>::type ex(
      (get_associated_executor)(function));

  boost::asio::spawn(ex, std::forward<Function>(function), salloc);
}

template <typename Handler, typename Function, typename StackAllocator>
void spawn(Handler&& handler, Function&& function, StackAllocator&& salloc,
    typename enable_if<!is_executor<typename decay<Handler>::type>::value &&
      !is_convertible<Handler&, execution_context&>::value &&
      !detail::is_stack_allocator<typename decay<Function>::type>::value &&
      detail::is_stack_allocator<typename decay<StackAllocator>::type>::value>::type*)
{
  typedef typename decay<Handler>::type handler_type;
  typedef typename decay<Function>::type function_type;

  typename associated_executor<handler_type>::type ex(
      (get_associated_executor)(handler));

  typename associated_allocator<handler_type>::type a(
      (get_associated_allocator)(handler));

  detail::spawn_helper<handler_type, function_type, StackAllocator> helper;
  helper.data_ = std::make_shared<
      detail::spawn_data<handler_type, function_type, StackAllocator> >(
        std::forward<Handler>(handler), true,
        std::forward<Function>(function),
        std::forward<StackAllocator>(salloc));

  ex.dispatch(helper, a);
}

template <typename Handler, typename Function, typename StackAllocator>
void spawn(basic_yield_context<Handler> ctx,
    Function&& function, StackAllocator&& salloc,
    typename enable_if<detail::is_stack_allocator<
      typename decay<StackAllocator>::type>::value>::type*)
{
  typedef typename decay<Function>::type function_type;

  Handler handler(ctx.handler_); // Explicit copy that might be moved from.

  typename associated_executor<Handler>::type ex(
      (get_associated_executor)(handler));

  typename associated_allocator<Handler>::type a(
      (get_associated_allocator)(handler));

  detail::spawn_helper<Handler, function_type, StackAllocator> helper;
  helper.data_ = std::make_shared<
      detail::spawn_data<Handler, function_type, StackAllocator> >(
        std::forward<Handler>(handler), false,
        std::forward<Function>(function),
        std::forward<StackAllocator>(salloc));

  ex.dispatch(helper, a);
}

template <typename Function, typename Executor, typename StackAllocator>
inline void spawn(const Executor& ex,
    Function&& function, StackAllocator&& salloc,
    typename enable_if<is_executor<Executor>::value &&
      detail::is_stack_allocator<typename decay<StackAllocator>::type>::value>::type*)
{
  boost::asio::spawn(boost::asio::strand<Executor>(ex),
      std::forward<Function>(function),
      std::forward<StackAllocator>(salloc));
}

template <typename Function, typename Executor, typename StackAllocator>
inline void spawn(const strand<Executor>& ex,
    Function&& function, StackAllocator&& salloc,
    typename enable_if<detail::is_stack_allocator<
      typename decay<StackAllocator>::type>::value>::type*)
{
  boost::asio::spawn(boost::asio::bind_executor(
        ex, &detail::default_spawn_handler),
      std::forward<Function>(function),
      std::forward<StackAllocator>(salloc));
}

template <typename Function, typename StackAllocator>
inline void spawn(const boost::asio::io_context::strand& s,
    Function&& function, StackAllocator&& salloc,
    typename enable_if<detail::is_stack_allocator<
      typename decay<StackAllocator>::type>::value>::type*)
{
  boost::asio::spawn(boost::asio::bind_executor(
        s, &detail::default_spawn_handler),
      std::forward<Function>(function),
      std::forward<StackAllocator>(salloc));
}

template <typename Function, typename ExecutionContext, typename StackAllocator>
inline void spawn(ExecutionContext& ctx,
    Function&& function, StackAllocator&& salloc,
    typename enable_if<is_convertible<
      ExecutionContext&, execution_context&>::value &&
      detail::is_stack_allocator<typename decay<StackAllocator>::type>::value>::type*)
{
  boost::asio::spawn(ctx.get_executor(),
      std::forward<Function>(function),
      std::forward<StackAllocator>(salloc));
}

#endif // !defined(GENERATING_DOCUMENTATION)

} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_IMPL_SPAWN_HPP

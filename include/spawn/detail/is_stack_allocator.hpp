//
// detail/is_stack_allocator.hpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Casey Bodley (cbodley at redhat dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_IS_STACK_ALLOCATOR_HPP
#define BOOST_ASIO_DETAIL_IS_STACK_ALLOCATOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>
#include <boost/asio/detail/type_traits.hpp>

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
namespace detail {

struct stack_allocator_memfns_base
{
  void allocate();
  void deallocate();
};

template <typename T>
struct stack_allocator_memfns_derived
  : T, stack_allocator_memfns_base
{
};

template <typename T, T>
struct stack_allocator_memfns_check
{
};

template <typename>
char (&allocate_memfn_helper(...))[2];

template <typename T>
char allocate_memfn_helper(
    stack_allocator_memfns_check<
      void (stack_allocator_memfns_base::*)(),
      &stack_allocator_memfns_derived<T>::allocate>*);

template <typename>
char (&deallocate_memfn_helper(...))[2];

template <typename T>
char deallocate_memfn_helper(
    stack_allocator_memfns_check<
      void (stack_allocator_memfns_base::*)(),
      &stack_allocator_memfns_derived<T>::deallocate>*);

template <typename>
char (&traits_type_typedef_helper(...))[2];

template <typename T>
char traits_type_typedef_helper(typename T::traits_type*);

template <typename T>
struct is_stack_allocator_class
  : integral_constant<bool,
      sizeof(allocate_memfn_helper<T>(0)) != 1 &&
      sizeof(deallocate_memfn_helper<T>(0)) != 1 &&
      sizeof(traits_type_typedef_helper<T>(0)) == 1>
{
};

template <typename T>
struct is_stack_allocator
  : conditional<is_class<T>::value,
      is_stack_allocator_class<T>,
      false_type>::type
{
};

} // namespace detail
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_DETAIL_IS_STACK_ALLOCATOR_HPP

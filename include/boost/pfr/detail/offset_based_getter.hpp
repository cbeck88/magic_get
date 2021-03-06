// Copyright (c) 2017 Chris beck
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PFR_DETAIL_OFFSET_BASED_GETTER_HPP
#define BOOST_PFR_DETAIL_OFFSET_BASED_GETTER_HPP
#pragma once

#include <boost/pfr/detail/config.hpp>

#include <type_traits>
#include <utility>
#include <boost/pfr/detail/sequence_tuple.hpp>
#include <boost/pfr/detail/size_array.hpp>


namespace boost { namespace pfr { namespace detail {

template <std::size_t Index>
using size_t_ = std::integral_constant<std::size_t, Index >;

// Our own implementation of std::aligned_storage. On godbolt with MSVC, I have compilation errors
// using the standard version, it seems the compiler cannot generate default ctor.

template<std::size_t s, std::size_t a>
struct internal_aligned_storage {
   alignas(a) char storage_[s];
};

// Metafunction that replaces tuple<T1, T2, T3, ...> with
// tuple<std::aligned_storage_t<sizeof(T1), alignof(T1)>, std::aligned_storage<sizeof(T2), alignof(T2)>, ...>
//
// The point is, the new tuple is "layout compatible" in the sense that members have the same offsets,
// but this tuple is constexpr constructible.

template <typename T>
struct tuple_of_aligned_storage;

template <typename... Ts>
struct tuple_of_aligned_storage<sequence_tuple::tuple<Ts...>> {
  using type = sequence_tuple::tuple<internal_aligned_storage<sizeof(Ts), alignof(Ts)>...>;
};

// Note: If pfr has a typelist also, could also have an overload for that here

template <typename T>
using tuple_of_aligned_storage_t = typename tuple_of_aligned_storage<T>::type;

/***
 * Given a structure type and its sequence of members, we want to build a function
 * object "getter" that implements a version of `std::get` using offset arithmetic
 * and reinterpret_cast.
 *
 * typename U should be a user-defined struct
 * typename S should be a sequence_tuple which is layout compatible with U
 */

template <typename U, typename S>
class offset_based_getter {
  static_assert(sizeof(U) == sizeof(S), "Member sequence does not indicate correct size for struct type!");
  static_assert(alignof(U) == alignof(S), "Member sequence does not indicate correct alignment for struct type!");

  static_assert(!std::is_const<U>::value, "const should be stripped from user-defined type when using offset_based_getter or overload resolution will be ambiguous later, this indicates an error within pfr");
  static_assert(!std::is_reference<U>::value, "reference should be stripped from user-defined type when using offset_based_getter or overload resolution will be ambiguous later, this indicates an error within pfr");
  static_assert(!std::is_volatile<U>::value, "volatile should be stripped from user-defined type when using offset_based_getter or overload resolution will be ambiguous later. this indicates an error within pfr");

  // Get type of idx'th member
  template <std::size_t idx>
  using index_t = typename sequence_tuple::tuple_element<idx, S>::type;
  
  // Get offset of idx'th member
  // Idea: Layout object has the same offsets as instance of S, so if S and U are layout compatible, then these offset
  // calculations are correct.
  template <std::size_t idx>
  static std::ptrdiff_t offset() {
    /* static */ /* constexpr */ tuple_of_aligned_storage_t<S> layout{};
    /* static */ /* constexpr */ const auto * member = &sequence_tuple::get<idx>(layout);

    /**
     * The goal is that the layout object should ultimately get optimized out of the binary, and this function should
     * get reduced to a constant and inlined.
     *
     * Unfortunately we can't do reinterpret_cast in a constexpr function but we can try to make it really easy to optimize.
     *
     * There are some possible variations using static and constexpr above, but on compilers I tested, gcc and clang
     * seem to do the right thing always here, and constexpr here may trip up even early releases of MSVC2017.
     *
     * If `member` and `&layout` are constexpr then the function should be very easy to optimize to a constant.
     * But if we take address of static object layout that may force it to exist and make it harder to ultimately
     * eliminate from the binary.
     *
     * If layout is a stack object, its address is not constexpr, but most compilers should still be able to figure out
     * that the difference of the two pointers is a constant.
     */

    return reinterpret_cast<const char *>(member) - reinterpret_cast<const char *>(&layout);
  }

  // Encapsulates offset arithmetic and reinterpret_cast
  template <std::size_t idx>
  index_t<idx> * get_pointer(U * u) const {
    return reinterpret_cast<index_t<idx> *>(reinterpret_cast<char *>(u) + offset<idx>());
  }

  template <std::size_t idx>
  const index_t<idx> * get_pointer(const U * u) const {
    return reinterpret_cast<const index_t<idx> *>(reinterpret_cast<const char *>(u) + offset<idx>());
  }

  template <std::size_t idx>
  volatile index_t<idx> * get_pointer(volatile U * u) const {
    return reinterpret_cast<volatile index_t<idx> *>(reinterpret_cast<volatile char *>(u) + offset<idx>());
  }

  template <std::size_t idx>
  const volatile index_t<idx> * get_pointer(const volatile U * u) const {
    return reinterpret_cast<const volatile index_t<idx> *>(reinterpret_cast<const volatile char *>(u) + offset<idx>());
  }

public:
  template <std::size_t idx>
  index_t<idx> & get(U & u, size_t_<idx>) const {
    return *get_pointer<idx>(&u);
  }

  template <std::size_t idx>
  index_t<idx> const & get(U const & u, size_t_<idx>) const {
    return *get_pointer<idx>(&u);
  }

  template <std::size_t idx>
  index_t<idx> volatile & get(U volatile & u, size_t_<idx>) const {
    return *get_pointer<idx>(&u);
  }

  template <std::size_t idx>
  index_t<idx> const volatile & get(U const volatile & u, size_t_<idx>) const {
    return *get_pointer<idx>(&u);
  }

  /*
  This overload was similarly commented out in cast_to_layout_compatible
  
  template <std::size_t idx>
  index_t<idx> && get(U && u, size_t_<idx>) const {
    return std::move(*get_pointer<idx>(&u));
  }
  */
};


}}} // namespace boost::pfr::detail

#endif // BOOST_PFR_DETAIL_OFFSET_LIST_HPP

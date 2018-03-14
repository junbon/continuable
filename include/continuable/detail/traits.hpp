
/*

                        /~` _  _ _|_. _     _ |_ | _
                        \_,(_)| | | || ||_|(_||_)|(/_

                    https://github.com/Naios/continuable
                                   v3.0.0

  Copyright(c) 2015 - 2018 Denis Blank <denis.blank at outlook dot com>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files(the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions :

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
**/

#ifndef CONTINUABLE_DETAIL_TRAITS_HPP_INCLUDED
#define CONTINUABLE_DETAIL_TRAITS_HPP_INCLUDED

#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

#include <continuable/detail/features.hpp>

namespace cti {
namespace detail {
namespace traits {
namespace detail {
template <typename T, typename... Args>
struct index_of_impl;
template <typename T, typename... Args>
struct index_of_impl<T, T, Args...> : std::integral_constant<std::size_t, 0U> {
};
template <typename T, typename U, typename... Args>
struct index_of_impl<T, U, Args...>
    : std::integral_constant<std::size_t,
                             1 + index_of_impl<T, Args...>::value> {};
} // namespace detail

/// Evaluates to the index of T in the given pack
template <typename T, typename... Args>
using index_of_t = detail::index_of_impl<T, Args...>;

/// A tagging type for wrapping other types
template <typename... T>
struct identity {};

template <typename>
struct is_identity : std::false_type {};
template <typename... Args>
struct is_identity<identity<Args...>> : std::true_type {};

template <typename T>
using identify = std::conditional_t<is_identity<std::decay_t<T>>::value, T,
                                    identity<std::decay_t<T>>>;

#if defined(CONTINUABLE_HAS_CXX17_VOID_T)
using std::void_t;
#else
namespace detail {
// Equivalent to C++17's std::void_t which targets a bug in GCC,
// that prevents correct SFINAE behavior.
// See http://stackoverflow.com/questions/35753920 for details.
template <typename...>
struct deduce_to_void : std::common_type<void> {};
} // namespace detail

/// C++17 like void_t type
template <typename... T>
using void_t = typename detail::deduce_to_void<T...>::type;
#endif // CONTINUABLE_HAS_CXX17_VOID_T

namespace detail {
template <typename Type, typename TrueCallback>
constexpr void static_if_impl(std::true_type, Type&& type,
                              TrueCallback&& trueCallback) {
  std::forward<TrueCallback>(trueCallback)(std::forward<Type>(type));
}

template <typename Type, typename TrueCallback>
constexpr void static_if_impl(std::false_type, Type&& /*type*/,
                              TrueCallback&& /*trueCallback*/) {
}

template <typename Type, typename TrueCallback, typename FalseCallback>
constexpr auto static_if_impl(std::true_type, Type&& type,
                              TrueCallback&& trueCallback,
                              FalseCallback&& /*falseCallback*/) {
  return std::forward<TrueCallback>(trueCallback)(std::forward<Type>(type));
}

template <typename Type, typename TrueCallback, typename FalseCallback>
constexpr auto static_if_impl(std::false_type, Type&& type,
                              TrueCallback&& /*trueCallback*/,
                              FalseCallback&& falseCallback) {
  return std::forward<FalseCallback>(falseCallback)(std::forward<Type>(type));
}

/// Evaluates to the size of the given tuple like type,
// / if the type has no static size it will be one.
template <typename T, typename Enable = void>
struct tuple_like_size : std::integral_constant<std::size_t, 1U> {};
template <typename T>
struct tuple_like_size<T, void_t<decltype(std::tuple_size<T>::value)>>
    : std::tuple_size<T> {};
} // namespace detail

/// Returns the pack size of the given empty pack
constexpr std::size_t pack_size_of(identity<>) noexcept {
  return 0U;
}
/// Returns the pack size of the given type
template <typename T>
constexpr std::size_t pack_size_of(identity<T>) noexcept {
  return detail::tuple_like_size<T>::value;
}
/// Returns the pack size of the given type
template <typename First, typename Second, typename... Args>
constexpr std::size_t pack_size_of(identity<First, Second, Args...>) noexcept {
  return 2U + sizeof...(Args);
}

/// Returns an index sequence of the given type
template <typename T>
constexpr auto sequence_of(identity<T>) noexcept {
  constexpr auto const size = pack_size_of(identity<T>{});
  return std::make_index_sequence<size>();
}

/// Invokes the callback only if the given type matches the check
template <typename Type, typename Check, typename TrueCallback>
constexpr void static_if(Type&& type, Check&& check,
                         TrueCallback&& trueCallback) {
  detail::static_if_impl(std::forward<Check>(check)(type),
                         std::forward<Type>(type),
                         std::forward<TrueCallback>(trueCallback));
}

/// Invokes the callback only if the given type matches the check
template <typename Type, typename Check, typename TrueCallback,
          typename FalseCallback>
constexpr auto static_if(Type&& type, Check&& check,
                         TrueCallback&& trueCallback,
                         FalseCallback&& falseCallback) {
  return detail::static_if_impl(std::forward<Check>(check)(type),
                                std::forward<Type>(type),
                                std::forward<TrueCallback>(trueCallback),
                                std::forward<FalseCallback>(falseCallback));
}

namespace detail {
/// Calls the given unpacker with the content of the given sequenceable
template <typename U, typename F, std::size_t... I>
constexpr auto unpack_impl(U&& unpacker, F&& first_sequenceable,
                           std::integer_sequence<std::size_t, I...>)
    -> decltype(std::forward<U>(unpacker)(
        std::get<I>(std::forward<F>(first_sequenceable))...)) {
  (void)first_sequenceable;
  return std::forward<U>(unpacker)(
      std::get<I>(std::forward<F>(first_sequenceable))...);
}
} // namespace detail

/// Calls the given callable object with the content of the given sequenceable
///
/// \note We can't use std::apply here since this implementation is SFINAE
///       aware and the std version not! This would lead to compilation errors.
template <typename Callable, typename TupleLike,
          typename Sequence = std::make_index_sequence<
              std::tuple_size<std::decay_t<TupleLike>>::value>>
constexpr auto unpack(Callable&& obj, TupleLike&& tuple_like)
    -> decltype(detail::unpack_impl(std::forward<Callable>(obj),
                                    std::forward<TupleLike>(tuple_like),
                                    Sequence{})) {

  return detail::unpack_impl(std::forward<Callable>(obj),
                             std::forward<TupleLike>(tuple_like), Sequence{});
}

namespace detail {
template <typename T, typename Args, typename = traits::void_t<>>
struct is_invokable_impl : std::common_type<std::false_type> {};

template <typename T, typename... Args>
struct is_invokable_impl<
    T, std::tuple<Args...>,
    void_t<decltype(std::declval<T>()(std::declval<Args>()...))>>
    : std::common_type<std::true_type> {};
} // namespace detail

/// Deduces to a std::true_type if the given type is callable with the arguments
/// inside the given tuple.
/// The main reason for implementing it with the detection idiom instead of
/// hana like detection is that MSVC has issues with capturing raw template
/// arguments inside lambda closures.
///
/// ```cpp
/// traits::is_invokable<object, std::tuple<Args...>>
/// ```
template <typename T, typename Args>
using is_invokable_from_tuple =
    typename detail::is_invokable_impl<T, Args>::type;

// Checks whether the given callable object is invocable with the given
// arguments. This doesn't take member functions into account!
template <typename T, typename... Args>
using is_invocable = is_invokable_from_tuple<T, std::tuple<Args...>>;

/// Deduces to a std::false_type
template <typename T>
using fail = std::integral_constant<bool, !std::is_same<T, T>::value>;

#ifdef CONTINUABLE_HAS_CXX17_DISJUNCTION
using std::disjunction;
#else
namespace detail {
/// Declares a C++14 polyfill for C++17 std::disjunction.
template <typename Args, typename = void_t<>>
struct disjunction_impl : std::common_type<std::true_type> {};
template <typename... Args>
struct disjunction_impl<identity<Args...>,
                        void_t<std::enable_if_t<!bool(Args::value)>...>>
    : std::common_type<std::false_type> {};
} // namespace detail

template <typename... Args>
using disjunction = typename detail::disjunction_impl<identity<Args...>>::type;
#endif // CONTINUABLE_HAS_CXX17_DISJUNCTION

#ifdef CONTINUABLE_HAS_CXX17_CONJUNCTION
using std::conjunction;
#else
namespace detail {
/// Declares a C++14 polyfill for C++17 std::conjunction.
template <typename Args, typename = void_t<>>
struct conjunction_impl : std::common_type<std::false_type> {};
template <typename... Args>
struct conjunction_impl<identity<Args...>,
                        void_t<std::enable_if_t<bool(Args::value)>...>>
    : std::common_type<std::true_type> {};
} // namespace detail

template <typename... Args>
using conjunction = typename detail::conjunction_impl<identity<Args...>>::type;
#endif // CONTINUABLE_HAS_CXX17_CONJUNCTION

} // namespace traits
} // namespace detail
} // namespace cti

#endif // CONTINUABLE_DETAIL_TRAITS_HPP_INCLUDED

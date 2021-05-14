#ifndef SUBPROCESS_TYPE_HPP
#define SUBPROCESS_TYPE_HPP

#include <string>
#include <typeinfo>

namespace subprocess::internal {
#include <string_view>

  template<typename T>
  constexpr auto get_type_name() -> std::string_view {
#if defined(__clang__)
    constexpr auto prefix = std::string_view{ "[T = " };
    constexpr auto suffix = "]";
    constexpr auto function = std::string_view{ __PRETTY_FUNCTION__ };
#elif defined(__GNUC__)
    constexpr auto prefix = std::string_view{ "with T = " };
    constexpr auto suffix = "; ";
    constexpr auto function = std::string_view{ __PRETTY_FUNCTION__ };
#elif defined(__MSC_VER)
    constexpr auto prefix = std::string_view{ "get_type_name<" };
    constexpr auto suffix = ">(void)";
    constexpr auto function = std::string_view{ __FUNCSIG__ };
#else
#error Unsupported compiler
#endif

    const auto start = function.find(prefix) + prefix.size();
    const auto end = function.find(suffix);
    const auto size = end - start;

    return function.substr(start, size);
  }
}  // namespace subprocess::internal
#endif

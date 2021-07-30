#ifndef SUBPROCESS_VARIANT_HELPERS_H_
#define SUBPROCESS_VARIANT_HELPERS_H_

#include <iostream>
#include <string>
#include <type_traits>

#include "subprocess/type_name.hpp"

namespace subprocess::internal {

  template<typename T>
  class DefinesToString {
    // grumble grumble, wish we had concepts
    typedef char one;
    struct two {
      char x[2];
    };

    template<typename C>
    static one test(decltype(&C::toString));
    template<typename C>
    static two test(...);

   public:
    enum { value = sizeof(test<T>(0)) == sizeof(char) };
  };

  template<class... Ts>
  struct overload : Ts... {
    using Ts::operator()...;
  };
  template<class... Ts>
  overload(Ts...) -> overload<Ts...>;

  template<typename V>
  std::string variant_to_string(const V& var) {
    return std::visit(
        [](auto&& arg) -> std::string {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (DefinesToString<T>::value) return arg.toString();
          auto itemName = get_type_name<T>();
          static const std::string prefix{ ">::" };
          auto start = itemName.find(prefix);
          if (start == std::string_view::npos) {
            start = 0;
          } else {
            start = start + prefix.size();
          }
          return std::string{ itemName.substr(start) };
        },
        var);
  }
}  // namespace subprocess::internal
#endif

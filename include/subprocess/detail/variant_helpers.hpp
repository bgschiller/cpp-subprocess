#ifndef SUBPROCESS_DETAIL_VARIANT_HELPERS_H_
#define SUBPROCESS_DETAIL_VARIANT_HELPERS_H_

#include <array>
#include <string>
#include <string_view>
#include <type_traits>

#include "type_name.hpp"

namespace subprocess::internal {

  template<typename T>
  class DefinesToString {
    // grumble grumble, wish we had concepts
    using one = char;
    struct two {
      std::array<char, 2> x;
    };

    template<typename C>
    static one test(decltype(&C::toString));
    template<typename C>
    static two test(...);

   public:
    enum class Value : bool { no = false, yes = true };
    static constexpr bool value = sizeof(test<T>(0)) == sizeof(char);
  };

  template<class... Ts>
  struct overloaded : Ts... {
    using Ts::operator()...;
  };
  template<class... Ts>
  overloaded(Ts...) -> overloaded<Ts...>;

  template<typename V>
  std::string variant_to_string(const V& var) {
    return std::visit(
        [](auto&& arg) -> std::string {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (DefinesToString<T>::value) {
            return arg.toString();
          }
          auto item_name = get_type_name<T>();
          static const std::string prefix{ ">::" };
          auto start_pos = item_name.find(prefix);
          if (start_pos == std::string_view::npos) {
            start_pos = 0;
          } else {
            start_pos = start_pos + prefix.size();
          }
          return std::string{ item_name.substr(start_pos) };
        },
        var);
  }
}  // namespace subprocess::internal
#endif

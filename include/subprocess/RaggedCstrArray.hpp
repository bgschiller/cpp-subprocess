#ifndef SUBPROCESS_RAII_CHAR_STAR_H_
#define SUBPROCESS_RAII_CHAR_STAR_H_

#include <iterator>
#include <string>
#include <vector>

namespace subprocess {
  struct RaiiCharStar {
    mutable std::vector<char> buf;
    RaiiCharStar(std::string s)
        : buf(s.c_str(), s.c_str() + s.size() + 1) { }
    operator char*() const { return &buf[0]; };
  };

  /**
   * A RaggedCstrArray owns a vector of c-strings, releasing their
   * memory when it is destroyed.
   *
   *   _ptrs: a vector of char*, one for each RaiiCharChar, terminated by a nullptr
   *   │
   *   │      _cStrs: a vector of RaiiCharStar, null-terminated C-strings
   *   │      │
   *   │      ▼
   *   │      0: Apple\0
   *   ▼         ▲
   *   0:────────┘
   *          1: Banana\0
   *             ▲
   *   1:────────┘
   *          2: Clementine\0
   *             ▲
   *   2:────────┘
   *
   *   3: nullptr
   */
  class RaggedCstrArray {
   public:
    RaggedCstrArray() { }
    RaggedCstrArray(std::initializer_list<std::string> strs)
        : _cStrs{ strs.begin(), strs.end() }
        , _ptrs{ _cStrs.begin(), _cStrs.end() } { }

    void push(const std::string& str) {
      if (_ptrs.size() > _cStrs.size()) {
        // clear out the extra nullptr we have if asCharStar
        // was called prior to this
        _ptrs.erase(_ptrs.end() - 1);
      }

      _cStrs.push_back(str);
      _ptrs.push_back(&(*_cStrs.back()));
    }

    /**
     * returns the null-terminated ragged array of char*s
     * corresponding to the strings in this container
     */
    char** asCharStar() {
      _ptrs.push_back(nullptr);
      return &_ptrs[0];
    }

   private:
    std::vector<RaiiCharStar> _cStrs;
    std::vector<char*> _ptrs;
  };
}  // namespace subprocess
#endif

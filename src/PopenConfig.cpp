#include "subprocess/PopenConfig.hpp"

#ifdef _WIN32
#include <stdlib.h>  // _environ
#else
extern char** environ;
#endif

using namespace subprocess;

std::vector<EnvVar> PopenConfig::current_env() {
#ifdef _WIN32
  char** envPtr = _environ;
#else
  char** envPtr = environ;
#endif
  std::vector<EnvVar> envs;
  while (envPtr != nullptr && *envPtr != nullptr) {
    std::string var(*envPtr);
    envPtr++;
    auto split = var.find("=");
    if (split == std::string::npos) continue;
    envs.push_back(std::make_pair(var.substr(0, split), var.substr(split + 1, std::string::npos)));
  }
  return envs;
}

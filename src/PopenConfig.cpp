#include "subprocess/PopenConfig.hpp"

extern char **environ;

using namespace subprocess;

std::vector<EnvVar> PopenConfig::currentEnv() {
  char** envPtr = environ;
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

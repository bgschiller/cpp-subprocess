#include "subprocess/Redirection.hpp"

#include "subprocess/variant_helpers.hpp"

using namespace subprocess;
using namespace subprocess::internal;

Result<const std::nullopt_t> Redirection::match(
  std::function<Result<const std::nullopt_t>(const Pipe&)> pipe_case,
  std::function<Result<const std::nullopt_t>(const FileDescriptor&)> file_case,
  std::function<Result<const std::nullopt_t>(const Merge&)> merge_case,
  std::function<Result<const std::nullopt_t>()> none_case
) const {
  if (is_a<Pipe>()) return pipe_case(get<Pipe>());
  if (is_a<FileDescriptor>()) return file_case(get<FileDescriptor>());
  if (is_a<Merge>()) return merge_case(get<Merge>());
  if (is_a<None>()) return none_case();
  return std::nullopt;
}

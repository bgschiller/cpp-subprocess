#include "subprocess/Redirection.hpp"

#include <errno.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <share.h>
#else
#include <unistd.h>
#endif

#include "subprocess/posix.hpp"
#include "subprocess/variant_helpers.hpp"

using namespace subprocess;
using namespace subprocess::internal;

void Redirection::FileDescriptor::discard() {
  if (_owned) {
    subprocess::close_fd(fd);
    _owned = false;
  }
}
Redirection::FileDescriptor::FileDescriptor(int _fd)
    : fd{ _fd }
    , _owned{ true } { }
Redirection::FileDescriptor::~FileDescriptor() { discard(); }
Redirection::FileDescriptor::FileDescriptor(const FileDescriptor& other)
    : fd{ other.fd }
    , _owned{ false } { }
Redirection::FileDescriptor& Redirection::FileDescriptor::operator=(const FileDescriptor& other) {
  discard();
  fd = other.fd;
  _owned = false;
  return *this;
}

Redirection::FileDescriptor::FileDescriptor(FileDescriptor&& other)
    : fd{ other.fd }
    , _owned{ true } {
  other._owned = false;
}
Redirection::FileDescriptor& Redirection::FileDescriptor::operator=(FileDescriptor&& other) {
  discard();
  fd = other.fd;
  _owned = other._owned;
  other._owned = false;
  return *this;
}

Result<Redirection> Redirection::Open(
    const std::filesystem::path& path, int flags, mode_type mode) {
#ifdef _WIN32
  int fd = -1;
  errno = _sopen_s(&fd, path.string().c_str(), flags | _O_BINARY, _SH_DENYNO, mode);
#else
  int fd = ::open(path.c_str(), flags, mode);
#endif
  if (fd < 0) {
    return PopenError{ PopenError::ErrKind::IoError,
                       std::string("open(): ") + std::to_string(errno) + std::string(" ") +
                           subprocess::error_string(errno) };
  }
  return Redirection{ Redirection::FileDescriptor(fd) };
}

Result<Redirection> Redirection::Read(const std::filesystem::path& path) {
  return Open(path, O_RDONLY, 0);
}

Result<Redirection> Redirection::Write(const std::filesystem::path& path) {
  return Open(path.c_str(), O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
}

Result<Redirection> Redirection::Append(const std::filesystem::path& path) {
  return Open(path.c_str(), O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
}

Redirection::Redirection(Redirection&& other)
    : _state{ std::move(other._state) } { }

Redirection& Redirection::operator=(Redirection&& other) {
  _state = std::move(other._state);
  return *this;
}

std::string Redirection::toString() const { return internal::variant_to_string(_state); }

Result<const std::nullopt_t> Redirection::match(
    std::function<Result<const std::nullopt_t>(const Pipe&)> pipe_case,
    std::function<Result<const std::nullopt_t>(const FileDescriptor&)> file_case,
    std::function<Result<const std::nullopt_t>(const Merge&)> merge_case,
    std::function<Result<const std::nullopt_t>()> none_case) const {
  if (is_a<Pipe>()) return pipe_case(get<Pipe>());
  if (is_a<FileDescriptor>()) return file_case(get<FileDescriptor>());
  if (is_a<Merge>()) return merge_case(get<Merge>());
  if (is_a<None>()) return none_case();
  return std::nullopt;
}

#include "subprocess/Exec.hpp"

#include "subprocess/Popen.hpp"

namespace subprocess {
  Exec::Exec(
      std::string _command, std::vector<std::string> _args, PopenConfig _config,
      std::optional<std::vector<uint8_t>> _stdin_data)
      : command{ std::move(_command) }
      , args{ std::move(_args) }
      , config{ std::move(_config) }
      , stdin_data{ std::move(_stdin_data) } { }

  Exec Exec::cmd(std::string command) {
    Exec exec{ std::move(command), std::vector<std::string>{}, PopenConfig{}, std::nullopt };
    return exec;
  }

  Exec Exec::shell(std::string cmdstr) {
#ifdef _WIN32
    return Exec::cmd("cmd.exe").arg("/c").add_args({ std::move(cmdstr) });
#else
    return Exec::cmd("sh").arg("-c").add_args({ std::move(cmdstr) });
#endif
  }

  Exec& Exec::arg(std::string arg) & {
    args.push_back(std::move(arg));
    return *this;
  }
  Exec&& Exec::arg(std::string arg) && { return std::move(this->arg(std::move(arg))); }

  Exec& Exec::add_args(std::vector<std::string> new_args) & {
    this->args.insert(
        this->args.end(), std::make_move_iterator(new_args.begin()),
        std::make_move_iterator(new_args.end()));
    return *this;
  }
  Exec&& Exec::add_args(std::vector<std::string> new_args) && {
    return std::move(this->add_args(std::move(new_args)));
  }

  Exec& Exec::detached() & {
    config.detached = true;
    return *this;
  }
  Exec&& Exec::detached() && { return std::move(this->detached()); }

  Exec& Exec::env_clear() & {
    config.env = std::vector<EnvVar>{};
    return *this;
  }
  Exec&& Exec::env_clear() && { return std::move(this->env_clear()); }

  void Exec::ensure_env() {
    if (!config.env) {
      config.env = PopenConfig::current_env();
    }
  }

  Exec& Exec::env(const std::string& key, const std::string& value) & {
    ensure_env();
    config.env->push_back(std::make_pair(key, value));
    return *this;
  }
  Exec&& Exec::env(const std::string& key, const std::string& value) && {
    return std::move(this->env(key, value));
  }

  Exec& Exec::env_extend(const std::vector<EnvVar>& vars) & {
    ensure_env();
    config.env->insert(
        config.env->end(), std::make_move_iterator(vars.begin()),
        std::make_move_iterator(vars.end()));
    return *this;
  }
  Exec&& Exec::env_extend(const std::vector<EnvVar>& vars) && {
    return std::move(this->env_extend(vars));
  }

  Exec& Exec::env_remove(const std::string& key) & {
    ensure_env();
    config.env->erase(
        std::remove_if(
            config.env->begin(), config.env->end(),
            [&key](const EnvVar& var) { return var.first == key; }),
        config.env->end());
    return *this;
  }
  Exec&& Exec::env_remove(const std::string& key) && { return std::move(this->env_remove(key)); }

  Exec& Exec::cwd(const std::filesystem::path& dir) & {
    config.cwd = dir;
    return *this;
  }
  Exec&& Exec::cwd(const std::filesystem::path& dir) && { return std::move(this->cwd(dir)); }

  Exec& Exec::stdin_(const std::vector<uint8_t>& data) & {
    if (!config.stdin_.is_a<Redirection::None>()) {
      throw std::runtime_error("stdin is already set");
    }
    config.stdin_ = Redirection::Pipe();
    stdin_data = data;
    return *this;
  }
  Exec&& Exec::stdin_(const std::vector<uint8_t>& data) && { return std::move(this->stdin_(data)); }

  Exec& Exec::stdin_(const std::string& data) & {
    if (!config.stdin_.is_a<Redirection::None>()) {
      throw std::runtime_error("stdin is already set");
    }
    config.stdin_ = Redirection::Pipe();
    stdin_data = std::vector<uint8_t>(data.begin(), data.end());
    return *this;
  }
  Exec&& Exec::stdin_(const std::string& data) && { return std::move(this->stdin_(data)); }

  Exec& Exec::stdin_(Redirection redirection) & {
    bool pipeReplacingPipe =
        config.stdin_.is_a<Redirection::Pipe>() && redirection.is_a<Redirection::Pipe>();
    if (!config.stdin_.is_a<Redirection::None>() && !pipeReplacingPipe) {
      throw std::runtime_error("stdin is already set");
    }
    config.stdin_ = std::move(redirection);
    return *this;
  }
  Exec&& Exec::stdin_(Redirection redirection) && {
    return std::move(this->stdin_(std::move(redirection)));
  }

  Exec& Exec::stdin_(NullFile) & {
    if (!config.stdin_.is_a<Redirection::None>()) {
      throw std::runtime_error("stdin is already set");
    }
#ifdef _WIN32
    config.stdin_ = Redirection::Read("NUL").or_throw();
#else
    config.stdin_ = Redirection::Read("/dev/null").or_throw();
#endif
    return *this;
  }
  Exec&& Exec::stdin_(NullFile nf) && { return std::move(this->stdin_(nf)); }

  Exec& Exec::stdout_(Redirection redirection) & {
    bool pipeReplacingPipe =
        config.stdout_.is_a<Redirection::Pipe>() && redirection.is_a<Redirection::Pipe>();
    if (!config.stdout_.is_a<Redirection::None>() && !pipeReplacingPipe) {
      throw std::runtime_error("stdout is already set");
    }
    config.stdout_ = std::move(redirection);
    return *this;
  }
  Exec&& Exec::stdout_(Redirection redirection) && {
    return std::move(this->stdout_(std::move(redirection)));
  }

  Exec& Exec::stdout_(NullFile) & {
    if (!config.stdout_.is_a<Redirection::None>()) {
      throw std::runtime_error("stdout is already set");
    }
#ifdef _WIN32
    config.stdout_ = Redirection::Write("NUL").or_throw();
#else
    config.stdout_ = Redirection::Write("/dev/null").or_throw();
#endif
    return *this;
  }
  Exec&& Exec::stdout_(NullFile nf) && { return std::move(this->stdout_(nf)); }

  Exec& Exec::stderr_(Redirection redirection) & {
    bool pipeReplacingPipe =
        config.stderr_.is_a<Redirection::Pipe>() && redirection.is_a<Redirection::Pipe>();
    if (!config.stderr_.is_a<Redirection::None>() && !pipeReplacingPipe) {
      throw std::runtime_error("stderr is already set");
    }
    config.stderr_ = std::move(redirection);
    return *this;
  }
  Exec&& Exec::stderr_(Redirection redirection) && {
    return std::move(this->stderr_(std::move(redirection)));
  }

  Exec& Exec::stderr_(NullFile) & {
    if (!config.stderr_.is_a<Redirection::None>()) {
      throw std::runtime_error("stderr is already set");
    }
#ifdef _WIN32
    config.stderr_ = Redirection::Write("NUL").or_throw();
#else
    config.stderr_ = Redirection::Write("/dev/null").or_throw();
#endif
    return *this;
  }
  Exec&& Exec::stderr_(NullFile nf) && { return std::move(this->stderr_(nf)); }

  Result<Popen> Exec::popen() {
    // Build argv: command is argv[0], followed by any extra args.
    std::vector<std::string> argv;
    argv.reserve(1 + args.size());
    argv.push_back(command);
    argv.insert(argv.end(), args.begin(), args.end());

    return Popen::create(argv, config);
  }

  Result<ExitStatus> Exec::join() {
    auto p = popen();
    if (!p.ok()) return p.take_error();
    return p.take_value().wait();
  }

  Result<boost::fdistream> Exec::stream_stdout() {
    if (config.stdout_.is_a<Redirection::None>()) {
      config.stdout_ = Redirection::Pipe();
    }
    auto p = popen();
    if (!p.ok()) return p.take_error();
    auto proc = p.take_value();
    if (!proc.std_out.has_value()) {
      return PopenError{ PopenError::LogicError,
                         "stream_stdout: stdout must be configured as Redirection::Pipe" };
    }
    // Detach so the destructor does not block in wait() while the caller
    // has not yet started reading from the stream.
    proc.detached = true;
    return std::move(*proc.std_out);
  }

  Result<CaptureData> Exec::capture() {
    if (config.stdout_.is_a<Redirection::None>()) {
      config.stdout_ = Redirection::Pipe();
    }
    if (config.stderr_.is_a<Redirection::None>()) {
      config.stderr_ = Redirection::Pipe();
    }

    auto p = popen();
    if (!p.ok()) return p.take_error();
    auto proc = p.take_value();

    return proc.communicate_bytes(std::move(stdin_data));
  }

  Result<boost::fdostream> Exec::stream_stdin() {
    if (config.stdin_.is_a<Redirection::None>()) {
      config.stdin_ = Redirection::Pipe();
    }
    auto p = popen();
    if (!p.ok()) return p.take_error();
    auto proc = p.take_value();
    if (!proc.std_in.has_value()) {
      return PopenError{ PopenError::LogicError,
                         "stream_stdin: stdin must be configured as Redirection::Pipe" };
    }
    // Detach so the destructor does not block in wait() while the caller
    // has not yet finished writing to the stream.
    proc.detached = true;
    return std::move(*proc.std_in);
  }
  void Exec::release_redirection_fds() {
    config.stdin_.release_internal_fds();
    config.stdout_.release_internal_fds();
    config.stderr_.release_internal_fds();
  }
}  // namespace subprocess

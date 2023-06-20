#include "subprocess/Exec.hpp"

using namespace subprocess;

Exec::Exec(
  std::string _command,
  std::vector<std::string> _args,
  PopenConfig _config,
  std::optional<std::vector<uint8_t>> _stdin_data
) : command{std::move(_command)},
    args{std::move(_args)},
    config{std::move(_config)},
    stdin_data{std::move(_stdin_data)}
{ }

Exec Exec::cmd(std::string command) {
  Exec exec{
    std::move(command),
    std::vector<std::string>{},
    PopenConfig{},
    std::nullopt
  };
  return exec;
}

Exec& Exec::arg(std::string arg) {
  args.push_back(std::move(arg));
  return *this;
}

Exec& Exec::args(std::vector<std::string> args) {
  this->args.insert(
    this->args.end(),
    std::make_move_iterator(args.begin()),
    std::make_move_iterator(args.end())
  );
  return *this;
}

Exec& Exec::detached() {
  config.detached = true;
  return *this;
}

Exec& Exec::env_clear() {
  config.env = std::vector<EnvVar>{};
  return *this;
}

void Exec::ensure_env() {
  if (!config.env) {
    config.env = PopenConfig::currentEnv();
  }
}

Exec& Exec::env(const std::string& key, const std::string& value) {
  ensure_env();
  config.env->push_back(std::make_pair(key, value));
  return *this;
}

Exec& Exec::env_extend(const std::vector<EnvVar>& vars) {
  ensure_env();
  config.env->insert(
    config.env->end(),
    std::make_move_iterator(env.begin()),
    std::make_move_iterator(env.end())
  );
  return *this;
}

Exec& Exec::env_remove(const std::string& key) {
  ensure_env();
  config.env->erase(
    std::remove_if(
      config.env->begin(),
      config.env->end(),
      [&key](const EnvVar& var) { return var.first == key; }
    ),
    config.env->end()
  );
  return *this;
}

Exec& Exec::cwd(const std::string& dir) {
  config.cwd = dir;
  return *this;
}

Exec& Exec::stdin(const std::vector<uint8_t>& data) {
  if (!config.stdin.is_a<Redirection::None>()) {
    throw std::runtime_error("stdin is already set");
  }
  config.stdin = Redirection::Pipe();
  stdin_data = data;
  return *this;
}

Exec& Exec::stdin(const std::string& data) {
  if (config.stdin.is_a<Redirection::None>()) {
    throw std::runtime_error("stdin is already set");
  }
  config.stdin = Redirection::Pipe();
  stdin_data = std::vector<uint8_t>(data.begin(), data.end());
  return *this;
}

Exec& Exec::stdin(Redirection redirection) {
  bool pipeReplacingPipe = config.stdin.is_a<Redirection::Pipe>() && redirection.is_a<Redirection::Pipe>();
  if (!config.stdin.is_a<Redirection::None>() && !pipeReplacingPipe){
    throw std::runtime_error("stdin is already set");
  }
  config.stdin = redirection;
  return *this;
}

Exec& Exec::stdin(NullFile) {
  if (!config.stdin.is_a<Redirection::None>()) {
    throw std::runtime_error("stdin is already set");
  }
  config.stdin = Redirection::Read("/dev/null");
  return *this;
}

Exec& Exec::stdout(Redirection redirection) {
  bool pipeReplacingPipe = config.stdout.is_a<Redirection::Pipe>() && redirection.is_a<Redirection::Pipe>();
  if (!config.stdout.is_a<Redirection::None>() && !pipeReplacingPipe) {
    throw std::runtime_error("stdout is already set");
  }
  config.stdout = redirection;
  return *this;
}

Exec& Exec::stdout(NullFile) {
  if (!config.stdout.is_a<Redirection::None>()) {
    throw std::runtime_error("stdout is already set");
  }
  config.stdout = Redirection::Write("/dev/null");
  return *this;
}

Exec& Exec::stderr(Redirection redirection) {
  bool pipeReplacingPipe = config.stderr.is_a<Redirection::Pipe>() && redirection.is_a<Redirection::Pipe>();
  if (!config.stderr.is_a<Redirection::None>() && !pipeReplacingPipe) {
    throw std::runtime_error("stderr is already set");
  }
  config.stderr = redirection;
  return *this;
}

Exec& Exec::stderr(NullFile) {
  if (!config.stderr.is_a<Redirection::None>()) {
    throw std::runtime_error("stderr is already set");
  }
  config.stderr = Redirection::Write("/dev/null");
  return *this;
}

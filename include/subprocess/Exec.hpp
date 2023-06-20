#ifndef SUBPROCESS_EXEC_H_
#define SUBPROCESS_EXEC_H_

#include <optional>
#include <string>

#include "PopenConfig.hpp"
#include "Redirection.hpp"

namespace subprocess {

  /// Marker value for [`stdin`], [`stdout`], and [`stderr`] methods
  /// of [`Exec`] and [`Pipeline`].
  ///
  /// Use of this value means that the corresponding stream should
  /// be redirected to the devnull device.
  ///
  /// [`stdin`]: struct.Exec.html#method.stdin
  /// [`stdout`]: struct.Exec.html#method.stdout
  /// [`stderr`]: struct.Exec.html#method.stderr
  /// [`Exec`]: struct.Exec.html
  /// [`Pipeline`]: struct.Pipeline.html
  struct NullFile {};

  class Exec {
  private:
    Exec() = delete;
    Exec(
      std::string command,
      std::vector<std::string> args,
      PopenConfig config,
      std::optional<std::vector<uint8_t>> stdin_data
    );
    std::string command;
    std::vector<std::string> args;
    PopenConfig config;
    std::optional<std::vector<uint8_t>> stdin_data;
  public:
    /// Constructs a new `Exec`, configured to run `command`.
    ///
    /// The command will be run directly in the OS, without an
    /// intervening shell.  To run it through a shell, use
    /// [`Exec::shell`] instead.
    ///
    /// By default, the command will be run without arguments, and
    /// none of the standard streams will be modified.
    ///
    /// [`Exec::shell`]: struct.Exec.html#method.shell
    static Exec cmd(std::string command);


    /// Constructs a new `Exec`, configured to run `cmdstr` with
    /// the system shell.
    ///
    /// `subprocess` never spawns shells without an explicit
    /// request.  This command requests the shell to be used; on
    /// Unix-like systems, this is equivalent to
    /// `Exec::cmd("sh").arg("-c").arg(cmdstr)`.  On Windows, it
    /// runs `Exec::cmd("cmd.exe").arg("/c")`.
    ///
    /// `shell` is useful for porting code that uses the C
    /// `system` function, which also spawns a shell.
    ///
    /// When invoking this function, be careful not to interpolate
    /// arguments into the string run by the shell, such as
    /// `Exec::shell(std::string("sort ") + filename)`.  Such code is
    /// prone to errors and, if `filename` comes from an untrusted
    /// source, to shell injection attacks.  Instead, use
    /// `Exec::cmd("sort").arg(filename)`.
    // static Exec shell(std::string cmdstr);


    /// Appends `arg` to argument list.
    Exec& arg(std::string arg);

    /// Extends the argument list with `args`.
    Exec& args(std::vector<std::string> args);

    /// Specifies that the process is initially detached.
    ///
    /// A detached process means that we will not wait for the
    /// process to finish when the object that owns it goes out of
    /// scope.
    Exec& detached();

private:
    void ensure_env();

public:
    /// Clears the environment of the subprocess.
    ///
    /// When this is invoked, the subprocess will not inherit the
    /// environment of this process.
    Exec& env_clear();

    /// Sets an environment variable in the child process.
    ///
    /// If the same variable is set more than once, the last value
    /// is used.
    ///
    /// Other environment variables are by default inherited from
    /// the current process.  If this is undesirable, call
    /// `env_clear` first.
    Exec& env(const std::string& key, const std::string& value);


    /// Sets multiple environment variables in the child process.
    ///
    /// The keys and values of the variables are specified by the
    /// slice.  If the same variable is set more than once, the
    /// last value is used.
    ///
    /// Other environment variables are by default inherited from
    /// the current process.  If this is undesirable, call
    /// `env_clear` first.
    Exec& env_extend(const std::vector<EnvVar>& env);

    /// Specifies the current working directory of the child process.
    ///
    /// If unspecified, the current working directory is inherited
    /// from the parent.
    Exec& cwd(const std::filesystem::path& dir);

    /// Specifies how to set up the standard input of the child process.
    ///
    /// Argument can be:
    ///
    /// * a [`Redirection`];
    /// * a `const std::vector<uint8_t>&` or `const std::string&`, which will set up a
    ///   `Redirection::Pipe` for stdin, making sure that `capture` feeds that data
    ///   into the standard input of the subprocess;
    /// * [`NullFile`], which will redirect the standard input to read from
    ///    `/dev/null`.
    ///
    /// [`Redirection`]: enum.Redirection.html
    /// [`NullFile`]: struct.NullFile.html
    Exec& stdin(Redirection capture);
    Exec& stdin(const std::vector<uint8_t>& data);
    Exec& stdin(const std::string& data);
    Exec& stdin(NullFile);

    /// Specifies how to set up the standard output of the child process.
    ///
    /// Argument can be:
    ///
    /// * a [`Redirection`];
    /// * [`NullFile`], which will redirect the standard output to go to
    ///    `/dev/null`.
    ///
    /// [`Redirection`]: enum.Redirection.html
    /// [`NullFile`]: struct.NullFile.html
    Exec& stdout(Redirection capture);
    Exec& stdout(NullFile);

    /// Specifies how to set up the standard error of the child process.
    ///
    /// Argument can be:
    ///
    /// * a [`Redirection`];
    /// * [`NullFile`], which will redirect the standard error to go to
    ///    `/dev/null`.
    ///
    /// [`Redirection`]: enum.Redirection.html
    /// [`NullFile`]: struct.NullFile.html
    Exec& stderr(Redirection capture);
    Exec& stderr(NullFile);

  };
}

#endif

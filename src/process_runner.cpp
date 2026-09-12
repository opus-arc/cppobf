#include "process_runner.h"

#include <cerrno>
#include <cstring>
#include <spawn.h>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace cppobf {

ProcessResult ProcessRunner::Run(const std::vector<std::string>& arguments) {
  if (arguments.empty()) {
    throw std::invalid_argument("Cannot run an empty command.");
  }

  int pipe_fds[2];
  if (pipe(pipe_fds) != 0) {
    throw std::runtime_error("pipe failed: " + std::string(std::strerror(errno)));
  }

  posix_spawn_file_actions_t actions;
  if (posix_spawn_file_actions_init(&actions) != 0) {
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    throw std::runtime_error("posix_spawn_file_actions_init failed.");
  }
  posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDERR_FILENO);
  posix_spawn_file_actions_addclose(&actions, pipe_fds[0]);
  posix_spawn_file_actions_addclose(&actions, pipe_fds[1]);

  std::vector<char*> argv;
  argv.reserve(arguments.size() + 1);
  for (const std::string& argument : arguments) {
    argv.push_back(const_cast<char*>(argument.c_str()));
  }
  argv.push_back(nullptr);

  pid_t pid = 0;
  const int spawn_error =
      posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  close(pipe_fds[1]);
  if (spawn_error != 0) {
    close(pipe_fds[0]);
    throw std::runtime_error("Failed to start " + arguments.front() + ": " +
                             std::string(std::strerror(spawn_error)));
  }

  std::string output;
  char buffer[8192];
  while (true) {
    const ssize_t count = read(pipe_fds[0], buffer, sizeof(buffer));
    if (count > 0) {
      output.append(buffer, static_cast<std::size_t>(count));
      continue;
    }
    if (count == 0) break;
    if (errno != EINTR) {
      close(pipe_fds[0]);
      throw std::runtime_error("Failed while reading child output: " +
                               std::string(std::strerror(errno)));
    }
  }
  close(pipe_fds[0]);

  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) {
      throw std::runtime_error("waitpid failed: " +
                               std::string(std::strerror(errno)));
    }
  }

  ProcessResult result;
  result.output = std::move(output);
  if (WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exit_code = 128 + WTERMSIG(status);
  }
  return result;
}

}  // namespace cppobf

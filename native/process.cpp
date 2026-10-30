#include "core.hpp"
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <spawn.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <cerrno>
extern char** environ;
#endif
namespace kz {
void Engine::cancel() {
  cancelled_ = true; std::lock_guard<std::mutex> lock(processMutex_);
  if (!process_) return;
#ifdef _WIN32
  TerminateProcess(reinterpret_cast<HANDLE>(process_), 255);
#else
  kill(static_cast<pid_t>(process_), SIGTERM);
#endif
}
std::string Engine::run(const std::vector<std::string>& args) {
  if (cancelled_) throw std::runtime_error("Operation cancelled.");
  std::string output; bool overflow = false; constexpr size_t cap = 32 * 1024 * 1024;
  int code = -1;
#ifdef _WIN32
  struct Handle { HANDLE value = nullptr; ~Handle(){ if(value && value != INVALID_HANDLE_VALUE) CloseHandle(value); } } read, write, input, process, thread;
  SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  if (!CreatePipe(&read.value, &write.value, &security, 0)) throw std::runtime_error("Cannot create engine pipe.");
  SetHandleInformation(read.value, HANDLE_FLAG_INHERIT, 0);
  input.value = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
  if (input.value == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create engine input.");
  std::wstring command = quoteWindows(executable_.wstring());
  for (const auto& arg : args) command += L" " + quoteWindows(widen(arg));
  STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = input.value; startup.hStdOutput = startup.hStdError = write.value;
  PROCESS_INFORMATION info{};
  {
    std::lock_guard<std::mutex> lock(processMutex_);
    if (cancelled_) throw std::runtime_error("Operation cancelled.");
    if (!CreateProcessW(executable_.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &info)) throw std::runtime_error("Cannot start 7-Zip. Select the official full engine.");
    process.value = info.hProcess; thread.value = info.hThread; process_ = reinterpret_cast<intptr_t>(info.hProcess);
  }
  CloseHandle(write.value); write.value = nullptr;
  char buffer[8192]; DWORD count;
  while (ReadFile(read.value, buffer, sizeof(buffer), &count, nullptr) && count) {
    if (output.size() + count > cap) { overflow = true; cancel(); break; }
    output.append(buffer, count);
  }
  WaitForSingleObject(process.value, INFINITE);
  { std::lock_guard<std::mutex> lock(processMutex_); process_ = 0; }
  DWORD exitCode; GetExitCodeProcess(process.value, &exitCode); code = static_cast<int>(exitCode);
#else
  int pipes[2]; if (pipe(pipes)) throw std::runtime_error("Cannot create engine pipe.");
  fcntl(pipes[0], F_SETFD, FD_CLOEXEC); fcntl(pipes[1], F_SETFD, FD_CLOEXEC);
  posix_spawn_file_actions_t actions; posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
  posix_spawn_file_actions_adddup2(&actions, pipes[1], STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions, pipes[1], STDERR_FILENO);
  posix_spawn_file_actions_addclose(&actions, pipes[0]); posix_spawn_file_actions_addclose(&actions, pipes[1]);
  std::vector<std::string> strings{executable_.string()}; strings.insert(strings.end(), args.begin(), args.end());
  std::vector<char*> argv; for (auto& arg : strings) argv.push_back(arg.data()); argv.push_back(nullptr);
  pid_t pid = 0; int result;
  { std::lock_guard<std::mutex> lock(processMutex_);
    result = cancelled_ ? ECANCELED : posix_spawn(&pid, executable_.c_str(), &actions, nullptr, argv.data(), environ);
    if (!result) process_ = pid;
  }
  posix_spawn_file_actions_destroy(&actions); close(pipes[1]);
  if (result) { close(pipes[0]); throw std::runtime_error("Cannot start 7zz. Select the official engine."); }
  char buffer[8192]; ssize_t count;
  while ((count = read(pipes[0], buffer, sizeof(buffer))) != 0) {
    if (count < 0) { if (errno == EINTR) continue; break; }
    if (output.size() + static_cast<size_t>(count) > cap) { overflow = true; cancel(); break; }
    output.append(buffer, static_cast<size_t>(count));
  }
  close(pipes[0]); int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
  { std::lock_guard<std::mutex> lock(processMutex_); process_ = 0; }
  if (WIFEXITED(status)) code = WEXITSTATUS(status);
#endif
  if (overflow) throw std::runtime_error("Engine output exceeded safety limit.");
  if (cancelled_) throw std::runtime_error("Operation cancelled.");
  if (code != 0) throw std::runtime_error("7-Zip error " + std::to_string(code) + ". Check password, format and archive integrity.");
  return output;
}
}

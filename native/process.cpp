#include "core.hpp"
#include <stdexcept>
#include <algorithm>
#include <cctype>
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
std::string Engine::run(const std::vector<std::string>& args,const fs::path& program,int operation,const fs::path& workingDirectory) {
  const auto executable=program.empty()?executable_:program;
  if (cancelled_) throw std::runtime_error("Operation cancelled.");
  progress_=-1;phase_=operation?operation:args.empty()?0:args[0]=="a"?1:args[0]=="x"?2:args[0]=="t"?3:4;
  std::string progressLine;
  auto consume=[&](const char* bytes,size_t count){
    for(size_t i=0;i<count;++i){
      char c=bytes[i];
      if(c=='\r'||c=='\n'||c=='\b'){progressLine.clear();continue;}
      if(progressLine.size()<16)progressLine+=c;
      if(c=='%'){
        size_t start=progressLine.find_first_not_of(' ');
        if(start==std::string::npos)continue;
        int n=0;bool valid=true;
        for(size_t j=start;j+1<progressLine.size();++j){char d=progressLine[j];if(d<'0'||d>'9'){valid=false;break;}n=n*10+d-'0';}
        if(valid&&n<=100)progress_=n;
      }
    }
  };
  std::string output; bool overflow = false; constexpr size_t cap = 32 * 1024 * 1024;
  int code = -1;
#ifdef _WIN32
  struct Handle { HANDLE value = nullptr; ~Handle(){ if(value && value != INVALID_HANDLE_VALUE) CloseHandle(value); } } read, write, input, process, thread;
  SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  if (!CreatePipe(&read.value, &write.value, &security, 0)) throw std::runtime_error("Cannot create engine pipe.");
  SetHandleInformation(read.value, HANDLE_FLAG_INHERIT, 0);
  input.value = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
  if (input.value == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create engine input.");
  std::wstring command = quoteWindows(executable.wstring());
  for (const auto& arg : args) command += L" " + quoteWindows(widen(arg));
  STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = input.value; startup.hStdOutput = startup.hStdError = write.value;
  PROCESS_INFORMATION info{};
  {
    std::lock_guard<std::mutex> lock(processMutex_);
    if (cancelled_) throw std::runtime_error("Operation cancelled.");
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, workingDirectory.empty()?nullptr:workingDirectory.c_str(), &startup, &info)) throw std::runtime_error("Cannot start archive engine. Check that its executable is installed.");
    process.value = info.hProcess; thread.value = info.hThread; process_ = reinterpret_cast<intptr_t>(info.hProcess);
  }
  CloseHandle(write.value); write.value = nullptr;
  char buffer[8192]; DWORD count;
  while (ReadFile(read.value, buffer, sizeof(buffer), &count, nullptr) && count) {
    consume(buffer,count);
    if(phase_!=4&&output.size()>65536)output.erase(0,output.size()-32768);
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
#ifdef __APPLE__
  if(!workingDirectory.empty()&&posix_spawn_file_actions_addchdir_np(&actions,workingDirectory.c_str())){
    posix_spawn_file_actions_destroy(&actions);close(pipes[0]);close(pipes[1]);throw std::runtime_error("Cannot set engine directory.");
  }
#else
  if(!workingDirectory.empty()){posix_spawn_file_actions_destroy(&actions);close(pipes[0]);close(pipes[1]);throw std::runtime_error("Working directory unsupported on this platform.");}
#endif
  posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
  posix_spawn_file_actions_adddup2(&actions, pipes[1], STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions, pipes[1], STDERR_FILENO);
  posix_spawn_file_actions_addclose(&actions, pipes[0]); posix_spawn_file_actions_addclose(&actions, pipes[1]);
  std::vector<std::string> strings{executable.string()}; strings.insert(strings.end(), args.begin(), args.end());
  std::vector<char*> argv; for (auto& arg : strings) argv.push_back(arg.data()); argv.push_back(nullptr);
  pid_t pid = 0; int result;
  { std::lock_guard<std::mutex> lock(processMutex_);
    result = cancelled_ ? ECANCELED : posix_spawn(&pid, executable.c_str(), &actions, nullptr, argv.data(), environ);
    if (!result) process_ = pid;
  }
  posix_spawn_file_actions_destroy(&actions); close(pipes[1]);
  if (result) { close(pipes[0]); throw std::runtime_error("Cannot start archive engine. Check that its executable is installed."); }
  char buffer[8192]; ssize_t count;
  while ((count = read(pipes[0], buffer, sizeof(buffer))) != 0) {
    if (count < 0) { if (errno == EINTR) continue; break; }
    consume(buffer,static_cast<size_t>(count));
    if(phase_!=4&&output.size()>65536)output.erase(0,output.size()-32768);
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
  if(code!=0&&executable.filename()==fs::path("MpCmdRun.exe"))throw std::runtime_error("Microsoft Defender가 탐지 또는 검사 오류를 보고했습니다 (코드 "+std::to_string(code)+"). Windows 보안의 검사 결과를 확인하세요. 파일 자동 조치는 요청하지 않았습니다.");
  if(code!=0){
    auto lower=output;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    for(const auto& message:{"wrong password","incorrect password","invalid password","password required","password is incorrect","enter password"})
      if(lower.find(message)!=std::string::npos)throw PasswordRequired("암호가 필요하거나 입력한 암호가 올바르지 않습니다.");
  }
  if (code != 0) throw std::runtime_error( "Archive engine error " + std::to_string(code) + ". Check password, format and archive integrity.");
  progress_=100;return output;
}
}

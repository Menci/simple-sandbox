#include <string>
#include <iostream>
#include <functional>
#include <system_error>
#include <vector>
#include <stdexcept>
#include <memory>
#include <mutex>

#include <cstring>
#include <cassert>

#include <filesystem>

#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <syscall.h>
#include <grp.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include "sandbox.h"
#include "utils.h"
#include "cgroup.h"
#include "semaphore.h"
#include "pipe.h"

namespace fs = std::filesystem;
using std::string;
using std::vector;
using fmt::format;

// Make sure fd 0,1,2 exists.
static void RedirectIO(const SandboxParameter &param, int nullfd)
{
    const string &std_input = param.stdinRedirection,
                 std_output = param.stdoutRedirection,
                 std_error = param.stderrRedirection;

    int inputfd, outputfd, errorfd;
    if (param.stdinRedirectionFileDescriptor == -1)
    {
        if (std_input != "")
        {
            inputfd = ENSURE(open(std_input.c_str(), O_RDONLY));
        }
        else
        {
            inputfd = nullfd;
        }
    }
    else
    {
        inputfd = param.stdinRedirectionFileDescriptor;
    }
    ENSURE(dup2(inputfd, STDIN_FILENO));

    if (param.stdoutRedirectionFileDescriptor == -1)
    {
        if (std_output != "")
        {
            outputfd = ENSURE(open(std_output.c_str(), O_WRONLY | O_TRUNC | O_CREAT,
                                   S_IWUSR | S_IRUSR | S_IRGRP | S_IWGRP));
        }
        else
        {
            outputfd = nullfd;
        }
    }
    else
    {
        outputfd = param.stdoutRedirectionFileDescriptor;
    }
    ENSURE(dup2(outputfd, STDOUT_FILENO));

    if (param.stderrRedirectionFileDescriptor == -1)
    {
        if (std_error != "")
        {
            if (std_error == std_output)
            {
                errorfd = outputfd;
            }
            else
            {
                errorfd = ENSURE(open(std_error.c_str(), O_WRONLY | O_TRUNC | O_CREAT,
                                      S_IWUSR | S_IRUSR | S_IRGRP | S_IWGRP));
            }
        }
        else
        {
            errorfd = nullfd;
        }
    }
    else
    {
        errorfd = param.stderrRedirectionFileDescriptor;
    }
    ENSURE(dup2(errorfd, STDERR_FILENO));
}

struct ExecutionParameter
{
    const SandboxParameter &parameter;

    PosixSemaphore semaphore1, semaphore2;
    // This pipe is used to forward error message from the child process to the parent.
    PosixPipe pipefd;

    ExecutionParameter(const SandboxParameter &param, int pipeOptions) : parameter(param),
                                                                         semaphore1(true, 0),
                                                                         semaphore2(true, 0),
                                                                         pipefd(pipeOptions)
    {
    }
};

static void EnsureDirectoryExistance(fs::path dir) {
    if (!fs::exists(dir))
    {
        throw std::runtime_error((format("The specified path {} does not exist.", dir)));
    }
    if (!fs::is_directory(dir))
    {
        throw std::runtime_error((format("The specified path {} exists, but is not a directory.", dir)));
    }
}

void GetUserEntryInSandbox(const fs::path &rootfs, const std::string username, std::vector<char> &dataBuffer, passwd &entry) {
    auto passwdFilePath = rootfs / "etc" / "passwd";
    std::unique_ptr<FILE, decltype(&fclose)> passwdFile(fopen(passwdFilePath.c_str(), "r"), &fclose);
    if (passwdFile == nullptr)
        throw std::system_error(errno, std::system_category(), "Couldn't open /etc/passwd in rootfs");

    long passwdBufferSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (passwdBufferSize == -1) passwdBufferSize = 16384;
    dataBuffer.resize(passwdBufferSize);

    passwd *user = nullptr;
    while (fgetpwent_r(passwdFile.get(), &entry, dataBuffer.data(), passwdBufferSize, &user) == 0)
        if (username == user->pw_name)
            break;

    if (user == nullptr)
        if (errno == ENOENT)
            throw std::invalid_argument(format("No such user: {}", username));
        else
            throw std::system_error(errno, std::system_category(), "fgetpwent_r");
}

static int ChildProcess(void *param_ptr)
{
    ExecutionParameter &execParam = *reinterpret_cast<ExecutionParameter *>(param_ptr);
    // We obtain a full copy of parameters here. The arguments may be destoryed after some time.
    SandboxParameter parameter = execParam.parameter;

    try
    {
        ENSURE(close(execParam.pipefd[0]));

        int nullfd = ENSURE(open("/dev/null", O_RDWR));
        if (parameter.redirectBeforeChroot)
        {
            RedirectIO(parameter, nullfd);
        }

        ENSURE(mount("none", "/", NULL, MS_REC | MS_PRIVATE, NULL)); // Make root private

        EnsureDirectoryExistance(parameter.chrootDirectory);
        ENSURE(mount(parameter.chrootDirectory.string().c_str(),
                     parameter.chrootDirectory.string().c_str(), "", MS_BIND | MS_RDONLY | MS_REC, ""));
        ENSURE(mount("", parameter.chrootDirectory.string().c_str(), "", MS_BIND | MS_REMOUNT | MS_RDONLY | MS_REC, ""));

        for (MountInfo &info : parameter.mounts)
        {
            if (!info.dst.is_absolute()) {
                throw std::invalid_argument(format("The dst path {} in mounts should be absolute.", info.dst));
            }

            fs::path target = parameter.chrootDirectory / std::filesystem::relative(info.dst, "/");
	    
            EnsureDirectoryExistance(info.src);
            EnsureDirectoryExistance(target);
            ENSURE(mount(info.src.string().c_str(), target.string().c_str(), "", MS_BIND | MS_REC, ""));
            if (info.limit == 0)
            {
                ENSURE(mount("", target.string().c_str(), "", MS_BIND | MS_REMOUNT | MS_RDONLY | MS_REC, ""));
            }
            else if (info.limit != -1)
            {
                // TODO: implement.
            }
        }

        ENSURE(chroot(parameter.chrootDirectory.string().c_str()));
        ENSURE(chdir(parameter.workingDirectory.string().c_str()));

        if (parameter.mountProc)
        {
            ENSURE(mount("proc", "/proc", "proc", 0, NULL));
        }
        if (!parameter.redirectBeforeChroot)
        {
            RedirectIO(parameter, nullfd);
        }

        if (!parameter.hostname.empty()) {
            ENSURE(sethostname(parameter.hostname.c_str(), parameter.hostname.length()));
        }

        if (parameter.stackSize != -2)
        {
            rlimit64 rlim;
            rlim.rlim_max = rlim.rlim_cur = parameter.stackSize != -1 ? parameter.stackSize : RLIM64_INFINITY;
            ENSURE(setrlimit64(RLIMIT_STACK, &rlim));
        }

        {
            rlimit rlim;
            rlim.rlim_max = rlim.rlim_cur = 0;
            ENSURE(setrlimit(RLIMIT_CORE, &rlim));
        }

        gid_t groupList[1];
        groupList[0] = parameter.gid;
        ENSURE(syscall(SYS_setgid, parameter.gid));
        ENSURE(syscall(SYS_setgroups, 1, groupList));
        ENSURE(syscall(SYS_setuid, parameter.uid));

        vector<char *> params = StringToPtr(parameter.executableParameters),
                       envi = StringToPtr(parameter.environmentVariables);

        int temp = -1;
        // Inform the parent that no exception occurred.
        ENSURE(write(execParam.pipefd[1], &temp, sizeof(int)));

        // Inform our parent that we are ready to go.
        execParam.semaphore1.Post();
        // Wait for parent's reply.
        execParam.semaphore2.Wait();

        ENSURE(execvpe(parameter.executable.c_str(), &params[0], &envi[0]));

        // If execvpe returns, then we meet an error.
        return 1;
    }
    catch (std::exception &err)
    {
        const char *errMessage = err.what();
        int len = strlen(errMessage);
        try
        {
            ENSURE(write(execParam.pipefd[1], &len, sizeof(int)));
            ENSURE(write(execParam.pipefd[1], errMessage, len));
            ENSURE(close(execParam.pipefd[1]));
            execParam.semaphore1.Post();
            return 1;
        }
        catch (...)
        {
            assert(false);
        }
    }
    catch (...)
    {
        assert(false);
    }
}

// The child stack is only used before `execvpe`, so it does not need much space.
const int childStackSize = 1024 * 700;
void *StartSandbox(const SandboxParameter &parameter,
                   pid_t &container_pid)
{
    container_pid = -1;
    try
    {
        // char* childStack = new char[childStackSize];
        std::vector<char> childStack(childStackSize); // I don't want to call `delete`

        std::unique_ptr<ExecutionParameter> execParam = std::make_unique<ExecutionParameter>(parameter, O_CLOEXEC | O_NONBLOCK);

        container_pid = ENSURE(clone(ChildProcess, &*childStack.end(),
                                     CLONE_NEWNET | CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD,
                                     const_cast<void *>(reinterpret_cast<const void *>(execParam.get()))));

        CgroupInfo memInfo("memory", parameter.cgroupName),
            cpuInfo("cpuacct", parameter.cgroupName),
            pidInfo("pids", parameter.cgroupName);

        vector<CgroupInfo *> infos = {&memInfo, &cpuInfo, &pidInfo};
        for (auto &item : infos)
        {
            CreateGroup(*item);
            KillGroupMembers(*item);
            WriteGroupProperty(*item, "tasks", container_pid);
        }

#define WRITE_WITH_CHECK(__where, __name, __value)                  \
    {                                                               \
        if ((__value) >= 0)                                         \
        {                                                           \
            WriteGroupProperty((__where), (__name), (__value));     \
        }                                                           \
        else                                                        \
        {                                                           \
            WriteGroupProperty((__where), (__name), string("max")); \
        }                                                           \
    }

        // Forcibly clear any memory usage by cache.
        // WriteGroupProperty(memInfo, "memory.force_empty", 0); // This is too slow!!!!
        WriteGroupProperty(memInfo, "memory.memsw.limit_in_bytes", -1);
        WriteGroupProperty(memInfo, "memory.limit_in_bytes", -1);
        WRITE_WITH_CHECK(memInfo, "memory.limit_in_bytes", parameter.memoryLimit);
        WRITE_WITH_CHECK(memInfo, "memory.memsw.limit_in_bytes", parameter.memoryLimit);
        WRITE_WITH_CHECK(pidInfo, "pids.max", parameter.processLimit);

        // Wait for at most 500ms. If the child process hasn't posted the semaphore,
        // We will assume that the child has already dead.
        bool waitResult = execParam->semaphore1.TimedWait(500);

        int errLen, bytesRead = read(execParam->pipefd[0], &errLen, sizeof(int));
        // Child will be killed once the error has been thrown.
        if (!waitResult || bytesRead == 0 || bytesRead == -1)
        {
            if (waitpid(container_pid, nullptr, WNOHANG) == 0)
            {
                // The child process is still alive.
                throw std::runtime_error("The child process is not responding.");
            }
            // The child process exited with no information available.
            throw std::runtime_error("The child process has exited unexpectedly.");
        }
        else if (errLen != -1) // -1 indicates OK.
        {
            vector<char> buf(errLen);
            ENSURE(read(execParam->pipefd[0], &*buf.begin(), errLen));
            string errstr(buf.begin(), buf.end());
            throw std::runtime_error((format("The child process has reported the following error: {}", errstr)));
        }

        // Clear usage stats.
        WriteGroupProperty(memInfo, "memory.memsw.max_usage_in_bytes", 0);
        WriteGroupProperty(cpuInfo, "cpuacct.usage", 0);

        // Continue the child.
        execParam->semaphore2.Post();

        return execParam.release();
    }
    catch (std::exception &ex)
    {
        // Do the cleanups; we don't care whether these operations are successful.
        if (container_pid != -1)
        {
            (void)kill(container_pid, SIGKILL);
            (void)waitpid(container_pid, NULL, WNOHANG);
        }
        std::rethrow_exception(std::current_exception());
    }
}

ExecutionResult
WaitForProcess(pid_t pid, void *executionParameter)
{
    std::unique_ptr<ExecutionParameter> execParam(reinterpret_cast<ExecutionParameter *>(executionParameter));

    ExecutionResult result;
    int status;
    ENSURE(waitpid(pid, &status, 0));

    // Try reading error message first
    int errLen, bytesRead = read(execParam->pipefd[0], &errLen, sizeof(int));
    if (bytesRead > 0)
    {
        vector<char> buf(errLen);
        ENSURE(read(execParam->pipefd[0], &*buf.begin(), errLen));
        string errstr(buf.begin(), buf.end());
        throw std::runtime_error((format("The child process has reported the following error: {}", errstr)));
    }

    if (WIFEXITED(status))
    {
        result.status = EXITED;
        result.code = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status))
    {
        result.status = SIGNALED;
        result.code = WTERMSIG(status);
    }
    return result;
}

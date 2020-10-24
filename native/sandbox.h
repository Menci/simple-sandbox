#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <unistd.h>
#include <pwd.h>

enum RunStatus {
    EXITED = 0, // App exited normally.
    SIGNALED = 01, // App is kill by some signal.
};

struct ExecutionResult
{
    int status;
    // If exited, this is the exit code; if signaled, this is the signal number.
    int code;
};

struct MountInfo
{
    // The source path on your host machine.
    std::filesystem::path src;
    // The destination path in the sandbox.
    // This path must exist, i.e. be `mkdir`ed in the chroot directory.
    std::filesystem::path dst;
    // The maximum length (in bytes) the sandboxed process may write to the mount.
    // 0 for readonly; -1 for no limit.
    int64_t limit;
};

struct SandboxParameter
{
    // Time limit is done by querying cpuacct cgroup every 100ms. This is done in the js code.

    int64_t stackSize;
    // Memory limit in bytes.
    // -1 for no limit.
    int64_t memoryLimit;
    // The maximum child process count created by the executable. Typically less than 10. -1 for no limit.
    int processLimit;
    // Redirect stdin / stdout before chrooting.
    // Useful when debugging; 
    // You can use `socat -d -d pty,raw,echo=0 -` to create a device in /dev/pts and redirect stdio to that pts.
    // And then execute a shell (/bin/sh) in the sandbox to debug problems.
    bool redirectBeforeChroot;
    // Mount `/proc`?
    bool mountProc;
    // This directory will be chrooted into (`chroot`) before running our binary.
    // Make sure this is not writable by `nobody` user!
    std::filesystem::path chrootDirectory;
    // This directory will be changed into (`chdir`) before running the binary.
    std::filesystem::path workingDirectory;

    // The directories that will be mounted to the sandbox.
    // See definition of MountInfo for details.
    std::vector<MountInfo> mounts;

    // This executable is the file that will be run.
    // You may specify your native binary (or file with #! interpreter)
    // located in your mounted directory, such as `/sandbox/binary/a.out`,
    // or it may be an interpreter such as `/usr/bin/python` (this must be in your chroot filesystem)
    //
    // Tip: if you want to run a series of command,
    // You can create a .sh script and execute it.
    std::string executable;
    // These are the parameters passed to the guest executable.
    std::vector<std::string> executableParameters;
    std::vector<std::string> environmentVariables;
    // This is the input file that will be redirected to the executable as Standard Input.
    // Note that if you specify a relative path, it will be relative to the `/sandbox/working` directory.
    // Or you may specify an absoulete path (though this is usually not not the case).
    // If left empty, no stdin will be redirected.
    std::string stdinRedirection;
    // Stdout redirection, same as above.
    std::string stdoutRedirection;
    // Same as above.
    std::string stderrRedirection;

    // If set to -1, the sandbox will try to open the files in the strings above;
    // If set to others, the values will be used as the IOs.
    int stdinRedirectionFileDescriptor;
    int stdoutRedirectionFileDescriptor;
    int stderrRedirectionFileDescriptor;

    // The UID and GID the guest executable will be run as. 
    uid_t uid;
    gid_t gid;

    // The cgroup name of the sandbox. Must be unique.
    std::string cgroupName;

    // The hostname inside the sandbox, by default equals to the hostname outside.
    std::string hostname;
};

void GetUserEntryInSandbox(const std::filesystem::path &rootfs, const std::string username, std::vector<char> &dataBuffer, passwd &entry);

void *StartSandbox(const SandboxParameter &, pid_t &);

ExecutionResult WaitForProcess(pid_t pid, void *executionParameter);

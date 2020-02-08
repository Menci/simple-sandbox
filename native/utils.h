#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <system_error>

std::string SignalToString(int signal);

void Ensure_Seccomp(int XX);

int __Ensure(int XX, const char *file, int line, const char *operation);

void Ensure0(int XX);

template <typename T>
T EnsureNot(T ret, T err)
{
    if (ret == err)
    {
        int errcode = errno;
        throw std::system_error(errcode, std::system_category());
    }
    return ret;
}

template <typename T>
T EnsureNot(T ret, T err, const std::string &msg)
{
    if (ret == err)
    {
        int errcode = errno;
        throw std::system_error(errcode, std::system_category(), msg);
                                
    }
    return ret;
}

template <typename... Args>
int ptrace_e(Args... args)
{
    return ENSURE(ptrace(args...));
}

template <typename T>
T CheckNull_Custom(T val, const char *operation)
{
    if (val == nullptr)
    {
        throw std::runtime_error(std::string("Operation ") + std::string(operation) + std::string(" returned null."));
    }
    return val;
}

std::vector<char *> StringToPtr(const std::vector<std::string> &original);

#define CHECKNULL(value) CheckNull_Custom(value, #value)
#define ENSURE(value) (__Ensure((value), __FILE__, __LINE__, #value))

// Copied from my good old work https://github.com/CDFLS/cwoj_daemon/blob/master/src/daemon/sandboxing/utils.cpp

#include <string>
#include <vector>
#include <system_error>

#include <fmt/format.h>

#include "utils.h"

using std::string;
using std::vector;
using std::system_error;
using std::system_category;
using fmt::format;

void Ensure_Seccomp(int XX)
{
    if (XX != 0)
    {
        throw system_error(XX < 0 ? -XX : XX, system_category());
    }
}

int __Ensure(int XX, const char *file, int line, const char *operation)
{
    return EnsureNot(XX, -1, (format("`{}`@{},{}", operation, file, line)));
}

void Ensure0(int XX)
{
    if (XX != 0)
    {
        int err = errno;
        throw system_error(err, system_category());
    }
}

vector<char *> StringToPtr(const vector<string> &original)
{
    std::vector<char *> result;
    for (size_t i = 0; i < original.size(); ++i)
        result.push_back(
            const_cast<char *>(original[i].c_str()));
    result.push_back(nullptr);
    return result;
}

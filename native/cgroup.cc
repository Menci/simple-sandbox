#include <string>
#include <vector>
#include <list>
#include <algorithm>
#include <map>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include <filesystem>

#include <mntent.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include "utils.h"
#include "cgroup.h"

using std::string;
using std::vector;
using std::list;
using std::map;
using std::ifstream;
using std::ofstream;
namespace fs = std::filesystem;
using fmt::format;

const map<string, vector<fs::path>> cgroup_mnt = InitializeCgroup();

static bool IsEmpty(const string &str)
{
    auto f = [](unsigned char const c) { return std::isspace(c); };
    return std::all_of(str.begin(), str.end(), f);
}
CgroupInfo::CgroupInfo(const string &controller, const string &group)
    : Controller(controller), Group(group)
{
    if (IsEmpty(controller))
    {
        throw std::invalid_argument("Controller name cannot be empty!");
    }
    else if (IsEmpty(group))
    {
        throw std::invalid_argument("Group name cannot be empty!");
    }
}

// This piece of code is copied from libcgroup but translated to C++. C++ is very great.
map<string, vector<fs::path>> InitializeCgroup()
{
    map<string, vector<fs::path>> cgroup_mnt;

    char buf[4 * FILENAME_MAX];

    ifstream proc_cgroup("/proc/cgroups");
    // The first line is ignored.
    proc_cgroup.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    vector<string> controllers;
    string subsys_name;
    int hierarchy, num_cgroups, enabled;
    while (proc_cgroup >> subsys_name >> hierarchy >> num_cgroups >> enabled)
    {
        controllers.push_back(string(subsys_name));
    }

    std::unique_ptr<FILE, decltype(fclose) *> proc_mount(CHECKNULL(fopen("/proc/mounts", "re")), fclose);
    std::unique_ptr<mntent> temp_ent = std::make_unique<mntent>();
    mntent *ent;
    while ((ent = getmntent_r(proc_mount.get(), temp_ent.get(),
                              buf,
                              sizeof(buf))) != NULL)
    {
        if (strcmp(ent->mnt_type, "cgroup"))
            continue;

        for (auto iter = controllers.begin(); iter != controllers.end(); iter++)
        {
            char *mntopt = hasmntopt(ent, iter->c_str());
            if (!mntopt)
                continue;

            cgroup_mnt[*iter].push_back(fs::path(string(ent->mnt_dir)));
        }
    }

    return cgroup_mnt;
}

static const fs::path &GetPath(const string &controller)
{
    auto mnts = cgroup_mnt.find(controller);
    if (mnts == cgroup_mnt.end())
    {
        throw std::invalid_argument((format("Controller {} does not exist.", controller)));
    }
    return (mnts->second)[0];
}

static fs::path EnsureGroup(const CgroupInfo &info)
{
    fs::path groupDirectory = GetPath(info.Controller) / info.Group;
    if (!fs::exists(groupDirectory) || !fs::is_directory(groupDirectory))
    {
        throw std::runtime_error((format("Path {} is not valid (does not exist or is not a directory).", groupDirectory)));
    }
    return groupDirectory;
}

template <typename T>
static void WriteFile(const fs::path &path, T val, bool overwrite)
{
    ofstream ofs;
    ofs.exceptions(std::ios::failbit | std::ios::badbit);
    auto flags = ofstream::out | (overwrite ? ofstream::trunc : ofstream::app);
    ofs.open(path, flags);
    ofs << val << std::endl;
}

static void ReadArray64(const fs::path &path, list<int64_t> &cc)
{
    ifstream ifs;
    // No fail bit; just ignore when failed.
    ifs.exceptions(std::ios::badbit);
    ifs.open(path);
    cc.clear();
    int64_t val;
    while (ifs >> val)
    {
        cc.push_back(val);
    }
}

static int64_t ReadInt64(const fs::path &path)
{
    ifstream ifs;
    ifs.exceptions(std::ios::failbit | std::ios::badbit);
    ifs.open(path);
    int64_t val;
    ifs >> val;
    return val;
}

void CreateGroup(const CgroupInfo &info)
{
    auto groupDirectory = GetPath(info.Controller) / info.Group;
    if (!fs::exists(groupDirectory))
    {
        fs::create_directories(groupDirectory);
    }
    else if (!fs::is_directory(groupDirectory))
    {
        throw std::runtime_error((format("Path {} has already been used and is not a directory.", groupDirectory)));
    }
}

int64_t ReadGroupProperty(const CgroupInfo &info, const string &property)
{
    auto groupDir = EnsureGroup(info);
    return ReadInt64(groupDir / property);
}

list<int64_t> ReadGroupPropertyArray(const CgroupInfo &info, const string &property)
{
    auto groupDir = EnsureGroup(info);
    list<int64_t> val;
    ReadArray64(groupDir / property, val);
    return val;
}

map<string, int64_t> ReadGroupPropertyMap(const CgroupInfo &info, const string &property)
{
    auto groupDir = EnsureGroup(info);
    map<string, int64_t> result;
    ifstream ifs;
    ifs.exceptions(std::ios::badbit);
    ifs.open(groupDir / property);
    while (ifs)
    {
        string name;
        ifs >> name;
        int64_t val;
        ifs >> val;
        result.insert(std::pair<string, int64_t>(name, val));
    }
    return result;
}

void KillGroupMembers(const CgroupInfo &info)
{
    auto v = ReadGroupPropertyArray(info, "tasks");
    for (auto &item : v)
    {
        ENSURE(kill((int)(item), SIGKILL));
    }
}

void RemoveCgroup(const CgroupInfo &info)
{
    KillGroupMembers(info);
    auto groupDir = EnsureGroup(info);
    rmdir(groupDir.c_str());
}

void WriteGroupProperty(const CgroupInfo &info, const string &property, int64_t val, bool overwrite)
{
    auto groupDir = EnsureGroup(info);
    return WriteFile(groupDir / property, val, overwrite);
}

void WriteGroupProperty(const CgroupInfo &info, const string &property, const string &val, bool overwrite)
{
    auto groupDir = EnsureGroup(info);
    return WriteFile(groupDir / property, val, overwrite);
}

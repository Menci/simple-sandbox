#include <string>
#include <vector>
#include <list>
#include <algorithm>
#include <map>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/format.hpp>

#include <mntent.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>

#include "utils.h"
#include "cgroup.h"

using std::string;
using std::vector;
using std::list;
using std::map;
using std::ifstream;
namespace fs = boost::filesystem;
using boost::format;

map<string, vector<fs::path>> cgroup_mnt;

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
bool InitializeCgroup()
{
    cgroup_mnt.clear();
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

    FILE *proc_mount = CheckNull(fopen("/proc/mounts", "re"));
    mntent *temp_ent = new mntent, *ent;
    while ((ent = getmntent_r(proc_mount, temp_ent,
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
    delete temp_ent;

    // TODO: implement RAII here. `throw` will cause file descriptor leak for now.
    if (proc_mount)
        fclose(proc_mount);

    return cgroup_mnt.size() != 0;
}

static const fs::path &GetPath(const string &controller)
{
    auto mnts = cgroup_mnt.find(controller);
    if (mnts == cgroup_mnt.end())
    {
        throw std::invalid_argument((format("Controller %1% does not exist.") % controller).str());
    }
    return (mnts->second)[0];
}

static fs::path EnsureGroup(const CgroupInfo &info)
{
    fs::path groupDirectory = GetPath(info.Controller) / info.Group;
    if (!fs::exists(groupDirectory) || !fs::is_directory(groupDirectory))
    {
        throw std::runtime_error((format("Path %1% is not valid (does not exist or is not a directory).") % groupDirectory).str());
    }
    return groupDirectory;
}

template <typename T>
static void WriteFile(const fs::path &path, T val, bool overwrite)
{
    fs::ofstream ofs;
    ofs.exceptions(std::ios::failbit | std::ios::badbit);
    auto flags = fs::ofstream::out | (overwrite ? fs::ofstream::trunc : fs::ofstream::app);
    ofs.open(path, flags);
    ofs << val << std::endl;
}

static void ReadArray64(const fs::path &path, list<uint64_t> &cc)
{
    fs::ifstream ifs;
    // No fail bit; just ignore when failed.
    ifs.exceptions(std::ios::badbit);
    ifs.open(path);
    cc.clear();
    uint64_t val;
    while (ifs >> val)
    {
        cc.push_back(val);
    }
}

static uint64_t ReadInt64(const fs::path &path)
{
    fs::ifstream ifs;
    ifs.exceptions(std::ios::failbit | std::ios::badbit);
    ifs.open(path);
    uint64_t val;
    ifs >> val;
    return val;
}

void CreateGroup(const CgroupInfo &info)
{
    auto groupDirectory = GetPath(info.Controller) / info.Group;
    if (!fs::exists(groupDirectory))
    {
        fs::create_directory(groupDirectory);
    }
    else if (!fs::is_directory(groupDirectory))
    {
        throw std::runtime_error((format("Path %1% has already been used and is not a directory.") % groupDirectory).str());
    }
}

uint64_t ReadGroupProperty(const CgroupInfo &info, const string &property)
{
    auto groupDir = EnsureGroup(info);
    return ReadInt64(groupDir / property);
}

list<uint64_t> ReadGroupPropertyArray(const CgroupInfo &info, const string &property)
{
    auto groupDir = EnsureGroup(info);
    list<uint64_t> val;
    ReadArray64(groupDir / property, val);
    return val;
}

void KillGroupMembers(const CgroupInfo &info)
{
    auto v = ReadGroupPropertyArray(info, "tasks");
    for (auto &item : v)
    {
        Ensure(kill((int)(item), SIGKILL));
    }
}

void WriteGroupProperty(const CgroupInfo &info, const string &property, uint64_t val, bool overwrite)
{
    auto groupDir = EnsureGroup(info);
    return WriteFile(groupDir / property, val, overwrite);
}

void WriteGroupProperty(const CgroupInfo &info, const string &property, const string &val, bool overwrite)
{
    auto groupDir = EnsureGroup(info);
    return WriteFile(groupDir / property, val, overwrite);
}
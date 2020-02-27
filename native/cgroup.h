#pragma once
// This is a simple version of libcgroup.
// The libcgroup itself is complicated and not very well documented, so here I implement a new one.
// This one only fits the sandbox and does not have a general-purpose design.

#include <string>
#include <list>
#include <map>

struct CgroupInfo
{
    std::string Controller;
    std::string Group;
    CgroupInfo(const std::string &controller, const std::string &group); // : Controller(controller), Group(group)
};

// Look for controllers and their mount paths.
std::map<std::string, std::vector<std::filesystem::path>> InitializeCgroup();

void CreateGroup(const CgroupInfo &info);

int64_t ReadGroupProperty(const CgroupInfo &info, const std::string &property);
std::list<int64_t> ReadGroupPropertyArray(const CgroupInfo &info, const std::string &property);
std::map<std::string, int64_t> ReadGroupPropertyMap(const CgroupInfo &info, const std::string &property);

void WriteGroupProperty(const CgroupInfo &info, const std::string &property, int64_t val, bool overwrite = true);
void WriteGroupProperty(const CgroupInfo &info, const std::string &property, const std::string& val, bool overwrite = true);
void RemoveCgroup(const CgroupInfo &info);

// Kill all existing tasks in a group.
void KillGroupMembers(const CgroupInfo &info);

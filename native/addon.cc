#include <map>
#include <vector>
#include <string>
#include <functional>
#include <exception>
#include <cstring>
#include <filesystem>

#include <napi.h>
#include <fmt/format.h>

#include "sandbox.h"
#include "cgroup.h"

using std::string;
namespace fs = std::filesystem;

std::string GetStringWithEmptyCheck(Napi::Value value) {
    return value.IsString() ? value.ToString().Utf8Value() : "";
}

Napi::Value NodeGetCgroupProperty2(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    string controllerName = GetStringWithEmptyCheck(info[0]);
    string cgroupName = GetStringWithEmptyCheck(info[1]);
    string propertyName = GetStringWithEmptyCheck(info[2]);
    string subPropertyName = GetStringWithEmptyCheck(info[3]);
    try
    {
        CgroupInfo cginfo(controllerName, cgroupName);
        // v8 doesn't support 64-bit integer, so let's use string.
        return Napi::String::New(env, std::to_string(ReadGroupPropertyMap(cginfo, propertyName)[subPropertyName]));
    }
    catch (std::exception &ex)
    {
        Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
    }
    catch (...)
    {
        Napi::Error::New(env, "Something unexpected happened while getting cgroup property.").ThrowAsJavaScriptException();
    }
    return Napi::Value();
}

Napi::Value NodeGetCgroupProperty(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    string controllerName = GetStringWithEmptyCheck(info[0]);
    string cgroupName = GetStringWithEmptyCheck(info[1]);
    string propertyName = GetStringWithEmptyCheck(info[2]);
    try
    {
        CgroupInfo cginfo(controllerName, cgroupName);
        // v8 doesn't support 64-bit integer, so let's use string.
        return Napi::String::New(env, std::to_string(ReadGroupProperty(cginfo, propertyName)));
    }
    catch (std::exception &ex)
    {
        Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
    }
    catch (...)
    {
        Napi::Error::New(env, "Something unexpected happened while getting cgroup property.").ThrowAsJavaScriptException();
    }
    return Napi::Value();
}

void NodeRemoveCgroup(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    string controllerName = GetStringWithEmptyCheck(info[0]);
    string cgroupName = GetStringWithEmptyCheck(info[1]);
    try
    {
        CgroupInfo cginfo(controllerName, cgroupName);
        RemoveCgroup(cginfo);
    }
    catch (std::exception &ex)
    {
        Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
    }
    catch (...)
    {
        Napi::Error::New(env, "Something unexpected happened while removing cgroup.").ThrowAsJavaScriptException();
    }
}

std::vector<string> StringArrayToVector(const Napi::Array &array) {
    std::vector<string> result(array.Length());
    for (size_t i = 0; i < array.Length(); i++) result[i] = GetStringWithEmptyCheck(array[i]);
    return result;
}

Napi::Value NodeStartSandbox(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    SandboxParameter param;
    Napi::Object jsparam = info[0].As<Napi::Object>();

    // param.timeLimit = jsparam.Get("time").ToNumber().Int32Value();
    param.memoryLimit = jsparam.Get("memory").ToNumber().Int64Value() / 4 * 5; // Reserve some space to detect memory limit exceeding.
    param.processLimit = jsparam.Get("process").ToNumber().Int32Value();
    param.redirectBeforeChroot = jsparam.Get("redirectBeforeChroot").ToBoolean().Value();
    param.mountProc = jsparam.Get("mountProc").ToBoolean().Value();
    param.chrootDirectory = fs::path(GetStringWithEmptyCheck(jsparam.Get("chroot")));
    param.workingDirectory = fs::path(GetStringWithEmptyCheck(jsparam.Get("workingDirectory")));
    param.executable = GetStringWithEmptyCheck(jsparam.Get("executable"));
    param.hostname = GetStringWithEmptyCheck(jsparam.Get("hostname"));

#define SET_REDIRECTION(_name_)                                                                  \
    if (jsparam.Get(#_name_).IsNumber())                                                         \
    {                                                                                            \
        param._name_##RedirectionFileDescriptor = jsparam.Get(#_name_).ToNumber().Int32Value();  \
    }                                                                                            \
    else                                                                                         \
    {                                                                                            \
        param._name_##RedirectionFileDescriptor = -1;                                            \
        param._name_##Redirection = GetStringWithEmptyCheck(jsparam.Get(#_name_));               \
    }

    SET_REDIRECTION(stdin);
    SET_REDIRECTION(stdout);
    SET_REDIRECTION(stderr);

    auto user = jsparam.Get("user").ToObject();
    param.uid = user.Get("uid").ToNumber().Uint32Value();
    param.gid = user.Get("gid").ToNumber().Uint32Value();
    param.cgroupName = GetStringWithEmptyCheck(jsparam.Get("cgroup"));

    param.stackSize = jsparam.Get("stackSize").ToNumber().Int64Value();
    if (param.stackSize <= 0) {
        param.stackSize = -2;
    }

    param.executableParameters = StringArrayToVector(jsparam.Get("parameters").As<Napi::Array>());
    param.environmentVariables = StringArrayToVector(jsparam.Get("environments").As<Napi::Array>());
    Napi::Array mounts = jsparam.Get("mounts").As<Napi::Array>();
    for (size_t i = 0; i < mounts.Length(); i++)
    {
        Napi::Object mntObj = static_cast<Napi::Value>(mounts[i]).As<Napi::Object>();
        MountInfo mnt;
        mnt.src = fs::path(GetStringWithEmptyCheck(mntObj.Get("src")));
        mnt.dst = fs::path(GetStringWithEmptyCheck(mntObj.Get("dst")));
        mnt.limit = mntObj.Get("limit").ToNumber().Int32Value();
        param.mounts.push_back(mnt);
    }

    try
    {
        pid_t pid;
        void *execParam = StartSandbox(param, pid);
        Napi::Object result = Napi::Object::New(env);
        result.Set("pid", Napi::Number::New(env, pid));
        Napi::ArrayBuffer pointerToExecParam = Napi::ArrayBuffer::New(env, sizeof(execParam));
        *reinterpret_cast<void **>(pointerToExecParam.Data()) = execParam;
        result.Set("execParam", pointerToExecParam);
        return result;
    }
    catch (std::exception &ex)
    {
        Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
    }
    catch (...)
    {
        Napi::Error::New(env, "Something unexpected happened while starting sandbox.").ThrowAsJavaScriptException();
    }
    return Napi::Value();
}

class WaitForProcessWorker : public Napi::AsyncWorker
{
private:
    pid_t pid;
    void *executionParameter;
    ExecutionResult result;

public:
    WaitForProcessWorker(Napi::Function &callback, pid_t pid, void *executionParameter)
        : Napi::AsyncWorker(callback), pid(pid), executionParameter(executionParameter) {}

    void Execute()
    {
        try
        {
            result = WaitForProcess(pid, executionParameter);
        }
        catch (std::exception &ex)
        {
            SetError(ex.what());
        }
        catch (...)
        {
            SetError("Something unexpected occurred while waiting for process termiation");
        }
    }

    void OnOK()
    {
        Napi::Env env = Env();

        Napi::Object obj = Napi::Object::New(env);

        obj.Set("status", result.status == EXITED ? "exited" : "signaled");
        obj.Set("code", result.code);

        Callback().Call({env.Undefined(), obj});
    }
};


void NodeWaitForProcess(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    pid_t pid = info[0].ToNumber().Int32Value();
    void *executionParameter = *reinterpret_cast<void **>(info[1].As<Napi::ArrayBuffer>().Data());
    Napi::Function callback = info[2].As<Napi::Function>();

    WaitForProcessWorker *waitForProcessWorker = new WaitForProcessWorker(callback, pid, executionParameter);
    waitForProcessWorker->Queue();
}

Napi::Value NodeGetUidAndGidInSandbox(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    fs::path rootfs = info[0].As<Napi::String>().Utf8Value();
    auto username = info[1].As<Napi::String>().Utf8Value();

    std::vector<char> dataBuffer;
    passwd user;
    try {
        GetUserEntryInSandbox(rootfs, username, dataBuffer, user);
    } catch (std::exception &ex) {
        Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
        return env.Undefined();
    }

    auto result = Napi::Object::New(env);
    result.Set("uid", Napi::Number::New(env, user.pw_uid));
    result.Set("gid", Napi::Number::New(env, user.pw_gid));

    return result;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("getCgroupProperty", Napi::Function::New(env, NodeGetCgroupProperty));
    exports.Set("getCgroupProperty2", Napi::Function::New(env, NodeGetCgroupProperty2));
    exports.Set("removeCgroup", Napi::Function::New(env, NodeRemoveCgroup));
    exports.Set("getUidAndGidInSandbox", Napi::Function::New(env, NodeGetUidAndGidInSandbox));
    exports.Set("startSandbox", Napi::Function::New(env, NodeStartSandbox));
    exports.Set("waitForProcess", Napi::Function::New(env, NodeWaitForProcess));
    return exports;
}

NODE_API_MODULE(NODE_MODULE_NAME, Init)

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

Napi::Value NodeGetCgroupProperty2(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    string controllerName = info[0].ToString().Utf8Value();
    string cgroupName = info[1].ToString().Utf8Value();
    string propertyName = info[2].ToString().Utf8Value();
    string subPropertyName = info[3].ToString().Utf8Value();
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

    string controllerName = info[0].ToString().Utf8Value();
    string cgroupName = info[1].ToString().Utf8Value();
    string propertyName = info[2].ToString().Utf8Value();
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

    string controllerName = info[0].ToString().Utf8Value();
    string cgroupName = info[1].ToString().Utf8Value();
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
    for (size_t i = 0; i < array.Length(); i++) result[i] = array[i].ToString().Utf8Value();
    return result;
}

Napi::Value NodeStartSandbox(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    SandboxParameter param;
    Napi::Object jsparam = info[0].As<Napi::Object>();

    // param.timeLimit = jsparam.Get("time").ToNumber().Int32Value();
    param.memoryLimit = jsparam.Get("memory").ToNumber().Int32Value() / 4 * 5; // Reserve some space to detect memory limit exceeding.
    param.processLimit = jsparam.Get("process").ToNumber().Int32Value();
    param.redirectBeforeChroot = jsparam.Get("redirectBeforeChroot").ToBoolean().Value();
    param.mountProc = jsparam.Get("mountProc").ToBoolean().Value();
    param.chrootDirectory = fs::path(jsparam.Get("chroot").ToString().Utf8Value());
    param.workingDirectory = fs::path(jsparam.Get("workingDirectory").ToString().Utf8Value());
    param.executablePath = jsparam.Get("executable").ToString().Utf8Value();
    param.hostname = jsparam.Get("hostname").ToString().Utf8Value();

#define SET_REDIRECTION(_name_)                                                                  \
    if (jsparam.Get(#_name_).IsNumber())                                                      \
    {                                                                                            \
        param._name_##RedirectionFileDescriptor = jsparam.Get(#_name_).ToNumber().Int32Value(); \
    }                                                                                            \
    else                                                                                         \
    {                                                                                            \
        param._name_##RedirectionFileDescriptor = -1;                                            \
        param._name_##Redirection = jsparam.Get(#_name_).ToString().Utf8Value();                \
    }

    SET_REDIRECTION(stdin);
    SET_REDIRECTION(stdout);
    SET_REDIRECTION(stderr);

    param.userName = jsparam.Get("user").ToString().Utf8Value();
    param.cgroupName = jsparam.Get("cgroup").ToString().Utf8Value();

    param.stackSize = jsparam.Get("stackSize").ToNumber().Int32Value();
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
        mnt.src = fs::path(mntObj.Get("src").ToString().Utf8Value());
        mnt.dst = fs::path(mntObj.Get("dst").ToString().Utf8Value());
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

Napi::Value NodeWaitForProcess(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    pid_t pid = info[0].ToNumber().Int32Value();
    void *executionParameter = *reinterpret_cast<void **>(info[1].As<Napi::ArrayBuffer>().Data());

    ExecutionResult result;
    try
    {
        result = WaitForProcess(pid, executionParameter);
    }
    catch (std::exception &ex)
    {
        Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
        return Napi::Value();
    }
    catch (...)
    {
        Napi::Error::New(env, "Something unexpected occurred while waiting for process termiation").ThrowAsJavaScriptException();
        return Napi::Value();
    }

    Napi::Object obj = Napi::Object::New(env);

    obj.Set("status", result.status == EXITED ? "exited" : "signaled");
    obj.Set("code", result.code);

    return obj;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    InitializeCgroup();
    exports.Set("getCgroupProperty", Napi::Function::New(env, NodeGetCgroupProperty));
    exports.Set("getCgroupProperty2", Napi::Function::New(env, NodeGetCgroupProperty2));
    exports.Set("removeCgroup", Napi::Function::New(env, NodeRemoveCgroup));
    exports.Set("startSandbox", Napi::Function::New(env, NodeStartSandbox));
    exports.Set("waitForProcess", Napi::Function::New(env, NodeWaitForProcess));
    return exports;
}

NODE_API_MODULE(NODE_MODULE_NAME, Init)

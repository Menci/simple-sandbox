#include <nan.h>
#include <vector>
#include <string>
#include <functional>
#include <exception>
#include <cstring>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>

#include <libcgroup.h>

#include "sandbox.h"
#include "cgroup.h"

using boost::format;
using std::vector;
using std::string;

using v8::Local;
using v8::Array;
using v8::Number;
using v8::Object;
using v8::Value;
using v8::Function;
using Nan::ThrowError;
using Nan::ThrowTypeError;
using Nan::Callback;
using Nan::HandleScope;
using Nan::Get;
using Nan::Set;
using Nan::AsyncQueueWorker;
using Nan::AsyncProgressWorker;
using Nan::AsyncWorker;

#define STR(_value_) Nan::New(_value_).ToLocalChecked()
#define NUM(_value_) Nan::New<Number>(_value_)
namespace fs = boost::filesystem;

class WaitForProcessWorker : public AsyncWorker
{
  public:
    WaitForProcessWorker(Callback *callback, pid_t pid) : AsyncWorker(callback), m_pid(pid)
    {
    }

    void Execute()
    {
        try
        {
            m_result = SBWaitForProcess(m_pid);
        }
        catch (std::exception &ex)
        {
            SetErrorMessage(ex.what());
        }
        catch (...)
        {
            SetErrorMessage("Something unexpected occurred while waiting for process termiation");
        }
    }

    void HandleOKCallback()
    {
        HandleScope scope;

        Local<Object> resultObject = Nan::New<Object>();
        string statusStr;
        switch (m_result.Status)
        {
        case EXITED:
            statusStr = string("exited");
            break;
        case SIGNALED:
            statusStr = string("signaled");
            break;
        }
        Set(resultObject, STR("status"), STR(statusStr));
        Set(resultObject, STR("code"), NUM(m_result.Code));

        Local<Value> argv[] = {
            Nan::Null(),
            resultObject};
        callback->Call(2, argv);
    }

    void HandleErrorCallback()
    {
        HandleScope scope;

        Local<Value> argv[] = {
            Nan::Error(ErrorMessage()),
            Nan::Null()};
        callback->Call(2, argv);
    }

  private:
    pid_t m_pid;
    ExecutionResult m_result;
};

class StartSandboxWorker : public AsyncWorker
{
  public:
    StartSandboxWorker(Callback *callback, SandboxParameter param) : AsyncWorker(callback),
                                                                     m_parameter(param)
    {
    }

    void Execute()
    {
        try
        {
            resultPid = StartSandbox(m_parameter);
        }
        catch (std::exception &ex)
        {
            SetErrorMessage(ex.what());
        }
        catch (...)
        {
            SetErrorMessage("Something unexpected happened while starting sandbox.");
        }
    }

    void HandleOKCallback()
    {
        HandleScope scope;

        Local<Object> resultObject = Nan::New<Object>();
        Set(resultObject, STR("pid"), NUM(resultPid));

        Local<Value> argv[] = {
            Nan::Null(),
            resultObject};
        callback->Call(2, argv);
    }

    void HandleErrorCallback()
    {
        HandleScope scope;

        Local<Value> argv[] = {
            Nan::Error(ErrorMessage()),
            Nan::Null()};
        callback->Call(2, argv);
    }

  private:
    SandboxParameter m_parameter;
    int resultPid;
};

static void StringArrayToVector(Local<Value> val, vector<string> &vec)
{
    HandleScope scope;
    if (val->IsArray())
    {
        Local<Array> paramArray = Local<Array>::Cast(val);
        for (uint8_t i = 0; i < paramArray->Length(); i++)
        {
            vec.push_back(string(*Nan::Utf8String(paramArray->Get(i))));
        }
    }
    else
    {
        vec.push_back(string(*Nan::Utf8String(val)));
    }
}

static string ValueToString(const Local<Value> &val)
{
    if (val->IsUndefined() || val->IsNull())
    {
        return string("");
    }
    else
    {
        return string(*Nan::Utf8String(val));
    }
}

NAN_METHOD(GetCgroupProperty)
{
    string controllerName = ValueToString(info[0]);
    string cgroupName = ValueToString(info[1]);
    string propertyName = ValueToString(info[2]);
    uint64_t val;
    try
    {
        CgroupInfo cginfo(controllerName, cgroupName);
        val = ReadGroupProperty(cginfo, propertyName);
    }
    catch (std::exception &ex)
    {
        ThrowTypeError(ex.what());
        return;
    }
    // v8 doesn't support 64-bit integer, so let's use string.
    info.GetReturnValue().Set(STR(std::to_string(val)));
}

NAN_METHOD(StartChild)
{
    SandboxParameter param;
    Local<Object> jsparam;
    if (info[0]->IsObject())
    {
        jsparam = Local<Object>::Cast(info[0]);
    }
    else
    {
        ThrowTypeError("Parameter must an object.");
        return;
    }

#define PARAM(_src_, _name_) (Get(_src_, STR(#_name_)).ToLocalChecked())
#define GET_INT(_src_, _param_name_) (PARAM(_src_, _param_name_)->IntegerValue())
#define GET_BOOL(_src_, _param_name_) (PARAM(_src_, _param_name_)->BooleanValue())
#define GET_STRING(_src_, _param_name_) (ValueToString(PARAM(_src_, _param_name_)))

    // param.timeLimit = GET_INT(time);
    param.memoryLimit = GET_INT(jsparam, memory) / 4 * 5; // Reserve some space to detect memory limit exceeding.
    param.processLimit = GET_INT(jsparam, process);
    param.redirectBeforeChroot = GET_BOOL(jsparam, redirectBeforeChroot);
    param.mountProc = GET_BOOL(jsparam, mountProc);
    param.chrootDirectory = fs::path(GET_STRING(jsparam, chroot));
    param.workingDirectory = fs::path(GET_STRING(jsparam, workingDirectory));
    param.executablePath = GET_STRING(jsparam, executable);
    param.stdinRedirection = GET_STRING(jsparam, stdin);
    param.stdoutRedirection = GET_STRING(jsparam, stdout);
    param.stderrRedirection = GET_STRING(jsparam, stderr);
    param.userName = GET_STRING(jsparam, user);
    param.cgroupName = GET_STRING(jsparam, cgroup);

    StringArrayToVector(PARAM(jsparam, parameters), param.executableParameters);
    StringArrayToVector(PARAM(jsparam, environments), param.environmentVariables);
    Local<Array> mounts = Local<Array>::Cast(PARAM(jsparam, mounts));
    for (size_t i = 0; i < mounts->Length(); i++)
    {
        Local<Object> mntObj = Local<Object>::Cast(mounts->Get(i));
        MountInfo mnt;
        mnt.src = fs::path(GET_STRING(mntObj, src));
        mnt.dst = fs::path(GET_STRING(mntObj, dst));
        mnt.limit = GET_INT(mntObj, limit);
        param.mounts.push_back(mnt);
    }

    Callback *callback = new Callback(info[1].As<Function>());

    AsyncQueueWorker(new StartSandboxWorker(callback, param));
}

NAN_METHOD(WaitForProcess)
{
    pid_t pid = info[0]->IntegerValue();
    Callback *callback = new Callback(info[1].As<Function>());
    AsyncQueueWorker(new WaitForProcessWorker(callback, pid));
}

NAN_MODULE_INIT(Init)
{
    try
    {
        bool result = InitializeCgroup();
        if (!result)
        {
            return ThrowError("Cgroup not mounted. Please check your system configuration.");
        }
    }
    catch (std::exception &ex)
    {
        return ThrowError(((format("Unable to initialize libcgroup: %1%") % ex.what())).str().c_str());
    }

    NAN_EXPORT(target, StartChild);
    NAN_EXPORT(target, GetCgroupProperty);
    NAN_EXPORT(target, WaitForProcess);
}

NODE_MODULE(addon, Init)
#include <mutex>

#include "util.h"

#include "HID.h"
#include "HIDAsync.h"
#include "devices.h"

static void
deinitialize(void *ptr)
{
    auto ptr2 = static_cast<ContextState *>(ptr);
    delete ptr2;
}

Napi::Object
Init(Napi::Env env, Napi::Object exports)
{
    std::shared_ptr<ApplicationContext> appCtx = ApplicationContext::get();
    if (appCtx == nullptr)
    {
        Napi::TypeError::New(env, "cannot initialize hidapi (hid_init failed)").ThrowAsJavaScriptException();
        return exports;
    }

    auto ctor = HIDAsync::Initialize(env);

    // Future: Once targetting node-api v6, this ContextState flow can be replaced with instanceData
    auto context = new ContextState(appCtx, Napi::Persistent(ctor));
    napi_add_env_cleanup_hook(env, deinitialize, context);

    exports.Set("HID", HID::Initialize(env));
    exports.Set("HIDAsync", ctor);

    exports.Set("openAsyncHIDDevice", Napi::Function::New(env, &HIDAsync::Create, nullptr, context)); // TODO: verify context will be alive long enough

    exports.Set("devices", Napi::Function::New(env, &devices));
    exports.Set("devicesAsync", Napi::Function::New(env, &devicesAsync, nullptr, context)); // TODO: verify context will be alive long enough

    return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)

#include <mutex>

#include "util.h"

#include "HID.h"
#include "HIDAsync.h"
#include "devices.h"

struct libRef
{
    // Wrap a shared_ptr in a struct we can use a normal pointer
    std::shared_ptr<void> ptr;

    Napi::FunctionReference asyncCtor;
};

static void
deinitialize(void *ptr)
{
    auto ptr2 = static_cast<libRef *>(ptr);
    delete ptr2;
}

Napi::Object
Init(Napi::Env env, Napi::Object exports)
{
    std::shared_ptr<void> ref = getAppCtx();
    if (ref == nullptr)
    {
        Napi::TypeError::New(env, "cannot initialize hidapi (hid_init failed)").ThrowAsJavaScriptException();
        return exports;
    }

    auto ctor = Napi::Persistent(HIDAsync::Initialize(env));

    // Future: Once targetting node-api v6, this libRef flow can be replaced with instanceData
    auto ref2 = new libRef{ref, std::move(ctor)};
    napi_add_env_cleanup_hook(env, deinitialize, ref2);

    exports.Set("HID", HID::Initialize(env));
    // exports.Set("HIDAsync", ctor);

    exports.Set("openAsyncHIDDevice", Napi::Function::New(env, &HIDAsync::Create, nullptr, &ref2->asyncCtor)); // TODO: verify asyncCtor will be alive long enough

    exports.Set("devices", Napi::Function::New(env, &devices));
    exports.Set("devicesAsync", Napi::Function::New(env, &devicesAsync));

    return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)

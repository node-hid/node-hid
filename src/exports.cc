#include <mutex>

#include "util.h"

#include "HID.h"
#include "HIDAsync.h"

struct libRef
{
    // Wrap a shared_ptr in a struct we can use a normal pointer
    std::shared_ptr<void> ptr;
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
    std::shared_ptr<void> ref = getLibRef(env);
    if (ref == nullptr)
    {
        // getLibRef threw an error
        return exports;
    }

    // Future: Once targetting node-api v6, this libRef flow can be replaced with instanceData
    napi_add_env_cleanup_hook(env, deinitialize, new libRef{ref});

    exports.Set("HID", HID::Initialize(env));
    exports.Set("HIDAsync", HIDAsync::Initialize(env));
    // exports.Set("devices", Napi::Function::New(env, &HID::devices));

    return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)

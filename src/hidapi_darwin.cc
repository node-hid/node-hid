#include "hidapi_darwin.h"

Napi::Value setOpenExclusive(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    hid_darwin_set_open_exclusive(0);

    return env.Null();
}
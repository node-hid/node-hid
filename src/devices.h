#include "util.h"

Napi::Value generateDeviceInfo(const Napi::Env &env, hid_device_info *dev);

Napi::Value devices(const Napi::CallbackInfo &info);

Napi::Value devicesAsync(const Napi::CallbackInfo &info);

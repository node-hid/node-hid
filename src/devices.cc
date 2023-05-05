#include "devices.h"

bool parseDevicesParameters(const Napi::CallbackInfo &info, int *vendorId, int *productId)
{
    switch (info.Length())
    {
    case 0:
        return true;
    case 2:
        *vendorId = info[0].As<Napi::Number>().Int32Value();
        *productId = info[1].As<Napi::Number>().Int32Value();
        return true;
    default:
        return false;
    }
}

Napi::Value generateDevicesResultAndFree(const Napi::Env &env, hid_device_info *devs)
{
    Napi::Array retval = Napi::Array::New(env);
    int count = 0;
    for (hid_device_info *dev = devs; dev; dev = dev->next)
    {
        retval.Set(count++, generateDeviceInfo(env, dev));
    }
    hid_free_enumeration(devs);
    return retval;
}

Napi::Value generateDeviceInfo(const Napi::Env &env, hid_device_info *dev)
{
    Napi::Object deviceInfo = Napi::Object::New(env);
    deviceInfo.Set("vendorId", Napi::Number::New(env, dev->vendor_id));
    deviceInfo.Set("productId", Napi::Number::New(env, dev->product_id));
    if (dev->path)
    {
        deviceInfo.Set("path", Napi::String::New(env, dev->path));
    }
    if (dev->serial_number)
    {
        deviceInfo.Set("serialNumber", Napi::String::New(env, utf8_encode(dev->serial_number)));
    }
    if (dev->manufacturer_string)
    {
        deviceInfo.Set("manufacturer", Napi::String::New(env, utf8_encode(dev->manufacturer_string)));
    }
    if (dev->product_string)
    {
        deviceInfo.Set("product", Napi::String::New(env, utf8_encode(dev->product_string)));
    }
    deviceInfo.Set("release", Napi::Number::New(env, dev->release_number));
    deviceInfo.Set("interface", Napi::Number::New(env, dev->interface_number));
    if (dev->usage_page)
    {
        deviceInfo.Set("usagePage", Napi::Number::New(env, dev->usage_page));
    }
    if (dev->usage)
    {
        deviceInfo.Set("usage", Napi::Number::New(env, dev->usage));
    }
    return deviceInfo;
}

Napi::Value devices(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    int vendorId = 0;
    int productId = 0;
    if (!parseDevicesParameters(info, &vendorId, &productId))
    {
        Napi::TypeError::New(env, "unexpected number of arguments to HID.devices() call, expecting either no arguments or vendor and product ID").ThrowAsJavaScriptException();
        return env.Null();
    }

    auto appCtx = ApplicationContext::get();
    if (!appCtx)
    {
        Napi::TypeError::New(env, "hidapi not initialized").ThrowAsJavaScriptException();
        return env.Null();
    }

    hid_device_info *devs;
    {
        std::unique_lock<std::mutex> lock(appCtx->enumerateLock);
        devs = hid_enumerate(vendorId, productId);
    }
    return generateDevicesResultAndFree(env, devs);
}

class DevicesWorker : public PromiseAsyncWorker<ContextState *>
{
public:
    DevicesWorker(const Napi::Env &env, ContextState *context, int vendorId, int productId)
        : PromiseAsyncWorker(env, context),
          vendorId(vendorId),
          productId(productId) {}

    ~DevicesWorker()
    {
        if (devs)
        {
            // ensure devs is freed if not done by OnOK
            hid_free_enumeration(devs);
            devs = nullptr;
        }
    }

    // This code will be executed on the worker thread
    void Execute() override
    {
        std::unique_lock<std::mutex> lock(context->appCtx->enumerateLock);
        devs = hid_enumerate(vendorId, productId);
    }

    Napi::Value GetPromiseResult(const Napi::Env &env) override
    {
        if (devs)
        {
            auto result = generateDevicesResultAndFree(env, devs);
            devs = nullptr; // devs has already been freed
            return result;
        }
        else
        {
            return Napi::Array::New(env, 0);
        }
    }

private:
    int vendorId;
    int productId;
    hid_device_info *devs;
};

Napi::Value devicesAsync(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    void *data = info.Data();
    if (!data)
    {
        Napi::TypeError::New(env, "devicesAsync missing context").ThrowAsJavaScriptException();
        return env.Null();
    }
    ContextState *context = (ContextState *)data;

    int vendorId = 0;
    int productId = 0;
    if (!parseDevicesParameters(info, &vendorId, &productId))
    {

        Napi::TypeError::New(env, "unexpected number of arguments to HID.devicesAsync() call, expecting either no arguments or vendor and product ID").ThrowAsJavaScriptException();
        return env.Null();
    }

    return (new DevicesWorker(env, context, vendorId, productId))->QueueAndRun();
}
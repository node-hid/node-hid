#include "devices.h"

Napi::Value devices(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    int vendorId = 0;
    int productId = 0;

    switch (info.Length())
    {
    case 0:
        break;
    case 2:
        vendorId = info[0].As<Napi::Number>().Int32Value();
        productId = info[1].As<Napi::Number>().Int32Value();
        break;
    default:
        Napi::TypeError::New(env, "unexpected number of arguments to HID.devices() call, expecting either no arguments or vendor and product ID").ThrowAsJavaScriptException();
        return env.Null();
    }

    hid_device_info *devs = hid_enumerate(vendorId, productId);
    Napi::Array retval = Napi::Array::New(env);
    int count = 0;
    for (hid_device_info *dev = devs; dev; dev = dev->next)
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
            deviceInfo.Set("serialNumber", Napi::String::New(env, narrow(dev->serial_number)));
        }
        if (dev->manufacturer_string)
        {
            deviceInfo.Set("manufacturer", Napi::String::New(env, narrow(dev->manufacturer_string)));
        }
        if (dev->product_string)
        {
            deviceInfo.Set("product", Napi::String::New(env, narrow(dev->product_string)));
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
        retval.Set(count++, deviceInfo);
    }
    hid_free_enumeration(devs);
    return retval;
}
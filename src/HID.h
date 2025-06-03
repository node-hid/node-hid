#include "util.h"

#include <atomic>

class HID : public Napi::ObjectWrap<HID>
{
public:
    static Napi::Value Initialize(Napi::Env &env);

    void closeHandle();

    HID(const Napi::CallbackInfo &info);
    ~HID() { closeHandle(); }

    hid_device *_hidHandle;

    std::atomic<bool> _readRunning = {false};
    std::atomic<bool> _readInterrupt = {false};

private:
    static Napi::Value devices(const Napi::CallbackInfo &info);

    Napi::Value close(const Napi::CallbackInfo &info);
    Napi::Value read(const Napi::CallbackInfo &info);
    Napi::Value readInterrupt(const Napi::CallbackInfo &info);
    Napi::Value write(const Napi::CallbackInfo &info);
    Napi::Value setNonBlocking(const Napi::CallbackInfo &info);
    Napi::Value getFeatureReport(const Napi::CallbackInfo &info);
    Napi::Value sendFeatureReport(const Napi::CallbackInfo &info);
    Napi::Value readSync(const Napi::CallbackInfo &info);
    Napi::Value readTimeout(const Napi::CallbackInfo &info);
    Napi::Value getDeviceInfo(const Napi::CallbackInfo &info);
};

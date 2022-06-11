#include "util.h"
#include "read.h"

class HIDAsync : public Napi::ObjectWrap<HIDAsync>
{
public:
    static Napi::Value Initialize(Napi::Env &env);

    HIDAsync(const Napi::CallbackInfo &info);
    ~HIDAsync() { closeHandle(); }

private:
    std::shared_ptr<ReadHelper> helper;
    std::shared_ptr<WrappedHidHandle> _hidHandle;

    void closeHandle();

    static Napi::Value Create(const Napi::CallbackInfo &info);

    Napi::Value close(const Napi::CallbackInfo &info);
    Napi::Value readStart(const Napi::CallbackInfo &info);
    Napi::Value readStop(const Napi::CallbackInfo &info);
    Napi::Value write(const Napi::CallbackInfo &info);
    Napi::Value setNonBlocking(const Napi::CallbackInfo &info);
    Napi::Value getFeatureReport(const Napi::CallbackInfo &info);
    Napi::Value sendFeatureReport(const Napi::CallbackInfo &info);
    Napi::Value read(const Napi::CallbackInfo &info);
    Napi::Value getDeviceInfo(const Napi::CallbackInfo &info);
};
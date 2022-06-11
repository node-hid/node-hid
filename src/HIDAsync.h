#include "util.h"
#include "read.h"

class HIDAsync : public Napi::ObjectWrap<HIDAsync>
{
public:
    static Napi::Value Initialize(Napi::Env &env);

    void closeHandle();

    HIDAsync(const Napi::CallbackInfo &info);
    ~HIDAsync() { closeHandle(); }

    std::shared_ptr<WrappedHidHandle> _hidHandle;

private:
    std::shared_ptr<ReadHelper> helper;

    Napi::Value close(const Napi::CallbackInfo &info);
    Napi::Value readStart(const Napi::CallbackInfo &info);         // OK
    Napi::Value readStop(const Napi::CallbackInfo &info);          // OK
    Napi::Value write(const Napi::CallbackInfo &info);             // Asynced
    Napi::Value setNonBlocking(const Napi::CallbackInfo &info);    // Asynced
    Napi::Value getFeatureReport(const Napi::CallbackInfo &info);  // Asynced
    Napi::Value sendFeatureReport(const Napi::CallbackInfo &info); // Asynced
    Napi::Value read(const Napi::CallbackInfo &info);              // Asynced
    Napi::Value getDeviceInfo(const Napi::CallbackInfo &info);     // Asynced
};
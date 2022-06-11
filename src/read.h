#ifndef NODEHID_READ_H__
#define NODEHID_READ_H__

#include "util.h"

#include <thread>
#include <atomic>

class ReadHelper
{
public:
    ReadHelper(std::shared_ptr<WrappedHidHandle> hidHandle);
    ~ReadHelper();

    void start(Napi::Env env, Napi::Function callback);
    void stop();

    std::atomic<bool> run_read = {false};

private:
    std::shared_ptr<WrappedHidHandle> _hidHandle;
    Napi::ThreadSafeFunction read_callback;
    std::thread read_thread;
};

#endif // NODEHID_READ_H__
#ifndef NODEHID_READ_H__
#define NODEHID_READ_H__

#include "util.h"

#include <thread>
#include <atomic>

class ReadHelper
{
public:
    ReadHelper(std::shared_ptr<DeviceContext> hidHandle);
    ~ReadHelper();

    void start(Napi::Env env, Napi::Function callback);
    void stop_and_join();

    std::atomic<bool> run_read = {false};

private:
    std::shared_ptr<DeviceContext> _hidHandle;
    std::thread read_thread;
    Napi::ThreadSafeFunction read_callback;
};

#endif // NODEHID_READ_H__
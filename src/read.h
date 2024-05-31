#ifndef NODEHID_READ_H__
#define NODEHID_READ_H__

#include "util.h"

#include <thread>
#include <atomic>
#include <condition_variable>

struct ReadThreadState
{
    std::atomic<bool> abort = {false};

    bool is_running();
    void wait();

    void release();

private:
    std::mutex lock;
    bool running = true;
    std::condition_variable wait_for_end;
};

std::shared_ptr<ReadThreadState>
start_read_helper(Napi::Env env, std::shared_ptr<DeviceContext> hidHandle, Napi::Function callback);

#endif // NODEHID_READ_H__
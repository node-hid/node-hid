#ifndef NODEHID_UTIL_H__
#define NODEHID_UTIL_H__

#define NAPI_VERSION 4
#include <napi.h>

#include <queue>

#include <hidapi.h>

#define READ_BUFF_MAXSIZE 2048

void deleteArray(const Napi::Env &env, unsigned char *ptr);

std::string copyArrayOrBufferIntoVector(const Napi::Value &val, std::vector<unsigned char> &message);

class WrappedHidHandle
{
public:
    WrappedHidHandle(hid_device *hidHandle);
    ~WrappedHidHandle();
    hid_device *hid;

    /**
     * Push a job onto the queue.
     * Note: This must only be run from the main thread
     */
    void QueueJob(const Napi::Env &, Napi::AsyncWorker *job);

    /**
     * The job has finished, start the next in the queue.
     * Note: This must only be run from the main thread
     */
    void JobFinished(const Napi::Env &);

private:
    bool isRunning = false;
    std::queue<Napi::AsyncWorker *> jobQueue;
    std::mutex jobQueueMutex;
};

#endif // NODEHID_UTIL_H__
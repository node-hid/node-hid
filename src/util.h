#ifndef NODEHID_UTIL_H__
#define NODEHID_UTIL_H__

#define NAPI_VERSION 4
#include <napi.h>

#include <queue>

#include <hidapi.h>

#define READ_BUFF_MAXSIZE 2048

std::shared_ptr<void> getLibRef();

Napi::Buffer<unsigned char> convertToNodeOwnerBuffer(const Napi::Env &env, unsigned char *ptr, size_t len);

std::string narrow(wchar_t *wide);

/**
 * Convert a js value (either a buffer ot array of numbers) into a vector of bytes.
 * Returns a non-empty string upon failure
 */
std::string copyArrayOrBufferIntoVector(const Napi::Value &val, std::vector<unsigned char> &message);

class WrappedHidHandle
{
public:
    WrappedHidHandle(std::shared_ptr<void> libRef, hid_device *hidHandle);
    ~WrappedHidHandle();

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

    hid_device *hid;

private:
    std::shared_ptr<void> libRef;

    bool isRunning = false;
    std::queue<Napi::AsyncWorker *> jobQueue;
    std::mutex jobQueueMutex;
};

#endif // NODEHID_UTIL_H__
#include <sstream>

#include "util.h"

// Ensure hid_init/hid_exit is coordinated across all threads
std::mutex initLock;
std::weak_ptr<void> initRef;

static void releaseLib(void *)
{
    // Make sure we dont try to aquire it or run init at the same time
    std::unique_lock<std::mutex> lock(initLock);

    initRef.reset();

    if (hid_exit())
    {
        // thread is exiting, can't log?
    }
}

std::shared_ptr<void> getLibRef()
{
    {
        // Make sure we run init on only one thread
        std::unique_lock<std::mutex> lock(initLock);

        auto ref = initRef.lock();
        if (!ref)
        {
            // Not initialised, so lets do that
            if (hid_init())
            {
                return nullptr;
            }

            ref = std::shared_ptr<void>(nullptr, releaseLib);
            initRef = ref;
        }
        return ref;
    }
}

void deleteArray(const Napi::Env &env, unsigned char *ptr)
{
    delete[] ptr;
}

Napi::Buffer<unsigned char> convertToNodeOwnerBuffer(const Napi::Env &env, unsigned char *ptr, size_t len)
{
    return Napi::Buffer<unsigned char>::New(env, ptr, len, deleteArray);
}

std::string narrow(wchar_t *wide)
{
    std::wstring ws(wide);
    std::ostringstream os;
    for (size_t i = 0; i < ws.size(); i++)
    {
        os << os.narrow(ws[i], '?');
    }
    return os.str();
}

std::string copyArrayOrBufferIntoVector(const Napi::Value &val, std::vector<unsigned char> &message)
{
    if (val.IsBuffer())
    {
        Napi::Buffer<unsigned char> buffer = val.As<Napi::Buffer<unsigned char>>();
        uint32_t len = buffer.Length();
        unsigned char *data = buffer.Data();
        message.assign(data, data + len);

        return "";
    }
    else if (val.IsArray())
    {
        Napi::Array messageArray = val.As<Napi::Array>();
        message.reserve(messageArray.Length());

        for (unsigned i = 0; i < messageArray.Length(); i++)
        {
            Napi::Value v = messageArray.Get(i);
            if (!v.IsNumber())
            {
                return "unexpected array element in array to send, expecting only integers";
            }
            uint32_t b = v.As<Napi::Number>().Uint32Value();
            message.push_back((unsigned char)b);
        }

        return "";
    }
    else
    {
        return "unexpected data to send, expecting an array or buffer";
    }
}

WrappedHidHandle::WrappedHidHandle(std::shared_ptr<void> libRef, hid_device *hidHandle) : hid(hidHandle), libRef(libRef) {}
WrappedHidHandle::~WrappedHidHandle()
{
    if (hid)
    {
        hid_close(hid);
        hid = nullptr;
    }

    // TODO - discard the jobQueue in a safe manner
}

void WrappedHidHandle::QueueJob(const Napi::Env &, Napi::AsyncWorker *job)
{
    std::unique_lock<std::mutex> lock(jobQueueMutex);
    if (!isRunning)
    {
        isRunning = true;
        job->Queue();
    }
    else
    {
        jobQueue.push(job);
    }
}

void WrappedHidHandle::JobFinished(const Napi::Env &)
{
    std::unique_lock<std::mutex> lock(jobQueueMutex);

    if (jobQueue.size() == 0)
    {
        isRunning = false;
    }
    else
    {
        auto newJob = jobQueue.front();
        jobQueue.pop();
        newJob->Queue();
    }
}

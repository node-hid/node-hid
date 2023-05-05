#include <sstream>
#include <locale>
#include <codecvt>

#include "util.h"

// Ensure hid_init/hid_exit is coordinated across all threads. Global data is bad for context-aware modules, but this is designed to be safe
std::mutex lockApplicationContext;
std::weak_ptr<ApplicationContext> weakApplicationContext; // This will let it be garbage collected when it goes out of scope in the last thread

ApplicationContext::~ApplicationContext()
{
    // Make sure we dont try to aquire it or run init at the same time
    std::unique_lock<std::mutex> lock(lockApplicationContext);

    if (hid_exit())
    {
        // thread is exiting, can't log?
    }
}

std::shared_ptr<ApplicationContext> ApplicationContext::get()
{
    // Make sure that we don't try to lock the pointer while it is being freed
    // and that two threads don't try to create it concurrently
    std::unique_lock<std::mutex> lock(lockApplicationContext);

    auto ref = weakApplicationContext.lock();
    if (!ref)
    {
        // Not initialised, so lets do that
        if (hid_init())
        {
            return nullptr;
        }

        ref = std::make_shared<ApplicationContext>();
        weakApplicationContext = ref;
    }
    return ref;
}

std::string utf8_encode(const std::wstring &source)
{
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(source);
}

std::wstring utf8_decode(const std::string &source)
{
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(source);
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

DeviceContext::~DeviceContext()
{
    if (hid)
    {
        // We shouldn't ever get here, but lets make sure it was freed
        hid_close(hid);
        hid = nullptr;
    }
}

void AsyncWorkerQueue::QueueJob(const Napi::Env &, Napi::AsyncWorker *job)
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

void AsyncWorkerQueue::JobFinished(const Napi::Env &)
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

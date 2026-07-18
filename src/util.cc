#include <sstream>
#include <algorithm>
#include <cwchar>
#include <clocale>
#include <cstdlib>

#include "util.h"

// Ensure hid_init/hid_exit is coordinated across all threads. Global data is bad for context-aware modules, but this is designed to be safe
std::mutex lockApplicationContext;
std::weak_ptr<ApplicationContext> weakApplicationContext; // This will let it be garbage collected when it goes out of scope in the last thread

ApplicationContext::~ApplicationContext()
{
    // Make sure we dont try to aquire it or run init at the same time
    std::unique_lock<std::mutex> lock(lockApplicationContext);

    // Another thread may have re-created a context (and re-run hid_init) between our
    // refcount hitting zero and this destructor acquiring the lock. In that case the
    // newer context owns the eventual hid_exit and we must not tear down under it
    if (!weakApplicationContext.expired())
        return;

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
        // TEST-ONLY escape hatch, not supported for production use: hidapi's linux
        // hidraw backend converts device strings with mbstowcs, which needs a UTF-8
        // LC_CTYPE to handle non-ascii text (the default "C" locale converts them to
        // empty strings). Calling setlocale from a library is not safe in general
        // (process-global, and racy against other threads using locale-dependent
        // libc functions), so the test suite must opt in explicitly
        const char *testLocale = getenv("NODE_HID_TEST_SET_LOCALE");
        if (testLocale && *testLocale)
        {
            setlocale(LC_CTYPE, testLocale);
        }

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

// wchar_t is 2 bytes (UTF-16) on Windows, 4 bytes (UTF-32) on POSIX.
// Anything else is unsupported — fail loudly at compile time rather than
// corrupting text at runtime.
static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4,
              "Unsupported wchar_t size: expected 2 (Windows) or 4 (POSIX)");

// std::u16string (from N-API's Utf16Value) -> std::wstring (for hidapi input)
std::wstring u16_to_wide(const std::u16string &in)
{
    if constexpr (sizeof(wchar_t) == 2)
    {
        // Windows: char16_t and wchar_t are the same UTF-16 code units.
        return std::wstring(in.begin(), in.end());
    }
    else
    {
        // POSIX: combine surrogate pairs into UTF-32 code points.
        std::wstring out;
        out.reserve(in.size());
        size_t i = 0, n = in.size();
        while (i < n)
        {
            uint32_t cp = in[i];
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < n)
            {
                uint32_t lo = in[i + 1];
                if (lo >= 0xDC00 && lo <= 0xDFFF)
                {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    i += 2;
                }
                else
                {
                    cp = 0xFFFD; // high surrogate not followed by low
                    i += 1;
                }
            }
            else if (cp >= 0xD800 && cp <= 0xDFFF)
            {
                cp = 0xFFFD; // lone surrogate
                i += 1;
            }
            else
            {
                i += 1;
            }
            out.push_back((wchar_t)cp);
        }
        return out;
    }
}

// std::wstring (from hidapi output) -> std::u16string (for N-API's String::New)
std::u16string wide_to_u16(const std::wstring &in)
{
    if constexpr (sizeof(wchar_t) == 2)
    {
        // Windows: already UTF-16.
        return std::u16string(in.begin(), in.end());
    }
    else
    {
        // POSIX: split astral code points into surrogate pairs.
        std::u16string out;
        out.reserve(in.size());
        for (wchar_t wc : in)
        {
            uint32_t cp = (uint32_t)wc;
            if (cp <= 0xFFFF)
            {
                out.push_back((char16_t)cp);
            }
            else if (cp <= 0x10FFFF)
            {
                cp -= 0x10000;
                out.push_back((char16_t)(0xD800 + (cp >> 10)));
                out.push_back((char16_t)(0xDC00 + (cp & 0x3FF)));
            }
            else
            {
                out.push_back((char16_t)0xFFFD); // out of Unicode range
            }
        }
        return out;
    }
}

static std::string appendHidErrorString(const wchar_t *err, std::string message)
{
    // hidapi reports L"Success" when the last call did not record an error detail
    if (err && *err && std::wcscmp(err, L"Success") != 0)
    {
        message.reserve(message.size() + 2 + std::wcslen(err));
        message += ": ";

        // hid_error strings are debug/log text and ascii in practice (fixed english
        // messages, or strerror under the default locale). Anything wider is not
        // worth carrying proper conversion machinery for; flatten it lossily
        for (const wchar_t *c = err; *c; c++)
        {
            message += (*c >= 0x20 && *c < 0x7F) ? (char)*c : '?';
        }
    }
    return message;
}

std::string appendLastHidError(hid_device *hid, std::string message)
{
    return appendHidErrorString(hid_error(hid), std::move(message));
}

std::string appendLastHidReadError(hid_device *hid, std::string message)
{
    // Unlike hid_error, hid_read_error has no global fallback
    if (!hid)
        return message;

    return appendHidErrorString(hid_read_error(hid), std::move(message));
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

AsyncWorkerQueue::~AsyncWorkerQueue()
{
    // Backstop; anything queued at this point can never be started
    DrainQueue();
}

void AsyncWorkerQueue::DrainQueue()
{
    // Take the jobs out under the lock, but delete them outside it: deleting a worker
    // can release the last shared_ptr to another DeviceContext, recursing into that
    // queue's destructor/DrainQueue
    std::queue<Napi::AsyncWorker *> jobs;
    {
        std::unique_lock<std::mutex> lock(jobQueueMutex);
        jobQueue.swap(jobs);
    }

    while (!jobs.empty())
    {
        // These were never queued with napi, so they must be deleted manually
        delete jobs.front();
        jobs.pop();
    }
}

void ContextState::RegisterDevice(const std::shared_ptr<DeviceContext> &device)
{
    std::unique_lock<std::mutex> lock(devicesMutex);

    // Prune any devices which have already been freed, to stop the list growing unboundedly
    devices.erase(
        std::remove_if(devices.begin(), devices.end(), [](const std::weak_ptr<DeviceContext> &weak)
                       { return weak.expired(); }),
        devices.end());

    devices.push_back(device);
}

void ContextState::DrainAllQueues()
{
    std::vector<std::shared_ptr<DeviceContext>> liveDevices;
    {
        std::unique_lock<std::mutex> lock(devicesMutex);
        for (auto &weak : devices)
        {
            if (auto device = weak.lock())
                liveDevices.push_back(std::move(device));
        }
        devices.clear();
    }

    for (auto &device : liveDevices)
        device->DrainQueue();

    DrainQueue();

    // When liveDevices goes out of scope, any DeviceContext whose last reference was
    // held by a queued worker is freed, closing its hid handle
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

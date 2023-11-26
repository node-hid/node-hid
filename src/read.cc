#include "read.h"

struct ReadCallbackProps
{
    unsigned char *buf;
    int len;
};

static void ReadCallback(Napi::Env env, Napi::Function jsCallback, ReadCallbackProps *data)
{
    auto buffer = Napi::Buffer<unsigned char>::Copy(env, data->buf, data->len);
    delete data->buf;
    delete data;

    jsCallback.Call({env.Null(), buffer});
};
static void ReadErrorCallback(Napi::Env env, Napi::Function jsCallback, void *data)
{
    auto error = Napi::String::New(env, "could not read from HID device");

    jsCallback.Call({error, env.Null()});
};

ReadHelper::ReadHelper(std::shared_ptr<DeviceContext> hidHandle)
{
    _hidHandle = hidHandle;
}
ReadHelper::~ReadHelper()
{
    stop_and_join();
}

void ReadHelper::stop_and_join()
{
    run_read = false;

    if (read_thread.joinable())
    {
        read_thread.join();
    }
}

void ReadHelper::start(Napi::Env env, Napi::Function callback)
{
    // If the read is already running, then abort
    if (run_read)
        return;
    run_read = true;

    read_thread = std::thread([&]()
                              {
                              int mswait = 50;
                              int len = 0;
                              unsigned char *buf = new unsigned char[READ_BUFF_MAXSIZE];

                              run_read = true;
                              while (run_read)
                              {
                                len = hid_read_timeout(_hidHandle->hid, buf, READ_BUFF_MAXSIZE, mswait);
                                if (len < 0)
                                {
                                  // Emit and error and stop reading
                                  read_callback.BlockingCall((void *)nullptr, ReadErrorCallback);
                                  break;
                                }
                                else if (len > 0)
                                {
                                  auto data = new ReadCallbackProps;
                                  data->buf = buf;
                                  data->len = len;

                                  read_callback.BlockingCall(data, ReadCallback);
                                  // buf is now owned by ReadCallback
                                  buf = new unsigned char[READ_BUFF_MAXSIZE];
                                }
                              }

                              run_read = false;
                              delete[] buf;

                              // Cleanup the function
                              read_callback.Release(); });

    read_callback = Napi::ThreadSafeFunction::New(
        env,
        callback,        // JavaScript function called asynchronously
        "HID:read",      // Name
        0,               // Unlimited queue
        1,               // Only one thread will use this initially
        [&](Napi::Env) { // Finalizer used to clean threads up
                         // Wait for end of the thread, if it wasnt the one to close up
            if (read_thread.joinable())
            {
                read_thread.join();
            }
        });
}

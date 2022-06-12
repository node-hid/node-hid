#ifndef NODEHID_UTIL_H__
#define NODEHID_UTIL_H__

#define NAPI_VERSION 4
#include <napi.h>

#include <queue>

#include <hidapi.h>

#define READ_BUFF_MAXSIZE 2048

/**
 * Convert a pointer into a Napi::Buffer, transferring ownership to the Buffer (it will free it at the appropriate point)
 */
Napi::Buffer<unsigned char> convertToNodeOwnerBuffer(const Napi::Env &env, unsigned char *ptr, size_t len);

std::string narrow(wchar_t *wide);

/**
 * Convert a js value (either a buffer ot array of numbers) into a vector of bytes.
 * Returns a non-empty string upon failure
 */
std::string copyArrayOrBufferIntoVector(const Napi::Value &val, std::vector<unsigned char> &message);

/**
 * Application-wide shared state.
 * This is referenced by the main thread and every worker_thread where node-hid has been loaded and not yet unloaded.
 */
class ApplicationContext
{
public:
    ~ApplicationContext();

    static std::shared_ptr<ApplicationContext> get();

    // A lock for any enumerate/open operations, as they are not thread safe
    // In async land, these are also done in a single-threaded queue, this lock is used to link up with the sync side
    std::mutex enumerateLock;
};

class AsyncWorkerQueue
{
    // TODO - discard the jobQueue in a safe manner

public:
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

/**
 * Context-wide shared state.
 * One of these will be created for each Napi::Env (main thread and each worker_thread)
 */
class ContextState : public AsyncWorkerQueue
{
public:
    ContextState(std::shared_ptr<ApplicationContext> appCtx, Napi::FunctionReference asyncCtor) : AsyncWorkerQueue(), appCtx(appCtx), asyncCtor(std::move(asyncCtor)) {}

    // Keep the ApplicationContext alive for longer than this state
    std::shared_ptr<ApplicationContext> appCtx;

    // Constructor for the HIDAsync class
    Napi::FunctionReference asyncCtor;
};

class DeviceContext : public AsyncWorkerQueue
{
public:
    DeviceContext(std::shared_ptr<ApplicationContext> appCtx, hid_device *hidHandle) : AsyncWorkerQueue(), hid(hidHandle), appCtx(appCtx)
    {
    }

    ~DeviceContext();

    hid_device *hid;

    bool is_closed = false;

private:
    // Hold a reference to the ApplicationContext,
    std::shared_ptr<ApplicationContext> appCtx;
};

template <class T>
class PromiseAsyncWorker : public Napi::AsyncWorker
{
public:
    PromiseAsyncWorker(
        const Napi::Env &env, T context)
        : Napi::AsyncWorker(env),
          context(context),
          deferred(Napi::Promise::Deferred::New(env))
    {
    }

    // This code will be executed on the worker thread. Note: Napi types cannot be used
    virtual void Execute() override = 0;

    virtual Napi::Value GetResult(const Napi::Env &env) = 0;

    void OnOK() override
    {
        Napi::Env env = Env();
        context->JobFinished(env);

        deferred.Resolve(GetResult(env));
    }
    void OnError(Napi::Error const &error) override
    {
        context->JobFinished(Env());
        deferred.Reject(error.Value());
    }

    Napi::Promise QueueAndRun()
    {
        auto promise = deferred.Promise();

        context->QueueJob(Env(), this);

        return promise;
    }

protected:
    T context;

private:
    Napi::Promise::Deferred deferred;
};

#endif // NODEHID_UTIL_H__
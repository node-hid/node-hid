// -*- C++ -*-

// Copyright Hans Huebner and contributors. All rights reserved.
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use, copy,
// modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include <sstream>
#include <vector>
#include <queue>

#include "util.h"
#include "HIDAsync.h"
#include "read.h"

HIDAsync::HIDAsync(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<HIDAsync>(info)
{
  Napi::Env env = info.Env();

  if (info.Length() != 1 || !info[0].IsExternal())
  {
    Napi::TypeError::New(env, "HIDAsync constructor is not supported").ThrowAsJavaScriptException();
    return;
  }

  auto appCtx = getAppCtx();
  if (!appCtx)
  {
    Napi::TypeError::New(env, "hidapi not initialized").ThrowAsJavaScriptException();
    return;
  }

  auto ptr = info[0].As<Napi::External<hid_device>>().Data();
  _hidHandle = std::make_shared<WrappedHidHandle>(appCtx, ptr);
  helper = std::make_shared<ReadHelper>(_hidHandle);
}

class CloseWorker : public PromiseAsyncWorker<WrappedHidHandle>
{
public:
  CloseWorker(
      Napi::Env &env, std::shared_ptr<WrappedHidHandle> hid)
      : PromiseAsyncWorker(env, hid) {}

  // This code will be executed on the worker thread. Note: Napi types cannot be used
  void Execute() override
  {
    if (queue->hid)
    {
      hid_close(queue->hid);
      queue->hid = nullptr; // TODO - we need to null check this in other workers
    }
  }

  Napi::Value GetResult(const Napi::Env &env) override
  {
    return env.Undefined();
  }

private:
};

void HIDAsync::closeHandle()
{
  if (helper)
  {
    helper->stop();
    helper = nullptr;
  }

  // hid_close is called by the destructor
  _hidHandle = nullptr;
}

class OpenByPathWorker : public PromiseAsyncWorker<ApplicationContext>
{
public:
  OpenByPathWorker(const Napi::Env &env, std::shared_ptr<ApplicationContext> appCtx, std::string path)
      : PromiseAsyncWorker(env, appCtx),
        path(path) {}

  ~OpenByPathWorker()
  {
    if (dev)
    {
      // dev wasn't claimed
      hid_close(dev);
      dev = nullptr;
    }
  }

  // This code will be executed on the worker thread
  void Execute() override
  {
    std::unique_lock<std::mutex> lock(queue->enumerateLock);
    dev = hid_open_path(path.c_str());
    if (!dev)
    {
      std::ostringstream os;
      os << "cannot open device with path " << path;
      SetError(os.str());
    }
  }

  Napi::Value GetResult(const Napi::Env &env) override
  {
    // TODO call the constructor
    // auto result = generateDevicesResultAndFree(env, devs);
    dev = nullptr; // devs has already been freed
    return env.Null();
  }

private:
  std::string path;
  hid_device *dev;
};

class OpenByUsbIdsWorker : public PromiseAsyncWorker<ApplicationContext>
{
public:
  OpenByUsbIdsWorker(const Napi::Env &env, std::shared_ptr<ApplicationContext> appCtx, int vendorId, int productId, std::string serial)
      : PromiseAsyncWorker(env, appCtx),
        vendorId(vendorId),
        productId(productId),
        serial(serial) {}

  ~OpenByUsbIdsWorker()
  {
    if (dev)
    {
      // dev wasn't claimed
      hid_close(dev);
      dev = nullptr;
    }
  }

  // This code will be executed on the worker thread
  void Execute() override
  {
    std::unique_lock<std::mutex> lock(queue->enumerateLock);

    wchar_t wserialstr[100]; // FIXME: is there a better way?
    wchar_t *wserialptr = NULL;
    if (serial != "")
    {
      mbstowcs(wserialstr, serial.c_str(), 100); // TODO: this is not thread safe!
      wserialptr = wserialstr;
    }

    dev = hid_open(vendorId, productId, wserialptr);
    if (!dev)
    {
      std::ostringstream os;
      os << "cannot open device with vendor id 0x" << std::hex << vendorId << " and product id 0x" << productId;
      SetError(os.str());
    }
  }

  Napi::Value GetResult(const Napi::Env &env) override
  {
    // TODO call the constructor
    // auto result = generateDevicesResultAndFree(env, devs);
    dev = nullptr; // devs has already been freed
    return env.Null();
  }

private:
  int vendorId;
  int productId;
  std::string serial;
  hid_device *dev;
};

Napi::Value HIDAsync::Create(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  auto appCtx = getAppCtx();
  if (!appCtx)
  {
    Napi::TypeError::New(env, "hidapi not initialized").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 1)
  {
    Napi::TypeError::New(env, "HIDAsync::Create requires at least one arguments").ThrowAsJavaScriptException();
    return env.Null();
  }

  // TODO

  if (info.Length() == 1)
  {
    // open by path
    if (!info[0].IsString())
    {
      Napi::TypeError::New(env, "Device path must be a string").ThrowAsJavaScriptException();
      return env.Null();
    }

    std::string path = info[0].As<Napi::String>().Utf8Value();

    return (new OpenByPathWorker(env, appCtx, path))->QueueAndRun();
  }
  else
  {
    if (!info[0].IsNumber() || !info[1].IsNumber())
    {
      Napi::TypeError::New(env, "VendorId and ProductId must be integers").ThrowAsJavaScriptException();
      return env.Null();
    }

    std::string serial;
    if (info.Length() > 2)
    {
      if (!info[2].IsString())
      {
        Napi::TypeError::New(env, "Serial must be a string").ThrowAsJavaScriptException();
        return env.Null();
      }

      serial = info[2].As<Napi::String>().Utf8Value();
    }

    int32_t vendorId = info[0].As<Napi::Number>().Int32Value();
    int32_t productId = info[1].As<Napi::Number>().Int32Value();

    return (new OpenByUsbIdsWorker(env, appCtx, vendorId, productId, serial))->QueueAndRun();
  }
}

Napi::Value HIDAsync::readStart(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (!helper || _hidHandle->is_closed)
  {
    Napi::TypeError::New(env, "device has been closed").ThrowAsJavaScriptException();
    return env.Null();
  }

  auto callback = info[0].As<Napi::Function>();
  helper->start(env, callback);

  return env.Null();
}

Napi::Value HIDAsync::readStop(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (!helper)
  {
    // Napi::TypeError::New(env, "device has been closed").ThrowAsJavaScriptException();
    return env.Null();
  }

  helper->stop();

  return env.Null();
};

class ReadOnceWorker : public PromiseAsyncWorker<WrappedHidHandle>
{
public:
  ReadOnceWorker(
      Napi::Env &env,
      std::shared_ptr<WrappedHidHandle> hid,
      int timeout)
      : PromiseAsyncWorker(env, hid),
        _timeout(timeout)
  {
  }
  ~ReadOnceWorker()
  {
    if (buffer)
    {
      delete[] buffer;
    }
  }

  // This code will be executed on the worker thread. Note: Napi types cannot be used
  void Execute() override
  {
    if (queue->hid)
    {
      buffer = new unsigned char[READ_BUFF_MAXSIZE];
      // TODO: Is this necessary? Docs say that hid_read_timeout with -1 is 'blocking', but dont clarify what that means when set to nonblocking mode
      if (_timeout == -1)
      {
        returnedLength = hid_read(queue->hid, buffer, READ_BUFF_MAXSIZE);
      }
      else
      {
        returnedLength = hid_read_timeout(queue->hid, buffer, READ_BUFF_MAXSIZE, _timeout);
      }

      if (returnedLength < 0)
      {
        SetError("could not read data from device");
      }
    }
    else
    {
      SetError("device has been closed");
    }
  }

  Napi::Value GetResult(const Napi::Env &env) override
  {
    auto result = convertToNodeOwnerBuffer(env, buffer, returnedLength);
    buffer = nullptr;

    return result;
  }

private:
  int returnedLength = 0;
  unsigned char *buffer;
  int _timeout;
};

Napi::Value HIDAsync::read(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (!_hidHandle || _hidHandle->is_closed)
  {
    Napi::TypeError::New(env, "device has been closed").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (helper != nullptr && helper->run_read)
  {
    Napi::TypeError::New(env, "Cannot use read while async read is running").ThrowAsJavaScriptException();
    return env.Null();
  }

  int timeout = -1;
  if (info.Length() != 0)
  {
    if (info[0].IsNumber())
    {
      timeout = info[0].As<Napi::Number>().Uint32Value();
    }
    else
    {
      Napi::TypeError::New(env, "time out parameter must be a number").ThrowAsJavaScriptException();
      return env.Null();
    }
  }

  return (new ReadOnceWorker(env, _hidHandle, timeout))->QueueAndRun();
}

class GetFeatureReportWorker : public PromiseAsyncWorker<WrappedHidHandle>
{
public:
  GetFeatureReportWorker(
      Napi::Env &env,
      std::shared_ptr<WrappedHidHandle> hid,
      uint8_t reportId,
      int bufSize)
      : PromiseAsyncWorker(env, hid),
        bufferLength(bufSize)
  {
    buffer = new unsigned char[bufSize];
    buffer[0] = reportId;
  }
  ~GetFeatureReportWorker()
  {
    if (buffer)
    {
      delete[] buffer;
    }
  }

  // This code will be executed on the worker thread. Note: Napi types cannot be used
  void Execute() override
  {
    if (queue->hid)
    {
      bufferLength = hid_get_feature_report(queue->hid, buffer, bufferLength);
      if (bufferLength < 0)
      {
        SetError("could not get feature report from device");
      }
    }
    else
    {
      SetError("device has been closed");
    }
  }

  Napi::Value GetResult(const Napi::Env &env) override
  {
    auto result = convertToNodeOwnerBuffer(env, buffer, bufferLength);
    buffer = nullptr;

    return result;
  }

private:
  int written = 0;
  unsigned char *buffer;
  int bufferLength;
};

Napi::Value HIDAsync::getFeatureReport(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (!_hidHandle || _hidHandle->is_closed)
  {
    Napi::TypeError::New(env, "device has been closed").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() != 2 || !info[0].IsNumber() || !info[1].IsNumber())
  {
    Napi::TypeError::New(env, "need report ID and length parameters in getFeatureReport").ThrowAsJavaScriptException();
    return env.Null();
  }

  const uint8_t reportId = info[0].As<Napi::Number>().Uint32Value();
  const int bufSize = info[1].As<Napi::Number>().Uint32Value();
  if (bufSize <= 0)
  {
    Napi::TypeError::New(env, "Length parameter cannot be zero in getFeatureReport").ThrowAsJavaScriptException();
    return env.Null();
  }

  return (new GetFeatureReportWorker(env, _hidHandle, reportId, bufSize))->QueueAndRun();
}

class SendFeatureReportWorker : public PromiseAsyncWorker<WrappedHidHandle>
{
public:
  SendFeatureReportWorker(
      Napi::Env &env,
      std::shared_ptr<WrappedHidHandle> hid,
      std::vector<unsigned char> srcBuffer)
      : PromiseAsyncWorker(env, hid),
        srcBuffer(srcBuffer) {}

  // This code will be executed on the worker thread. Note: Napi types cannot be used
  void Execute() override
  {
    if (queue->hid)
    {
      written = hid_send_feature_report(queue->hid, srcBuffer.data(), srcBuffer.size());
      if (written < 0)
      {
        SetError("could not send feature report to device");
      }
    }
    else
    {
      SetError("device has been closed");
    }
  }

  Napi::Value GetResult(const Napi::Env &env) override
  {
    return Napi::Number::New(env, written);
  }

private:
  int written = 0;
  std::vector<unsigned char> srcBuffer;
};

Napi::Value HIDAsync::sendFeatureReport(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (!_hidHandle || _hidHandle->is_closed)
  {
    Napi::TypeError::New(env, "device has been closed").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() != 1)
  {
    Napi::TypeError::New(env, "need report (including id in first byte) only in sendFeatureReportAsync").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::vector<unsigned char> message;
  std::string copyError = copyArrayOrBufferIntoVector(info[0], message);
  if (copyError != "")
  {
    Napi::TypeError::New(env, copyError).ThrowAsJavaScriptException();
    return env.Null();
  }

  return (new SendFeatureReportWorker(env, _hidHandle, message))->QueueAndRun();
}

Napi::Value HIDAsync::close(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (!_hidHandle || _hidHandle->is_closed)
  {
    Napi::TypeError::New(env, "device is already closed").ThrowAsJavaScriptException();
    return env.Null();
  }

  // TODO - option to flush or purge queued operations

  // Mark it as closed, to stop new jobs being pushed to the queue
  _hidHandle->is_closed = true;

  auto result = (new CloseWorker(env, _hidHandle))->QueueAndRun();

  closeHandle();

  return result;
}

class SetNonBlockingWorker : public PromiseAsyncWorker<WrappedHidHandle>
{
public:
  SetNonBlockingWorker(
      Napi::Env &env,
      std::shared_ptr<WrappedHidHandle> hid,
      int mode)
      : PromiseAsyncWorker(env, hid),
        mode(mode) {}

  // This code will be executed on the worker thread. Note: Napi types cannot be used
  void Execute() override
  {
    if (queue->hid)
    {
      int res = hid_set_nonblocking(queue->hid, mode);
      if (res < 0)
      {
        SetError("Error setting non-blocking mode.");
      }
    }
    else
    {
      SetError("device has been closed");
    }
  }

  Napi::Value GetResult(const Napi::Env &env) override
  {
    return env.Undefined();
  }

private:
  int mode;
};

Napi::Value HIDAsync::setNonBlocking(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (!_hidHandle || _hidHandle->is_closed)
  {
    Napi::TypeError::New(env, "device has been closed").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() != 1)
  {
    Napi::TypeError::New(env, "Expecting a 1 to enable, 0 to disable as the first argument.").ThrowAsJavaScriptException();
    return env.Null();
  }

  int blockStatus = info[0].As<Napi::Number>().Int32Value();

  return (new SetNonBlockingWorker(env, _hidHandle, blockStatus))->QueueAndRun();
}

class WriteWorker : public PromiseAsyncWorker<WrappedHidHandle>
{
public:
  WriteWorker(
      Napi::Env &env,
      std::shared_ptr<WrappedHidHandle> hid,
      std::vector<unsigned char> srcBuffer)
      : PromiseAsyncWorker(env, hid),
        srcBuffer(srcBuffer) {}

  // This code will be executed on the worker thread. Note: Napi types cannot be used
  void Execute() override
  {
    if (queue->hid)
    {
      written = hid_write(queue->hid, srcBuffer.data(), srcBuffer.size());
      if (written < 0)
      {
        SetError("Cannot write to hid device");
      }
    }
    else
    {
      SetError("device has been closed");
    }
  }

  Napi::Value GetResult(const Napi::Env &env) override
  {
    return Napi::Number::New(env, written);
  }

private:
  int written = 0;
  std::vector<unsigned char> srcBuffer;
};

Napi::Value HIDAsync::write(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (!_hidHandle || _hidHandle->is_closed)
  {
    Napi::TypeError::New(env, "device has been closed").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() != 1)
  {
    Napi::TypeError::New(env, "HID write requires one argument").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::vector<unsigned char> message;
  std::string copyError = copyArrayOrBufferIntoVector(info[0], message);
  if (copyError != "")
  {
    Napi::TypeError::New(env, copyError).ThrowAsJavaScriptException();
    return env.Null();
  }

  return (new WriteWorker(env, _hidHandle, std::move(message)))->QueueAndRun();
}

class GetDeviceInfoWorker : public PromiseAsyncWorker<WrappedHidHandle>
{
public:
  GetDeviceInfoWorker(
      Napi::Env &env,
      std::shared_ptr<WrappedHidHandle> hid)
      : PromiseAsyncWorker(env, hid) {}

  // This code will be executed on the worker thread. Note: Napi types cannot be used
  void Execute() override
  {
    if (queue->hid)
    {
      const int maxlen = 256;
      wchar_t wstr[maxlen]; // FIXME: use new & delete

      if (hid_get_manufacturer_string(queue->hid, wstr, maxlen) == 0)
      {
        resManufacturer = narrow(wstr);
      }

      if (hid_get_product_string(queue->hid, wstr, maxlen) == 0)
      {
        resProduct = narrow(wstr);
      }

      if (hid_get_serial_number_string(queue->hid, wstr, maxlen) == 0)
      {
        resSerialNumber = narrow(wstr);
      }
    }
    else
    {
      SetError("device has been closed");
    }
  }

  Napi::Value GetResult(const Napi::Env &env) override
  {
    Napi::Object deviceInfo = Napi::Object::New(env);

    deviceInfo.Set("manufacturer", Napi::String::New(env, resManufacturer));
    deviceInfo.Set("product", Napi::String::New(env, resProduct));
    deviceInfo.Set("serialNumber", Napi::String::New(env, resSerialNumber));

    return deviceInfo;
  }

private:
  std::string resManufacturer;
  std::string resProduct;
  std::string resSerialNumber;
};
Napi::Value HIDAsync::getDeviceInfo(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (!_hidHandle || _hidHandle->is_closed)
  {
    Napi::TypeError::New(env, "device has been closed").ThrowAsJavaScriptException();
    return env.Null();
  }

  return (new GetDeviceInfoWorker(env, _hidHandle))->QueueAndRun();
}

Napi::Value HIDAsync::Initialize(Napi::Env &env)
{
  Napi::Function ctor = DefineClass(env, "HIDAsync", {
                                                         StaticMethod<&HIDAsync::Create>("Create", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),

                                                         InstanceMethod("close", &HIDAsync::close),
                                                         InstanceMethod("readStart", &HIDAsync::readStart),
                                                         InstanceMethod("readStop", &HIDAsync::readStop),
                                                         InstanceMethod("write", &HIDAsync::write, napi_enumerable),
                                                         InstanceMethod("getFeatureReport", &HIDAsync::getFeatureReport, napi_enumerable),
                                                         InstanceMethod("sendFeatureReport", &HIDAsync::sendFeatureReport, napi_enumerable),
                                                         InstanceMethod("setNonBlocking", &HIDAsync::setNonBlocking, napi_enumerable),
                                                         InstanceMethod("read", &HIDAsync::read, napi_enumerable),
                                                         InstanceMethod("getDeviceInfo", &HIDAsync::getDeviceInfo, napi_enumerable),
                                                     });

  return ctor;
}

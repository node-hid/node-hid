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

#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <thread>
#include <atomic>
#include <queue>
#include <chrono>

#include <stdlib.h>

#include "util.h"
#include "HIDAsync.h"
#include "read.h"

HIDAsync::HIDAsync(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<HIDAsync>(info)
{
  Napi::Env env = info.Env();

  if (!info.IsConstructCall())
  {
    Napi::TypeError::New(env, "HID function can only be used as a constructor").ThrowAsJavaScriptException();
    return;
  }

  if (info.Length() < 1)
  {
    Napi::TypeError::New(env, "HID constructor requires at least one arguments").ThrowAsJavaScriptException();
    return;
  }

  if (info.Length() == 1)
  {
    // open by path
    if (!info[0].IsString())
    {
      Napi::TypeError::New(env, "Device path must be a string").ThrowAsJavaScriptException();
      return;
    }

    std::string path = info[0].As<Napi::String>().Utf8Value();
    auto hidHandle = hid_open_path(path.c_str());
    if (!hidHandle)
    {
      std::ostringstream os;
      os << "cannot open device with path " << path;
      Napi::TypeError::New(env, os.str()).ThrowAsJavaScriptException();
      return;
    }

    _hidHandle = std::make_shared<WrappedHidHandle>(hidHandle);
  }
  else
  {
    int32_t vendorId = info[0].As<Napi::Number>().Int32Value();
    int32_t productId = info[1].As<Napi::Number>().Int32Value();
    wchar_t wserialstr[100]; // FIXME: is there a better way?
    wchar_t *wserialptr = NULL;
    if (info.Length() > 2)
    {
      std::string serialstr = info[2].As<Napi::String>().Utf8Value();
      mbstowcs(wserialstr, serialstr.c_str(), 100);
      wserialptr = wserialstr;
    }

    auto hidHandle = hid_open(vendorId, productId, wserialptr);
    if (!hidHandle)
    {
      std::ostringstream os;
      os << "cannot open device with vendor id 0x" << std::hex << vendorId << " and product id 0x" << productId;
      Napi::TypeError::New(env, os.str()).ThrowAsJavaScriptException();
      return;
    }
    _hidHandle = std::make_shared<WrappedHidHandle>(hidHandle);
  }

  helper = std::make_shared<ReadHelper>(_hidHandle);
}

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

Napi::Value HIDAsync::readStart(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (!helper)
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

class MyAsyncWorker : public Napi::AsyncWorker
{
public:
  MyAsyncWorker(
      Napi::Env &env,
      std::shared_ptr<WrappedHidHandle> hid)
      : Napi::AsyncWorker(env),
        _hid(hid),
        deferred(Napi::Promise::Deferred::New(env))
  {
  }

  // This code will be executed on the worker thread. Note: Napi types cannot be used
  virtual void Execute() override;

  virtual Napi::Value GetResult(const Napi::Env &env) = 0;

  void OnOK() override
  {
    Napi::Env env = Env();
    _hid->JobFinished(env);

    deferred.Resolve(GetResult(env));
  }
  void OnError(Napi::Error const &error) override
  {
    _hid->JobFinished(Env());
    deferred.Reject(error.Value());
  }

  Napi::Promise GetPromise() const
  {
    return deferred.Promise();
  }

protected:
  const std::shared_ptr<WrappedHidHandle> _hid;

private:
  Napi::Promise::Deferred deferred;
};

class ReadOnceWorker : public MyAsyncWorker
{
public:
  ReadOnceWorker(
      Napi::Env &env,
      std::shared_ptr<WrappedHidHandle> hid,
      int timeout)
      : MyAsyncWorker(env, hid),
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
    if (_hid)
    {
      buffer = new unsigned char[READ_BUFF_MAXSIZE];
      // TODO: Is this necessary? Docs say that hid_read_timeout with -1 is 'blocking', but dont clarify what that means when set to nonblocking mode
      if (_timeout == -1)
      {
        returnedLength = hid_read(_hid->hid, buffer, READ_BUFF_MAXSIZE);
      }
      else
      {
        returnedLength = hid_read_timeout(_hid->hid, buffer, READ_BUFF_MAXSIZE, _timeout);
      }

      if (returnedLength < 0)
      {
        SetError("could not read data from device");
      }
    }
    else
    {
      SetError("No hid handle");
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

  if (!_hidHandle)
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

  auto job = new ReadOnceWorker(env, _hidHandle, timeout);

  _hidHandle->QueueJob(env, job);

  return job->GetPromise();
}

class GetFeatureReportWorker : public MyAsyncWorker
{
public:
  GetFeatureReportWorker(
      Napi::Env &env,
      std::shared_ptr<WrappedHidHandle> hid,
      uint8_t reportId,
      int bufSize)
      : MyAsyncWorker(env, hid),
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
    if (_hid)
    {
      bufferLength = hid_get_feature_report(_hid->hid, buffer, bufferLength);
      if (bufferLength < 0)
      {
        SetError("could not get feature report from device");
      }
    }
    else
    {
      SetError("No hid handle");
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

  if (!_hidHandle)
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

  auto job = new GetFeatureReportWorker(env, _hidHandle, reportId, bufSize);

  _hidHandle->QueueJob(env, job);

  return job->GetPromise();
}

class SendFeatureReportWorker : public MyAsyncWorker
{
public:
  SendFeatureReportWorker(
      Napi::Env &env,
      std::shared_ptr<WrappedHidHandle> hid,
      std::vector<unsigned char> srcBuffer)
      : MyAsyncWorker(env, hid),
        srcBuffer(srcBuffer) {}

  // This code will be executed on the worker thread. Note: Napi types cannot be used
  void Execute() override
  {
    if (_hid)
    {
      written = hid_send_feature_report(_hid->hid, srcBuffer.data(), srcBuffer.size());
      if (written < 0)
      {
        SetError("could not send feature report to device");
      }
    }
    else
    {
      SetError("No hid handle");
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

  if (!_hidHandle)
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

  auto job = new SendFeatureReportWorker(env, _hidHandle, message);

  _hidHandle->QueueJob(env, job);

  return job->GetPromise();
}

Napi::Value HIDAsync::close(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  this->closeHandle();

  return env.Null();
}

class SetNonBlockingWorker : public MyAsyncWorker
{
public:
  SetNonBlockingWorker(
      Napi::Env &env,
      std::shared_ptr<WrappedHidHandle> hid,
      int mode)
      : MyAsyncWorker(env, hid),
        mode(mode) {}

  // This code will be executed on the worker thread. Note: Napi types cannot be used
  void Execute() override
  {
    if (_hid)
    {
      int res = hid_set_nonblocking(_hid->hid, mode);
      if (res < 0)
      {
        SetError("Error setting non-blocking mode.");
      }
    }
    else
    {
      SetError("No hid handle");
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

  if (!_hidHandle)
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

  auto job = new SetNonBlockingWorker(env, _hidHandle, blockStatus);

  _hidHandle->QueueJob(env, job);

  return job->GetPromise();
}

class WriteWorker : public MyAsyncWorker
{
public:
  WriteWorker(
      Napi::Env &env,
      std::shared_ptr<WrappedHidHandle> hid,
      std::vector<unsigned char> srcBuffer)
      : MyAsyncWorker(env, hid),
        srcBuffer(srcBuffer) {}

  // This code will be executed on the worker thread. Note: Napi types cannot be used
  void Execute() override
  {
    if (_hid)
    {
      written = hid_write(_hid->hid, srcBuffer.data(), srcBuffer.size());
      if (written < 0)
      {
        SetError("Cannot write to hid device");
      }
    }
    else
    {
      SetError("No hid handle");
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

  if (!_hidHandle)
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

  auto job = new WriteWorker(env, _hidHandle, std::move(message));

  _hidHandle->QueueJob(env, job);

  return job->GetPromise();
}

static std::string narrow(wchar_t *wide)
{
  std::wstring ws(wide);
  std::ostringstream os;
  for (size_t i = 0; i < ws.size(); i++)
  {
    os << os.narrow(ws[i], '?');
  }
  return os.str();
}

class GetDeviceInfoWorker : public MyAsyncWorker
{
public:
  GetDeviceInfoWorker(
      Napi::Env &env,
      std::shared_ptr<WrappedHidHandle> hid)
      : MyAsyncWorker(env, hid) {}

  // This code will be executed on the worker thread. Note: Napi types cannot be used
  void Execute() override
  {
    if (_hid)
    {
      const int maxlen = 256;
      wchar_t wstr[maxlen]; // FIXME: use new & delete

      if (hid_get_manufacturer_string(_hid->hid, wstr, maxlen) == 0)
      {
        resManufacturer = narrow(wstr);
      }

      if (hid_get_product_string(_hid->hid, wstr, maxlen) == 0)
      {
        resProduct = narrow(wstr);
      }

      if (hid_get_serial_number_string(_hid->hid, wstr, maxlen) == 0)
      {
        resSerialNumber = narrow(wstr);
      }
    }
    else
    {
      SetError("No hid handle");
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

  if (!_hidHandle)
  {
    Napi::TypeError::New(env, "device has been closed").ThrowAsJavaScriptException();
    return env.Null();
  }

  auto job = new GetDeviceInfoWorker(env, _hidHandle);

  _hidHandle->QueueJob(env, job);

  return job->GetPromise();
}

Napi::Value HIDAsync::devices(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  int vendorId = 0;
  int productId = 0;

  switch (info.Length())
  {
  case 0:
    break;
  case 2:
    vendorId = info[0].As<Napi::Number>().Int32Value();
    productId = info[1].As<Napi::Number>().Int32Value();
    break;
  default:
    Napi::TypeError::New(env, "unexpected number of arguments to HID.devices() call, expecting either no arguments or vendor and product ID").ThrowAsJavaScriptException();
    return env.Null();
  }

  hid_device_info *devs = hid_enumerate(vendorId, productId);
  Napi::Array retval = Napi::Array::New(env);
  int count = 0;
  for (hid_device_info *dev = devs; dev; dev = dev->next)
  {
    Napi::Object deviceInfo = Napi::Object::New(env);
    deviceInfo.Set("vendorId", Napi::Number::New(env, dev->vendor_id));
    deviceInfo.Set("productId", Napi::Number::New(env, dev->product_id));
    if (dev->path)
    {
      deviceInfo.Set("path", Napi::String::New(env, dev->path));
    }
    if (dev->serial_number)
    {
      deviceInfo.Set("serialNumber", Napi::String::New(env, narrow(dev->serial_number)));
    }
    if (dev->manufacturer_string)
    {
      deviceInfo.Set("manufacturer", Napi::String::New(env, narrow(dev->manufacturer_string)));
    }
    if (dev->product_string)
    {
      deviceInfo.Set("product", Napi::String::New(env, narrow(dev->product_string)));
    }
    deviceInfo.Set("release", Napi::Number::New(env, dev->release_number));
    deviceInfo.Set("interface", Napi::Number::New(env, dev->interface_number));
    if (dev->usage_page)
    {
      deviceInfo.Set("usagePage", Napi::Number::New(env, dev->usage_page));
    }
    if (dev->usage)
    {
      deviceInfo.Set("usage", Napi::Number::New(env, dev->usage));
    }
    retval.Set(count++, deviceInfo);
  }
  hid_free_enumeration(devs);
  return retval;
}

Napi::Value HIDAsync::Initialize(Napi::Env &env)
{
  Napi::Function ctor = DefineClass(env, "HIDAsync", {
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

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
#include "read.h"

class HIDAsync : public Napi::ObjectWrap<HIDAsync>
{
public:
  static void Initialize(Napi::Env &env, Napi::Object &exports);

  void closeHandle();

  HIDAsync(const Napi::CallbackInfo &info);
  ~HIDAsync() { closeHandle(); }

  std::shared_ptr<WrappedHidHandle> _hidHandle;

private:
  static Napi::Value devices(const Napi::CallbackInfo &info);

  std::shared_ptr<ReadHelper> helper;

  Napi::Value close(const Napi::CallbackInfo &info);
  Napi::Value readStart(const Napi::CallbackInfo &info);
  Napi::Value readStop(const Napi::CallbackInfo &info);
  Napi::Value write(const Napi::CallbackInfo &info); // Asynced
  Napi::Value setNonBlocking(const Napi::CallbackInfo &info);
  Napi::Value getFeatureReport(const Napi::CallbackInfo &info);
  Napi::Value sendFeatureReport(const Napi::CallbackInfo &info); // Asynced
  Napi::Value readSync(const Napi::CallbackInfo &info);
  Napi::Value readTimeout(const Napi::CallbackInfo &info);
  Napi::Value getDeviceInfo(const Napi::CallbackInfo &info);
};

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

Napi::Value HIDAsync::readSync(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (!_hidHandle)
  {
    Napi::TypeError::New(env, "device has been closed").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (helper != nullptr && helper->run_read)
  {
    Napi::TypeError::New(env, "Cannot use readSync while async read is running").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() != 0)
  {
    Napi::TypeError::New(env, "readSync needs zero length parameter").ThrowAsJavaScriptException();
    return env.Null();
  }

  unsigned char buff_read[READ_BUFF_MAXSIZE];
  int returnedLength = hid_read(_hidHandle->hid, buff_read, sizeof buff_read);
  if (returnedLength == -1)
  {
    Napi::TypeError::New(env, "could not read data from device").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Array retval = Napi::Array::New(env, returnedLength);
  for (int i = 0; i < returnedLength; i++)
  {
    retval.Set(i, Napi::Number::New(env, buff_read[i]));
  }
  return retval;
}

Napi::Value HIDAsync::readTimeout(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (!_hidHandle)
  {
    Napi::TypeError::New(env, "device has been closed").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (helper != nullptr && helper->run_read)
  {
    Napi::TypeError::New(env, "Cannot use readSync while async read is running").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() != 1 || !info[0].IsNumber())
  {
    Napi::TypeError::New(env, "readTimeout needs time out parameter").ThrowAsJavaScriptException();
    return env.Null();
  }

  const int timeout = info[0].As<Napi::Number>().Uint32Value();
  unsigned char buff_read[READ_BUFF_MAXSIZE];
  int returnedLength = hid_read_timeout(_hidHandle->hid, buff_read, sizeof buff_read, timeout);
  if (returnedLength == -1)
  {
    Napi::TypeError::New(env, "could not read data from device").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Array retval = Napi::Array::New(env, returnedLength);
  for (int i = 0; i < returnedLength; i++)
  {
    retval.Set(i, Napi::Number::New(env, buff_read[i]));
  }
  return retval;
}

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
  if (bufSize == 0)
  {
    Napi::TypeError::New(env, "Length parameter cannot be zero in getFeatureReport").ThrowAsJavaScriptException();
    return env.Null();
  }

  unsigned char *buf = new unsigned char[bufSize];
  buf[0] = reportId;

  int returnedLength = hid_get_feature_report(_hidHandle->hid, buf, bufSize);
  if (returnedLength == -1)
  {
    delete[] buf;
    Napi::TypeError::New(env, "could not get feature report from device").ThrowAsJavaScriptException();
    return env.Null();
  }

  // Pass ownership of `buf` to the Buffer
  return convertToNodeOwnerBuffer(env, buf, returnedLength);
}

class SendFeatureReportWorker : public Napi::AsyncWorker
{
public:
  SendFeatureReportWorker(
      Napi::Env &env,
      std::shared_ptr<WrappedHidHandle> hid,
      std::vector<unsigned char> srcBuffer)
      : Napi::AsyncWorker(env),
        _hid(hid),
        deferred(Napi::Promise::Deferred::New(env)),
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

  void OnOK() override
  {
    _hid->JobFinished(Env());
    deferred.Resolve(Napi::Number::New(Env(), written));
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

private:
  std::shared_ptr<WrappedHidHandle> _hid;
  int written = 0;
  Napi::Promise::Deferred deferred;
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

  int res = hid_set_nonblocking(_hidHandle->hid, blockStatus);
  if (res < 0)
  {
    Napi::TypeError::New(env, "Error setting non-blocking mode.").ThrowAsJavaScriptException();
    return env.Null();
  }

  return env.Null();
}

class WriteWorker : public Napi::AsyncWorker
{
public:
  WriteWorker(
      Napi::Env &env,
      std::shared_ptr<WrappedHidHandle> hid,
      std::vector<unsigned char> srcBuffer)
      : Napi::AsyncWorker(env),
        _hid(hid),
        deferred(Napi::Promise::Deferred::New(env)),
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

  void OnOK() override
  {
    _hid->JobFinished(Env());
    deferred.Resolve(Napi::Number::New(Env(), written));
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

private:
  std::shared_ptr<WrappedHidHandle> _hid;
  int written = 0;
  Napi::Promise::Deferred deferred;
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

Napi::Value HIDAsync::getDeviceInfo(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (!_hidHandle)
  {
    Napi::TypeError::New(env, "device has been closed").ThrowAsJavaScriptException();
    return env.Null();
  }

  const int maxlen = 256;
  wchar_t wstr[maxlen]; // FIXME: use new & delete

  Napi::Object deviceInfo = Napi::Object::New(env);

  hid_get_manufacturer_string(_hidHandle->hid, wstr, maxlen);
  deviceInfo.Set("manufacturer", Napi::String::New(env, narrow(wstr)));

  hid_get_product_string(_hidHandle->hid, wstr, maxlen);
  deviceInfo.Set("product", Napi::String::New(env, narrow(wstr)));

  hid_get_serial_number_string(_hidHandle->hid, wstr, maxlen);
  deviceInfo.Set("serialNumber", Napi::String::New(env, narrow(wstr)));

  return deviceInfo;
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

// // Ensure hid_init/hid_exit is coordinated across all threads
// std::mutex initLock;
// uint16_t activeThreads = 0;

// static void
// deinitialize(void *)
// {
//   // Make sure we run init on only one thread
//   std::unique_lock<std::mutex> lock(initLock);

//   activeThreads--;

//   if (activeThreads == 0)
//   {
//     // TODO: libusb might be grumpy about this. Is it being called before the hid devices have been disposed?
//     if (hid_exit())
//     {
//       // thread is exiting, can't log? TODO
//       // Napi::TypeError::New(env, "cannot uninitialize hidapi (hid_exit failed)").ThrowAsJavaScriptException();
//       return;
//     }
//   }
// }
void HIDAsync::Initialize(Napi::Env &env, Napi::Object &exports)
{
  // std::shared_ptr<void> ref;
  // {
  //   // Make sure we run init on only one thread
  //   std::unique_lock<std::mutex> lock(initLock);

  //   if (activeThreads == 0)
  //   {
  //     // Not initialised, so lets do that
  //     if (hid_init())
  //     {
  //       Napi::TypeError::New(env, "cannot initialize hidapi (hid_init failed)").ThrowAsJavaScriptException();
  //       return;
  //     }
  //   }

  //   activeThreads++;
  // }

  // napi_add_env_cleanup_hook(env, deinitialize, nullptr);

  Napi::Function ctor = DefineClass(env, "HID", {
                                                    InstanceMethod("close", &HIDAsync::close),
                                                    InstanceMethod("readStart", &HIDAsync::readStart),
                                                    InstanceMethod("readStop", &HIDAsync::readStop),
                                                    InstanceMethod("write", &HIDAsync::write, napi_enumerable),
                                                    InstanceMethod("getFeatureReport", &HIDAsync::getFeatureReport, napi_enumerable),
                                                    InstanceMethod("sendFeatureReport", &HIDAsync::sendFeatureReport, napi_enumerable),
                                                    InstanceMethod("setNonBlocking", &HIDAsync::setNonBlocking, napi_enumerable),
                                                    InstanceMethod("readSync", &HIDAsync::readSync, napi_enumerable),
                                                    InstanceMethod("readTimeout", &HIDAsync::readTimeout, napi_enumerable),
                                                    InstanceMethod("getDeviceInfo", &HIDAsync::getDeviceInfo, napi_enumerable),
                                                });

  exports.Set("HID", ctor);
  exports.Set("devices", Napi::Function::New(env, &HIDAsync::devices));
}

// Napi::Object Init(Napi::Env env, Napi::Object exports)
// {
//   HIDAsync::Initialize(env, exports);

//   return exports;
// }

// NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)

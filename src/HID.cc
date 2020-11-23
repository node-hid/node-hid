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

#include <stdlib.h>

#define NAPI_VERSION 3
#include <napi.h>

#include <hidapi.h>

#define READ_BUFF_MAXSIZE 2048

class HID : public Napi::ObjectWrap<HID>
{
public:
  static void Initialize(Napi::Env &env, Napi::Object &exports);

  void closeHandle();

  HID(const Napi::CallbackInfo &info);
  ~HID() { closeHandle(); }

  hid_device *_hidHandle;

private:
  static Napi::Value devices(const Napi::CallbackInfo &info);

  Napi::Value close(const Napi::CallbackInfo &info);
  Napi::Value read(const Napi::CallbackInfo &info);
  Napi::Value write(const Napi::CallbackInfo &info);
  Napi::Value setNonBlocking(const Napi::CallbackInfo &info);
  Napi::Value getFeatureReport(const Napi::CallbackInfo &info);
  Napi::Value sendFeatureReport(const Napi::CallbackInfo &info);
  Napi::Value readSync(const Napi::CallbackInfo &info);
  Napi::Value readTimeout(const Napi::CallbackInfo &info);
  Napi::Value getDeviceInfo(const Napi::CallbackInfo &info);
};

HID::HID(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<HID>(info)
{
  Napi::Env env = info.Env();

  if (!info.IsConstructCall())
  {
    Napi::TypeError::New(env, "HID function can only be used as a constructor").ThrowAsJavaScriptException();
    return;
  }

  if (info.Length() < 1)
  {
    Napi::TypeError::New(env, "HID constructor requires at least one argument").ThrowAsJavaScriptException();
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
    _hidHandle = hid_open_path(path.c_str());
    if (!_hidHandle)
    {
      std::ostringstream os;
      os << "cannot open device with path " << path;
      Napi::TypeError::New(env, os.str()).ThrowAsJavaScriptException();
      return;
    }
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

    _hidHandle = hid_open(vendorId, productId, wserialptr);
    if (!_hidHandle)
    {
      std::ostringstream os;
      os << "cannot open device with vendor id 0x" << std::hex << vendorId << " and product id 0x" << productId;
      Napi::TypeError::New(env, os.str()).ThrowAsJavaScriptException();
      return;
    }
  }
}

void HID::closeHandle()
{
  if (_hidHandle)
  {
    hid_close(_hidHandle);
    _hidHandle = 0;
  }
}

void deleteArray(const Napi::Env &env, unsigned char *ptr)
{
  delete[] ptr;
}

class ReadWorker : public Napi::AsyncWorker
{
public:
  ReadWorker(HID *hid, Napi::Function &callback)
      : Napi::AsyncWorker(hid->Value(), callback), _hid(hid) {}

  ~ReadWorker()
  {
    if (buf != nullptr)
    {
      delete[] buf;
    }
  }
  // This code will be executed on the worker thread
  void Execute() override
  {
    int mswait = 50;
    while (len == 0 && _hid->_hidHandle)
    {
      len = hid_read_timeout(_hid->_hidHandle, buf, READ_BUFF_MAXSIZE, mswait);
    }
    if (len <= 0)
    {
      SetError("could not read from HID device");
    }
  }

  void OnOK() override
  {
    auto buffer = Napi::Buffer<unsigned char>::New(Env(), buf, len, deleteArray);
    buf = nullptr; // It is now owned by the buffer
    Callback().Call({Env().Null(), buffer});
  }

private:
  HID *_hid;
  unsigned char *buf = new unsigned char[READ_BUFF_MAXSIZE];
  int len = 0;
};

Napi::Value HID::read(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (info.Length() != 1 || !info[0].IsFunction())
  {
    Napi::TypeError::New(env, "need one callback function argument in read").ThrowAsJavaScriptException();
    return env.Null();
  }

  auto callback = info[0].As<Napi::Function>();
  auto job = new ReadWorker(this, callback);
  job->Queue();

  return env.Null();
}

Napi::Value HID::readSync(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (info.Length() != 0)
  {
    Napi::TypeError::New(env, "readSync needs zero length parameter").ThrowAsJavaScriptException();
    return env.Null();
  }

  unsigned char buff_read[READ_BUFF_MAXSIZE];
  int returnedLength = hid_read(_hidHandle, buff_read, sizeof buff_read);
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

Napi::Value HID::readTimeout(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (info.Length() != 1 || !info[0].IsNumber())
  {
    Napi::TypeError::New(env, "readTimeout needs time out parameter").ThrowAsJavaScriptException();
    return env.Null();
  }

  const int timeout = info[0].As<Napi::Number>().Uint32Value();
  unsigned char buff_read[READ_BUFF_MAXSIZE];
  int returnedLength = hid_read_timeout(_hidHandle, buff_read, sizeof buff_read, timeout);
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

Napi::Value HID::getFeatureReport(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

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

  std::vector<unsigned char> buf(bufSize);
  buf[0] = reportId;

  int returnedLength = hid_get_feature_report(_hidHandle, buf.data(), bufSize);
  if (returnedLength == -1)
  {
    Napi::TypeError::New(env, "could not get feature report from device").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Array retval = Napi::Array::New(env, returnedLength);
  for (int i = 0; i < returnedLength; i++)
  {
    retval.Set(i, Napi::Number::New(env, buf[i]));
  }
  return retval;
}

Napi::Value HID::sendFeatureReport(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (info.Length() != 1)
  {
    Napi::TypeError::New(env, "need report (including id in first byte) only in sendFeatureReport").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::vector<unsigned char> message;
  if (info[0].IsBuffer())
  {
    Napi::Buffer<unsigned char> buffer = info[0].As<Napi::Buffer<unsigned char>>();
    uint32_t len = buffer.Length();
    unsigned char *data = buffer.Data();
    message.assign(data, data + len);
  }
  else if (info[0].IsArray())
  {
    Napi::Array messageArray = info[0].As<Napi::Array>();
    message.reserve(messageArray.Length());

    for (unsigned i = 0; i < messageArray.Length(); i++)
    {
      Napi::Value v = messageArray.Get(i);
      if (!v.IsNumber())
      {
        Napi::TypeError::New(env, "unexpected array element in array to send, expecting only integers").ThrowAsJavaScriptException();
        return env.Null();
      }
      uint32_t b = v.As<Napi::Number>().Uint32Value();
      message.push_back((unsigned char)b);
    }
  }
  else
  {
    Napi::TypeError::New(env, "unexpected data to send, expecting an array or buffer").ThrowAsJavaScriptException();
    return env.Null();
  }

  int returnedLength = hid_send_feature_report(_hidHandle, message.data(), message.size());
  if (returnedLength == -1)
  { // Not sure if there would ever be a valid return value of 0.
    Napi::TypeError::New(env, "could not send feature report to device").ThrowAsJavaScriptException();
    return env.Null();
  }

  return Napi::Number::New(env, returnedLength);
}

Napi::Value HID::close(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  this->closeHandle();
  return env.Null();
}

Napi::Value HID::setNonBlocking(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (info.Length() != 1)
  {
    Napi::TypeError::New(env, "Expecting a 1 to enable, 0 to disable as the first argument.").ThrowAsJavaScriptException();
    return env.Null();
  }

  int blockStatus = info[0].As<Napi::Number>().Int32Value();
  int res = hid_set_nonblocking(_hidHandle, blockStatus);
  if (res < 0)
  {
    Napi::TypeError::New(env, "Error setting non-blocking mode.").ThrowAsJavaScriptException();
    return env.Null();
  }

  return env.Null();
}

Napi::Value HID::write(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (info.Length() != 1)
  {
    Napi::TypeError::New(env, "HID write requires one argument").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::vector<unsigned char> message;
  if (info[0].IsBuffer())
  {
    Napi::Buffer<unsigned char> buffer = info[0].As<Napi::Buffer<unsigned char>>();
    uint32_t len = buffer.Length();
    unsigned char *data = buffer.Data();
    message.assign(data, data + len);
  }
  else if (info[0].IsArray())
  {
    Napi::Array messageArray = info[0].As<Napi::Array>();
    message.reserve(messageArray.Length());

    for (unsigned i = 0; i < messageArray.Length(); i++)
    {
      Napi::Value v = messageArray.Get(i);
      if (!v.IsNumber())
      {
        Napi::TypeError::New(env, "unexpected array element in array to send, expecting only integers").ThrowAsJavaScriptException();
        return env.Null();
      }
      uint32_t b = v.As<Napi::Number>().Uint32Value();
      message.push_back((unsigned char)b);
    }
  }
  else
  {
    Napi::TypeError::New(env, "unexpected data to send, expecting an array or buffer").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (!_hidHandle)
  {
    Napi::TypeError::New(env, "Cannot write to closed device").ThrowAsJavaScriptException();
    return env.Null();
  }

  int returnedLength = hid_write(_hidHandle, message.data(), message.size());
  if (returnedLength < 0)
  {
    Napi::TypeError::New(env, "Cannot write to hid device").ThrowAsJavaScriptException();
    return env.Null();
  }

  return Napi::Number::New(env, returnedLength);
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

Napi::Value HID::getDeviceInfo(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  const int maxlen = 256;
  wchar_t wstr[maxlen]; // FIXME: use new & delete

  Napi::Object deviceInfo = Napi::Object::New(env);

  hid_get_manufacturer_string(_hidHandle, wstr, maxlen);
  deviceInfo.Set("manufacturer", Napi::String::New(env, narrow(wstr)));

  hid_get_product_string(_hidHandle, wstr, maxlen);
  deviceInfo.Set("product", Napi::String::New(env, narrow(wstr)));

  hid_get_serial_number_string(_hidHandle, wstr, maxlen);
  deviceInfo.Set("serialNumber", Napi::String::New(env, narrow(wstr)));

  return deviceInfo;
}

Napi::Value HID::devices(const Napi::CallbackInfo &info)
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

static void
deinitialize(void *)
{
  if (hid_exit())
  {
    // Process is exiting, no need to log? TODO
    // Napi::TypeError::New(env, "cannot uninitialize hidapi (hid_exit failed)").ThrowAsJavaScriptException();
    return;
  }
}
void HID::Initialize(Napi::Env &env, Napi::Object &exports)
{
  if (hid_init())
  {
    Napi::TypeError::New(env, "cannot initialize hidapi (hid_init failed)").ThrowAsJavaScriptException();
    return;
  }

  napi_add_env_cleanup_hook(env, deinitialize, nullptr);

  Napi::Function ctor = DefineClass(env, "HID", {
                                                    InstanceMethod("close", &HID::close),
                                                    InstanceMethod("read", &HID::read),
                                                    InstanceMethod("write", &HID::write, napi_enumerable),
                                                    InstanceMethod("getFeatureReport", &HID::getFeatureReport, napi_enumerable),
                                                    InstanceMethod("sendFeatureReport", &HID::sendFeatureReport, napi_enumerable),
                                                    InstanceMethod("setNonBlocking", &HID::setNonBlocking, napi_enumerable),
                                                    InstanceMethod("readSync", &HID::readSync, napi_enumerable),
                                                    InstanceMethod("readTimeout", &HID::readTimeout, napi_enumerable),
                                                    InstanceMethod("getDeviceInfo", &HID::getDeviceInfo, napi_enumerable),
                                                });

  exports.Set("HID", ctor);
  exports.Set("devices", Napi::Function::New(env, &HID::devices));
}

Napi::Object Init(Napi::Env env, Napi::Object exports)
{
  HID::Initialize(env, exports);

  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)

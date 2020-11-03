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

using namespace std;
// using namespace v8;
// using namespace node;
using namespace Napi;

#define READ_BUFF_MAXSIZE 2048

// //////////////////////////////////////////////////////////////////
// Throwable error class that can be converted to a JavaScript
// exception
// //////////////////////////////////////////////////////////////////
class JSException
{
public:
  JSException(const string &text) : _message(text) {}
  virtual ~JSException() {}
  virtual const string message() const { return _message; }
  virtual void asV8Exception(const Napi::Env &env) const
  {
    TypeError::New(env, message()).ThrowAsJavaScriptException();
  }

protected:
  string _message;
};

class HID
    : public Napi::ObjectWrap<HID>
{
public:
  static void Initialize(Napi::Env &env, Napi::Object &exports);
  //static NAN_METHOD(devices);

  typedef vector<unsigned char> databuf_t;

  // int write(const databuf_t &message);
  void close();

  HID(const Napi::CallbackInfo &info);
  ~HID() { close(); }

private:
  Napi::Value Close(const Napi::CallbackInfo &info);
  static Napi::Value Devices(const Napi::CallbackInfo &info);

  Napi::Value write(const Napi::CallbackInfo &info);
  Napi::Value setNonBlocking(const Napi::CallbackInfo &info);
  /*
  static NAN_METHOD(read);
  static NAN_METHOD(write);
  static NAN_METHOD(setNonBlocking);
  static NAN_METHOD(getFeatureReport);
  static NAN_METHOD(readSync);
  static NAN_METHOD(readTimeout);
  */
  Napi::Value getFeatureReport(const Napi::CallbackInfo &info);
  Napi::Value sendFeatureReport(const Napi::CallbackInfo &info);
  Napi::Value readSync(const Napi::CallbackInfo &info);
  Napi::Value readTimeout(const Napi::CallbackInfo &info);
  Napi::Value getDeviceInfo(const Napi::CallbackInfo &info);

  /*
  static void recvAsync(uv_work_t *req);
  static void recvAsyncDone(uv_work_t *req);

  */
  struct ReceiveIOCB
  {
    ReceiveIOCB(HID *hid, Napi::Function *callback)
        : _hid(hid),
          _callback(callback),
          _error(0)
    {
    }

    ~ReceiveIOCB()
    {
      if (_error)
      {
        delete _error;
      }
    }

    HID *_hid;
    Napi::Function *_callback;
    JSException *_error;
    vector<unsigned char> _data;
  };

  //void readResultsToJSCallbackArguments(ReceiveIOCB *iocb, Local<Value> argv[]);

  hid_device *_hidHandle;
};

HID::HID(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<HID>(info)
{
  Napi::Env env = info.Env();

  if (!info.IsConstructCall())
  {
    TypeError::New(env, "HID function can only be used as a constructor").ThrowAsJavaScriptException();
    return;
  }

  if (info.Length() < 1)
  {
    TypeError::New(env, "HID constructor requires at least one argument").ThrowAsJavaScriptException();
    return;
  }

  try
  {
    if (info.Length() == 1)
    {
      // open by path
      if (!info[0].IsString())
      {
        throw JSException("Device path must be a string");
      }

      std::string path = info[0].As<Napi::String>().Utf8Value();
      _hidHandle = hid_open_path(path.c_str());
      if (!_hidHandle)
      {
        ostringstream os;
        os << "cannot open device with path " << path;
        throw JSException(os.str());
      }
    }
    else
    {
      // TODO: are these safe with invalid data?
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
        ostringstream os;
        os << "cannot open device with vendor id 0x" << hex << vendorId << " and product id 0x" << productId;
        throw JSException(os.str());
      }
    }
  }
  catch (const JSException &e)
  {
    e.asV8Exception(env);
  }
}

void HID::close()
{
  if (_hidHandle)
  {
    hid_close(_hidHandle);
    _hidHandle = 0;
  }
}

/*
void HID::recvAsync(uv_work_t *req)
{
  ReceiveIOCB *iocb = static_cast<ReceiveIOCB *>(req->data);
  HID *hid = iocb->_hid;

  unsigned char buf[READ_BUFF_MAXSIZE];
  int mswait = 50;
  int len = 0;
  while (len == 0 && hid->_hidHandle)
  {
    len = hid_read_timeout(hid->_hidHandle, buf, sizeof buf, mswait);
  }
  if (len < 0)
  {
    iocb->_error = new JSException("could not read from HID device");
  }
  else
  {
    iocb->_data = vector<unsigned char>(buf, buf + len);
  }
}

void HID::readResultsToJSCallbackArguments(ReceiveIOCB *iocb, Local<Value> argv[])
{
  if (iocb->_error)
  {
    argv[0] = Exception::Error(Nan::New<String>(iocb->_error->message().c_str()).ToLocalChecked());
  }
  else
  {
    const vector<unsigned char> &message = iocb->_data;

    Local<Object> buf = Nan::NewBuffer(message.size()).ToLocalChecked();
    char *data = Buffer::Data(buf);

    int j = 0;
    for (vector<unsigned char>::const_iterator k = message.begin(); k != message.end(); k++)
    {
      data[j++] = *k;
    }
    argv[1] = buf;
  }
}

void HID::recvAsyncDone(uv_work_t *req)
{
  Nan::HandleScope scope;
  ReceiveIOCB *iocb = static_cast<ReceiveIOCB *>(req->data);

  Local<Value> argv[2];
  argv[0] = Nan::Undefined();
  argv[1] = Nan::Undefined();

  iocb->_hid->readResultsToJSCallbackArguments(iocb, argv);
  iocb->_hid->Unref();

  Nan::TryCatch tryCatch;
  //iocb->_callback->Call(2, argv);
  Nan::AsyncResource resource("node-hid recvAsyncDone");
  iocb->_callback->Call(2, argv, &resource);
  if (tryCatch.HasCaught())
  {
    Nan::FatalException(tryCatch);
  }

  delete req;
  delete iocb->_callback;

  delete iocb;
}

NAN_METHOD(HID::read)
{
  Nan::HandleScope scope;

  if (info.Length() != 1 || !info[0]->IsFunction())
  {
    return Nan::ThrowError("need one callback function argument in read");
  }

  HID *hid = Nan::ObjectWrap::Unwrap<HID>(info.This());
  hid->Ref();

  uv_work_t *req = new uv_work_t;
  req->data = new ReceiveIOCB(hid, new Nan::Callback(Local<Function>::Cast(info[0])));
  ;
#if NODE_MAJOR_VERSION >= 10
  uv_queue_work(Nan::GetCurrentEventLoop(), req, recvAsync, (uv_after_work_cb)recvAsyncDone);
#else
  uv_queue_work(uv_default_loop(), req, recvAsync, (uv_after_work_cb)recvAsyncDone);
#endif
  return;
}

*/
Napi::Value HID::readSync(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (info.Length() != 0)
  {
    TypeError::New(env, "readSync needs zero length parameter").ThrowAsJavaScriptException();
    return env.Null();
  }

  unsigned char buff_read[READ_BUFF_MAXSIZE];
  int returnedLength = hid_read(_hidHandle, buff_read, sizeof buff_read);

  if (returnedLength == -1)
  {
    TypeError::New(env, "could not read data from device").ThrowAsJavaScriptException();
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
    TypeError::New(env, "readTimeout needs time out parameter").ThrowAsJavaScriptException();
    return env.Null();
  }

  const int timeout = info[0].As<Napi::Number>().Uint32Value();
  unsigned char buff_read[READ_BUFF_MAXSIZE];
  int returnedLength = hid_read_timeout(_hidHandle, buff_read, sizeof buff_read, timeout);
  if (returnedLength == -1)
  {
    TypeError::New(env, "could not read data from device").ThrowAsJavaScriptException();
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
    TypeError::New(env, "need report ID and length parameters in getFeatureReport").ThrowAsJavaScriptException();
    return env.Null();
  }

  const uint8_t reportId = info[0].As<Number>().Uint32Value();
  const int bufSize = info[1].As<Number>().Uint32Value();
  if (bufSize == 0)
  {
    TypeError::New(env, "Length parameter cannot be zero in getFeatureReport").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::vector<unsigned char> buf(bufSize);
  buf[0] = reportId;

  int returnedLength = hid_get_feature_report(_hidHandle, buf.data(), bufSize);
  if (returnedLength == -1)
  {
    TypeError::New(env, "could not get feature report from device").ThrowAsJavaScriptException();
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
    TypeError::New(env, "need report (including id in first byte) only in sendFeatureReport").ThrowAsJavaScriptException();
    return env.Null();
  }

  vector<unsigned char> message;
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
        TypeError::New(env, "unexpected array element in array to send, expecting only integers").ThrowAsJavaScriptException();
        return env.Null();
      }
      uint32_t b = v.As<Napi::Number>().Uint32Value();
      message.push_back((unsigned char)b);
    }
  }
  else
  {
    TypeError::New(env, "unexpected data to send, expecting an array or buffer").ThrowAsJavaScriptException();
    return env.Null();
  }

  int returnedLength = hid_send_feature_report(_hidHandle, message.data(), message.size());
  if (returnedLength == -1)
  { // Not sure if there would ever be a valid return value of 0.
    TypeError::New(env, "could not send feature report to device").ThrowAsJavaScriptException();
    return env.Null();
  }

  return Napi::Number::New(env, returnedLength);
}

Napi::Value HID::Close(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  try
  {
    this->close();
    return env.Null();
  }
  catch (const JSException &e)
  {
    e.asV8Exception(env);
    return env.Null();
  }
}

Napi::Value HID::setNonBlocking(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (info.Length() != 1)
  {
    TypeError::New(env, "Expecting a 1 to enable, 0 to disable as the first argument.").ThrowAsJavaScriptException();
    return env.Null();
  }

  int blockStatus = info[0].As<Napi::Number>().Int32Value();
  int res = hid_set_nonblocking(_hidHandle, blockStatus);
  if (res < 0)
  {
    TypeError::New(env, "Error setting non-blocking mode.").ThrowAsJavaScriptException();
    return env.Null();
  }

  return env.Null();
}

Napi::Value HID::write(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (info.Length() != 1)
  {
    TypeError::New(env, "HID write requires one argument").ThrowAsJavaScriptException();
    return env.Null();
  }

  vector<unsigned char> message;
  if (info[0].IsBuffer())
  {
    Napi::Buffer<unsigned char> buffer = info[0].As<Napi::Buffer<unsigned char>>();
    uint32_t len = buffer.Length();
    unsigned char *data = buffer.Data();
    message.assign(data, data + len);
  }
  else
  {
    Napi::Array messageArray = info[0].As<Napi::Array>();
    message.reserve(messageArray.Length());

    for (unsigned i = 0; i < messageArray.Length(); i++)
    {
      Napi::Value v = messageArray.Get(i);
      if (!v.IsNumber())
      {
        TypeError::New(env, "unexpected array element in array to send, expecting only integers").ThrowAsJavaScriptException();
        return env.Null();
      }
      uint32_t b = v.As<Napi::Number>().Uint32Value();
      message.push_back((unsigned char)b);
    }
  }

  if (!_hidHandle)
  {
    TypeError::New(env, "Cannot write to closed device").ThrowAsJavaScriptException();
    return env.Null();
  }

  int returnedLength = hid_write(_hidHandle, message.data(), message.size());
  if (returnedLength < 0)
  {
    TypeError::New(env, "Cannot write to hid device").ThrowAsJavaScriptException();
    return env.Null();
  }

  return Napi::Number::New(env, returnedLength);
}

static string
narrow(wchar_t *wide)
{
  wstring ws(wide);
  ostringstream os;
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

  try
  {
    Napi::Object deviceInfo = Napi::Object::New(env);

    hid_get_manufacturer_string(_hidHandle, wstr, maxlen);
    deviceInfo.Set("manufacturer", Napi::String::New(env, narrow(wstr)));

    hid_get_product_string(_hidHandle, wstr, maxlen);
    deviceInfo.Set("product", Napi::String::New(env, narrow(wstr)));

    hid_get_serial_number_string(_hidHandle, wstr, maxlen);
    deviceInfo.Set("serialNumber", Napi::String::New(env, narrow(wstr)));

    return deviceInfo;
  }
  catch (const JSException &e)
  {
    e.asV8Exception(env);
    return env.Null();
  }
}

Napi::Value HID::Devices(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  int vendorId = 0;
  int productId = 0;

  try
  {
    switch (info.Length())
    {
    case 0:
      break;
    case 2:
      // TODO: are these safe with invalid data?
      vendorId = info[0].As<Napi::Number>().Int32Value();
      productId = info[1].As<Napi::Number>().Int32Value();
      break;
    default:
      throw JSException("unexpected number of arguments to HID.devices() call, expecting either no arguments or vendor and product ID");
    }
  }
  catch (JSException &e)
  {
    e.asV8Exception(env);
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
    // TypeError::New(env, "cannot uninitialize hidapi (hid_exit failed)").ThrowAsJavaScriptException();
    return;
  }
}
void HID::Initialize(Napi::Env &env, Napi::Object &exports)
{
  if (hid_init())
  {
    TypeError::New(env, "cannot initialize hidapi (hid_init failed)").ThrowAsJavaScriptException();
    return;
  }

  napi_add_env_cleanup_hook(env, deinitialize, nullptr);

  Napi::Function ctor = DefineClass(env, "HID", {
                                                    InstanceMethod("close", &HID::Close),
                                                    InstanceMethod("write", &HID::write),
                                                    InstanceMethod("getFeatureReport", &HID::getFeatureReport),
                                                    InstanceMethod("sendFeatureReport", &HID::sendFeatureReport),
                                                    InstanceMethod("setNonBlocking", &HID::setNonBlocking),
                                                    /*
  Nan::SetPrototypeMethod(hidTemplate, "read", read);
  Nan::SetPrototypeMethod(hidTemplate, "write", write);
  Nan::SetPrototypeMethod(hidTemplate, "getFeatureReport", getFeatureReport);
  Nan::SetPrototypeMethod(hidTemplate, "sendFeatureReport", sendFeatureReport);
  Nan::SetPrototypeMethod(hidTemplate, "setNonBlocking", setNonBlocking);
  Nan::SetPrototypeMethod(hidTemplate, "readSync", readSync);
  Nan::SetPrototypeMethod(hidTemplate, "readTimeout", readTimeout);
*/
                                                    InstanceMethod("readSync", &HID::readSync),
                                                    InstanceMethod("readTimeout", &HID::readTimeout),
                                                    InstanceMethod("getDeviceInfo", &HID::getDeviceInfo),
                                                });

  exports.Set("HID", ctor);
  exports.Set("devices", Napi::Function::New(env, &HID::Devices));
}

Napi::Object Init(Napi::Env env, Napi::Object exports)
{
  HID::Initialize(env, exports);

  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)

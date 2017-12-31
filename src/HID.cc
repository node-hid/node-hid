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

#include <node.h>
#include <nan.h>

#include <hidapi.h>

using namespace std;
using namespace v8;
using namespace node;

#define READ_BUFF_MAXSIZE 2048

// //////////////////////////////////////////////////////////////////
// Throwable error class that can be converted to a JavaScript
// exception
// //////////////////////////////////////////////////////////////////
class JSException
{
public:
  JSException(const string& text) : _message(text) {}
  virtual ~JSException() {}
  virtual const string message() const { return _message; }
  virtual void asV8Exception() const { Nan::ThrowError(message().c_str()); }

protected:
  string _message;
};

class HID
  : public Nan::ObjectWrap
{
public:
  static void Initialize(Local<Object> target);
  static NAN_METHOD(devices);

  typedef vector<unsigned char> databuf_t;

  int write(const databuf_t& message)
    throw(JSException);
  void close();
  void setNonBlocking(int message)
    throw(JSException);

private:
  HID(unsigned short vendorId, unsigned short productId, wchar_t* serialNumber = 0);
  HID(const char* path);
  ~HID() { close(); }

  static NAN_METHOD(New);
  static NAN_METHOD(read);
  static NAN_METHOD(write);
  static NAN_METHOD(close);
  static NAN_METHOD(setNonBlocking);
  static NAN_METHOD(getFeatureReport);
  static NAN_METHOD(readSync);
  static NAN_METHOD(readTimeout);

  static NAN_METHOD(sendFeatureReport);
  static NAN_METHOD(getDeviceInfo);


  static void recvAsync(uv_work_t* req);
  static void recvAsyncDone(uv_work_t* req);


  struct ReceiveIOCB {
    ReceiveIOCB(HID* hid, Nan::Callback *callback)
      : _hid(hid),
        _callback(callback),
        _error(0)
    {}

    ~ReceiveIOCB()
    {
      if (_error) {
        delete _error;
      }
    }

    HID* _hid;
    Nan::Callback *_callback;
    JSException* _error;
    vector<unsigned char> _data;
  };

  void readResultsToJSCallbackArguments(ReceiveIOCB* iocb, Local<Value> argv[]);

  hid_device* _hidHandle;
};

HID::HID(unsigned short vendorId, unsigned short productId, wchar_t* serialNumber)
{
  _hidHandle = hid_open(vendorId, productId, serialNumber);

  if (!_hidHandle) {
    ostringstream os;
    os << "cannot open device with vendor id 0x" << hex << vendorId << " and product id 0x" << productId;
    throw JSException(os.str());
  }
}

HID::HID(const char* path)
{
  _hidHandle = hid_open_path(path);

  if (!_hidHandle) {
    ostringstream os;
    os << "cannot open device with path " << path;
    throw JSException(os.str());
  }
}

void
HID::close()
{
  if (_hidHandle) {
    hid_close(_hidHandle);
    _hidHandle = 0;
  }
}

void
HID::setNonBlocking(int message)
  throw(JSException)
{
  int res;
  res = hid_set_nonblocking(_hidHandle, message);
  if (res < 0) {
    throw JSException("Error setting non-blocking mode.");
  }
}

int
HID::write(const databuf_t& message)
  throw(JSException)
{
  //unsigned char buf[message.size()];
  unsigned char* buf = new unsigned char[message.size()];
  unsigned char* p = buf;
  int res;
  for (vector<unsigned char>::const_iterator i = message.begin(); i != message.end(); i++) {
    *p++ = *i;
  }
  res = hid_write(_hidHandle, buf, message.size());
  delete[] buf;
  if (res < 0) {
    throw JSException("Cannot write to HID device");
  }
  return res;  // return actual number of bytes written
}

void
HID::recvAsync(uv_work_t* req)
{
  ReceiveIOCB* iocb = static_cast<ReceiveIOCB*>(req->data);
  HID* hid = iocb->_hid;

  unsigned char buf[READ_BUFF_MAXSIZE];
  int len = hid_read(hid->_hidHandle, buf, sizeof buf);
  if (len < 0) {
    iocb->_error = new JSException("could not read from HID device");
  } else {
    iocb->_data = vector<unsigned char>(buf, buf + len);
  }
}

void
HID::readResultsToJSCallbackArguments(ReceiveIOCB* iocb, Local<Value> argv[])
{
  if (iocb->_error) {
    argv[0] = Exception::Error(Nan::New<String>(iocb->_error->message().c_str()).ToLocalChecked());
  } else {
    const vector<unsigned char>& message = iocb->_data;
    //Get "fast" node Buffer constructor
    Local<Function> nodeBufConstructor = Local<Function>::Cast(
      Nan::GetCurrentContext()->Global()->Get(Nan::New<String>("Buffer").ToLocalChecked() )
    );
    //Construct a new Buffer
    Local<Value> nodeBufferArgs[1] = { Nan::New<Integer>((uint32_t)message.size()) };
    Local<Object> buf = Nan::NewInstance(nodeBufConstructor, 1, nodeBufferArgs).ToLocalChecked();

    char* data = Buffer::Data(buf);
    int j = 0;
    for (vector<unsigned char>::const_iterator k = message.begin(); k != message.end(); k++) {
      data[j++] = *k;
    }
    argv[1] = buf;
  }
}

void
HID::recvAsyncDone(uv_work_t* req)
{
  Nan::HandleScope scope;
  ReceiveIOCB* iocb = static_cast<ReceiveIOCB*>(req->data);

  Local<Value> argv[2];
  argv[0] = Nan::Undefined();
  argv[1] = Nan::Undefined();

  iocb->_hid->readResultsToJSCallbackArguments(iocb, argv);
  iocb->_hid->Unref();

  Nan::TryCatch tryCatch;
  iocb->_callback->Call(2, argv);

  if (tryCatch.HasCaught()) {
      Nan::FatalException(tryCatch);
  }

  delete req;
  delete iocb->_callback;

  delete iocb;
}

NAN_METHOD(HID::read)
{
  Nan::HandleScope scope;

  if (info.Length() != 1
      || !info[0]->IsFunction()) {
    return Nan::ThrowError("need one callback function argument in read");
  }

  HID* hid = Nan::ObjectWrap::Unwrap<HID>(info.This());
  hid->Ref();

  uv_work_t* req = new uv_work_t;
  req->data = new ReceiveIOCB(hid, new Nan::Callback(Local<Function>::Cast(info[0])));;
  uv_queue_work(uv_default_loop(), req, recvAsync, (uv_after_work_cb)recvAsyncDone);

  return;
}

NAN_METHOD(HID::readSync)
{
  Nan::HandleScope scope;

  if (info.Length() != 0) {
    return Nan::ThrowError("readSync need zero length parameter");
  }

  HID* hid = Nan::ObjectWrap::Unwrap<HID>(info.This());
  unsigned char buff_read[READ_BUFF_MAXSIZE];
  int returnedLength = hid_read(hid->_hidHandle, buff_read, sizeof buff_read);

  if (returnedLength == -1) {
    return Nan::ThrowError("could not read data from device");
  }
  Local<Array> retval = Nan::New<Array>();

  for (int i = 0; i < returnedLength; i++) {
    retval->Set(i, Nan::New<Integer>(buff_read[i]));
  }
  info.GetReturnValue().Set(retval);
}

NAN_METHOD(HID::readTimeout)
{
  Nan::HandleScope scope;

  if (info.Length() != 1 || !info[0]->IsUint32()) {
    return Nan::ThrowError("readTimeout needs time out parameter");
  }

  HID* hid = Nan::ObjectWrap::Unwrap<HID>(info.This());
  // const int timeout = info[0]->ToUint32()->Value();
  const int timeout = Nan::To<uint32_t>(info[0]).FromJust();
  unsigned char buff_read[READ_BUFF_MAXSIZE];
  int returnedLength = hid_read_timeout(hid->_hidHandle, buff_read, sizeof buff_read, timeout);

  if (returnedLength == -1) {
    return Nan::ThrowError("could not read data from device");
  }
  Local<Array> retval = Nan::New<Array>();

  for (int i = 0; i < returnedLength; i++) {
    retval->Set(i, Nan::New<Integer>(buff_read[i]));
  }
  info.GetReturnValue().Set(retval);
}

NAN_METHOD(HID::getFeatureReport)
{
  Nan::HandleScope scope;

  if (info.Length() != 2 || !info[1]->IsUint32() ) {
    return Nan::ThrowError("need report ID and length parameters in getFeatureReport");
  }

  const uint8_t reportId = Nan::To<uint32_t>(info[0]).FromJust();
  const int bufSize = Nan::To<uint32_t>(info[1]).FromJust();
  if( bufSize == 0 ) {
    return Nan::ThrowError("Length parameter cannot be zero in getFeatureReport");
  }

  HID* hid = Nan::ObjectWrap::Unwrap<HID>(info.This());

  //unsigned char buf[bufSize];
  unsigned char* buf = new unsigned char[bufSize];
  buf[0] = reportId;

  int returnedLength = hid_get_feature_report(hid->_hidHandle, buf, bufSize);

  if (returnedLength == -1) {
    delete[] buf;
    return Nan::ThrowError("could not get feature report from device");
  }
  Local<Array> retval = Nan::New<Array>();

  for (int i = 0; i < returnedLength; i++) {
    retval->Set(i, Nan::New<Integer>(buf[i]));
  }
  delete[] buf;
  info.GetReturnValue().Set(retval);
}


NAN_METHOD(HID::sendFeatureReport)
{
  Nan::HandleScope scope;

  if (info.Length() != 1){
    return Nan::ThrowError("need report (including id in first byte) only in sendFeatureReport");
  }


  HID* hid = Nan::ObjectWrap::Unwrap<HID>(info.This());

  vector<unsigned char> message;
  Local<Array> messageArray = Local<Array>::Cast(info[0]);
  for (unsigned i = 0; i < messageArray->Length(); i++) {
    if (!messageArray->Get(i)->IsNumber()) {
      throw JSException("unexpected array element in array to send, expecting only integers");
    }
    message.push_back((unsigned char) messageArray->Get(i)->Int32Value());
  }

  // Convert vector to char array
  //unsigned char buf[message.size()];
  unsigned char* buf = new unsigned char[message.size()];
  unsigned char* p = buf;
  for (vector<unsigned char>::const_iterator i = message.begin(); i != message.end(); i++) {
    *p++ = *i;
  }

  int returnedLength = hid_send_feature_report(hid->_hidHandle, buf, message.size());
  delete[] buf;
  if (returnedLength == -1) { // Not sure if there would ever be a valid return value of 0.
    return Nan::ThrowError("could not send feature report to device");
  }

  info.GetReturnValue().Set(Nan::New<Integer>(returnedLength));
}




NAN_METHOD(HID::New)
{
  Nan::HandleScope scope;

  if (!info.IsConstructCall()) {
    return Nan::ThrowError("HID function can only be used as a constructor");
  }

  if (info.Length() < 1) {
    return Nan::ThrowError("HID constructor requires at least one argument");
  }

  try {
    HID* hid;
    if (info.Length() == 1) {
      // open by path
      hid = new HID(*Nan::Utf8String(info[0]));
    } else {
      int32_t vendorId = info[0]->Int32Value();
      int32_t productId = info[1]->Int32Value();
      Local<Value> serial;
      wchar_t* serialPointer = 0;
      if (info.Length() > 2) {
        serialPointer = (wchar_t*) *v8::String::Value(info[2]);
      }
      hid = new HID(vendorId, productId, serialPointer);
    }
    hid->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }
  catch (const JSException& e) {
    e.asV8Exception();
  }
}

NAN_METHOD(HID::close)
{
  Nan::HandleScope scope;

  try {
    HID* hid = Nan::ObjectWrap::Unwrap<HID>(info.This());
    hid->close();
    return;
  }
  catch (const JSException& e) {
    e.asV8Exception();
  }
}

NAN_METHOD(HID::setNonBlocking)
{
  Nan::HandleScope scope;

  if (info.Length() != 1) {
    return Nan::ThrowError("Expecting a 1 to enable, 0 to disable as the first argument.");
  }
  int blockStatus = 0;
  blockStatus = info[0]->Int32Value();
  try {
    HID* hid = Nan::ObjectWrap::Unwrap<HID>(info.This());
    hid->setNonBlocking(blockStatus);
    return;
  }
  catch (const JSException& e) {
    e.asV8Exception();
  }
}

NAN_METHOD(HID::write)
{
  Nan::HandleScope scope;

  if (info.Length() != 1) {
    return Nan::ThrowError("HID write requires one argument");
  }

  try {
    HID* hid = Nan::ObjectWrap::Unwrap<HID>(info.This());

    vector<unsigned char> message;
    Local<Array> messageArray = Local<Array>::Cast(info[0]);
    for (unsigned i = 0; i < messageArray->Length(); i++) {
      if (!messageArray->Get(i)->IsNumber()) {
        throw JSException("unexpected array element in array to send, expecting only integers");
      }
      message.push_back((unsigned char) messageArray->Get(i)->Int32Value());
    }
    int returnedLength = hid->write(message); // returns number of bytes written

    info.GetReturnValue().Set(Nan::New<Integer>(returnedLength));
  }
  catch (const JSException& e) {
    e.asV8Exception();
  }
}

static string
narrow(wchar_t* wide)
{
  wstring ws(wide);
  ostringstream os;
  for (size_t i = 0; i < ws.size(); i++) {
    os << os.narrow(ws[i], '?');
  }
  return os.str();
}

NAN_METHOD(HID::getDeviceInfo)
{
  Nan::HandleScope scope;
  Local<Object> deviceInfo = Nan::New<Object>();
  const int maxlen = 256;
  wchar_t wstr[maxlen]; // FIXME: use new & delete

  try {
    HID* hid = Nan::ObjectWrap::Unwrap<HID>(info.This());

    hid_get_manufacturer_string(hid->_hidHandle, wstr, maxlen);
    deviceInfo->Set(Nan::New<String>("manufacturer").ToLocalChecked(),
      Nan::New<String>(narrow(wstr).c_str()).ToLocalChecked());

    hid_get_product_string(hid->_hidHandle, wstr, maxlen);
    deviceInfo->Set(Nan::New<String>("product").ToLocalChecked(),
      Nan::New<String>(narrow(wstr).c_str()).ToLocalChecked());

    hid_get_serial_number_string(hid->_hidHandle, wstr, maxlen);
    deviceInfo->Set(Nan::New<String>("serialNumber").ToLocalChecked(),
      Nan::New<String>(narrow(wstr).c_str()).ToLocalChecked());

  }
  catch (const JSException& e) {
    e.asV8Exception();
  }

  info.GetReturnValue().Set(deviceInfo);
}

NAN_METHOD(HID::devices)
{
  Nan::HandleScope scope;

  int vendorId = 0;
  int productId = 0;

  try {
    switch (info.Length()) {
    case 0:
      break;
    case 2:
      vendorId = info[0]->Int32Value();
      productId = info[1]->Int32Value();
      break;
    default:
      throw JSException("unexpected number of arguments to HID.devices() call, expecting either no arguments or vendor and product ID");
    }
  }
  catch (JSException& e) {
    e.asV8Exception();
  }

  hid_device_info* devs = hid_enumerate(vendorId, productId);
  Local<Array> retval = Nan::New<Array>();
  int count = 0;
  for (hid_device_info* dev = devs; dev; dev = dev->next) {
    Local<Object> deviceInfo = Nan::New<Object>();
    deviceInfo->Set(Nan::New<String>("vendorId").ToLocalChecked(), Nan::New<Integer>(dev->vendor_id));
    deviceInfo->Set(Nan::New<String>("productId").ToLocalChecked(), Nan::New<Integer>(dev->product_id));
    if (dev->path) {
      deviceInfo->Set(Nan::New<String>("path").ToLocalChecked(), Nan::New<String>(dev->path).ToLocalChecked());
    }
    if (dev->serial_number) {
      deviceInfo->Set(Nan::New<String>("serialNumber").ToLocalChecked(), Nan::New<String>(narrow(dev->serial_number).c_str()).ToLocalChecked());
    }
    if (dev->manufacturer_string) {
      deviceInfo->Set(Nan::New<String>("manufacturer").ToLocalChecked(), Nan::New<String>(narrow(dev->manufacturer_string).c_str()).ToLocalChecked());
    }
    if (dev->product_string) {
      deviceInfo->Set(Nan::New<String>("product").ToLocalChecked(), Nan::New<String>(narrow(dev->product_string).c_str()).ToLocalChecked());
    }
    deviceInfo->Set(Nan::New<String>("release").ToLocalChecked(), Nan::New<Integer>(dev->release_number));
    deviceInfo->Set(Nan::New<String>("interface").ToLocalChecked(), Nan::New<Integer>(dev->interface_number));
    if( dev->usage_page ) {
        deviceInfo->Set(Nan::New<String>("usagePage").ToLocalChecked(), Nan::New<Integer>(dev->usage_page));
    }
    if( dev->usage ) {
        deviceInfo->Set(Nan::New<String>("usage").ToLocalChecked(), Nan::New<Integer>(dev->usage));
    }
    retval->Set(count++, deviceInfo);
  }
  hid_free_enumeration(devs);
  info.GetReturnValue().Set(retval);
}

static void
deinitialize(void*)
{
  if (hid_exit()) {
    return Nan::ThrowError("cannot uninitialize hidapi (hid_exit failed)");
    // abort();
  }
}

void
HID::Initialize(Local<Object> target)
{
  if (hid_init()) {
    return Nan::ThrowError("cannot initialize hidapi (hid_init failed)");
    //abort();
  }

  node::AtExit(deinitialize, 0);

  Nan::HandleScope scope;

  Local<FunctionTemplate> hidTemplate = Nan::New<FunctionTemplate>(HID::New);
  hidTemplate->InstanceTemplate()->SetInternalFieldCount(1);
  hidTemplate->SetClassName(Nan::New<String>("HID").ToLocalChecked());

  Nan::SetPrototypeMethod(hidTemplate, "close", close);
  Nan::SetPrototypeMethod(hidTemplate, "read", read);
  Nan::SetPrototypeMethod(hidTemplate, "write", write);
  Nan::SetPrototypeMethod(hidTemplate, "getFeatureReport", getFeatureReport);
  Nan::SetPrototypeMethod(hidTemplate, "sendFeatureReport", sendFeatureReport);
  Nan::SetPrototypeMethod(hidTemplate, "setNonBlocking", setNonBlocking);
  Nan::SetPrototypeMethod(hidTemplate, "readSync", readSync);
  Nan::SetPrototypeMethod(hidTemplate, "readTimeout", readTimeout);
  Nan::SetPrototypeMethod(hidTemplate, "getDeviceInfo", getDeviceInfo);

  target->Set(Nan::New<String>("HID").ToLocalChecked(), hidTemplate->GetFunction());

  target->Set(Nan::New<String>("devices").ToLocalChecked(), Nan::New<FunctionTemplate>(HID::devices)->GetFunction());
}


extern "C" {

  static void init (Local<Object> target)
  {
    Nan::HandleScope scope;

    HID::Initialize(target);
  }

  NODE_MODULE(HID, init);
}

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

#include <v8.h>
#include <node.h>
#include <node_buffer.h>

#include <hidapi.h>

using namespace std;
using namespace v8;
using namespace node;

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
  virtual Handle<Value> asV8Exception() const { return ThrowException(String::New(message().c_str())); }

protected:
  string _message;
};

class HID
  : public ObjectWrap
{
public:
  static void Initialize(Handle<Object> target);
  static Handle<Value> devices(const Arguments& args);

  typedef vector<unsigned char> databuf_t;

  void write(const databuf_t& message)
    throw(JSException);
  void close();
  void setNonBlocking(int message)
    throw(JSException);

private:
  HID(unsigned short vendorId, unsigned short productId, wchar_t* serialNumber = 0);
  HID(const char* path);
  ~HID() { close(); }

  static Handle<Value> New(const Arguments& args);
  static Handle<Value> read(const Arguments& args);
  static Handle<Value> write(const Arguments& args);
  static Handle<Value> close(const Arguments& args);
  static Handle<Value> setNonBlocking(const Arguments& args);
  static Handle<Value> getFeatureReport(const Arguments& args);

  static Handle<Value> sendFeatureReport(const Arguments& args);


  static void recvAsync(uv_work_t* req);
  static void recvAsyncDone(uv_work_t* req);

  struct ReceiveIOCB {
    ReceiveIOCB(HID* hid, Persistent<Object> this_, Persistent<Function> callback)
      : _hid(hid),
        _this(this_),
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
    Persistent<Object> _this;
    Persistent<Function> _callback;
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

void
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
}

void
HID::recvAsync(uv_work_t* req)
{
  ReceiveIOCB* iocb = static_cast<ReceiveIOCB*>(req->data);
  HID* hid = iocb->_hid;

  unsigned char buf[1024];
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
    argv[0] = Exception::Error(String::New(iocb->_error->message().c_str()));
  } else {
    const vector<unsigned char>& message = iocb->_data;
    //Get "fast" node Buffer constructor
    Local<Function> nodeBufConstructor = Local<Function>::Cast(
      Context::GetCurrent()->Global()->Get(String::New("Buffer") )
    );
    //Construct a new Buffer
    Handle<Value> nodeBufferArgs[1] = { Integer::New(message.size()) };
    Local<Object> buf = nodeBufConstructor->NewInstance(1, nodeBufferArgs);
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
  HandleScope scope;
  ReceiveIOCB* iocb = static_cast<ReceiveIOCB*>(req->data);

  Local<Value> argv[2];
  argv[0] = *Undefined();
  argv[1] = *Undefined();

  iocb->_hid->readResultsToJSCallbackArguments(iocb, argv);
  iocb->_hid->Unref();

  TryCatch tryCatch;
  iocb->_callback->Call(iocb->_this, 2, argv);

  if (tryCatch.HasCaught()) {
    FatalException(tryCatch);
  }

  iocb->_this.Dispose();
  iocb->_callback.Dispose();

  delete iocb;
}

Handle<Value>
HID::read(const Arguments& args)
{
  HandleScope scope;

  if (args.Length() != 1
      || !args[0]->IsFunction()) {
    return ThrowException(String::New("need one callback function argument in read"));
  }

  HID* hid = ObjectWrap::Unwrap<HID>(args.This());
  hid->Ref();

  uv_work_t* req = new uv_work_t;
  req->data = new ReceiveIOCB(hid,
                             Persistent<Object>::New(Local<Object>::Cast(args.This())),
                             Persistent<Function>::New(Local<Function>::Cast(args[0])));
  uv_queue_work(uv_default_loop(), req, recvAsync, (uv_after_work_cb)recvAsyncDone);

  return Undefined();
}

Handle<Value>
HID::getFeatureReport(const Arguments& args)
{
  HandleScope scope;

  if (args.Length() != 2
      || args[1]->ToUint32()->Value() == 0) {
    return ThrowException(String::New("need report ID and non-zero length parameter in getFeatureReport"));
  }

  const uint8_t reportId = args[0]->ToUint32()->Value();
  HID* hid = ObjectWrap::Unwrap<HID>(args.This());
  const int bufSize = args[1]->ToUint32()->Value();
  //unsigned char buf[bufSize];
  unsigned char* buf = new unsigned char[bufSize];
  buf[0] = reportId;

  int returnedLength = hid_get_feature_report(hid->_hidHandle, buf, bufSize);

  if (returnedLength == -1) {
    delete[] buf;
    return ThrowException(String::New("could not get feature report from device"));
  }
  Local<Array> retval = Array::New();

  for (int i = 0; i < returnedLength; i++) {
    retval->Set(i, Integer::New(buf[i]));
  }
  delete[] buf;
  return retval;
}


Handle<Value>
HID::sendFeatureReport(const Arguments& args)
{
  HandleScope scope;

  if (args.Length() != 1){
    return ThrowException(String::New("need report (including id in first byte) only in sendFeatureReport"));
  }


  HID* hid = ObjectWrap::Unwrap<HID>(args.This());

  vector<unsigned char> message;
  Local<Array> messageArray = Local<Array>::Cast(args[0]);
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
    return ThrowException(String::New("could not send feature report to device"));
  }

  return Integer::New(returnedLength);
}




Handle<Value>
HID::New(const Arguments& args)
{
  if (!args.IsConstructCall()) {
    return ThrowException(String::New("HID function can only be used as a constructor"));
  }

  if (args.Length() < 1) {
    return ThrowException(String::New("HID constructor requires at least one argument"));
  }

  HandleScope scope;

  try {
    HID* hid;
    if (args.Length() == 1) {
      // open by path
      hid = new HID(*String::Utf8Value(args[0]));
    } else {
      int32_t vendorId = args[0]->Int32Value();
      int32_t productId = args[1]->Int32Value();
      Handle<Value> serial;
      wchar_t* serialPointer = 0;
      if (args.Length() > 2) {
        serialPointer = (wchar_t*) *String::Value(args[2]);
      }
      hid = new HID(vendorId, productId, serialPointer);
    }
    hid->Wrap(args.This());
    return args.This();
  }
  catch (const JSException& e) {
    return e.asV8Exception();
  }
}

Handle<Value>
HID::close(const Arguments& args)
{
  try {
    HID* hid = ObjectWrap::Unwrap<HID>(args.This());
    hid->close();
    return Undefined();
  }
  catch (const JSException& e) {
    return e.asV8Exception();
  }
}

Handle<Value>
HID::setNonBlocking(const Arguments& args)
{
  if (args.Length() != 1) {
    return ThrowException(String::New("Expecting a 1 to enable, 0 to disable as the first argument."));
  }
  int blockStatus = 0;
  blockStatus = args[0]->Int32Value();
  try {
    HID* hid = ObjectWrap::Unwrap<HID>(args.This());
    hid->setNonBlocking(blockStatus);
    return Undefined();
  }
  catch (const JSException& e) {
    return e.asV8Exception();
  }
}

Handle<Value>
HID::write(const Arguments& args)
{
  if (args.Length() != 1) {
    return ThrowException(String::New("HID write requires one argument"));
  }

  try {
    HID* hid = ObjectWrap::Unwrap<HID>(args.This());

    vector<unsigned char> message;
    Local<Array> messageArray = Local<Array>::Cast(args[0]);
    for (unsigned i = 0; i < messageArray->Length(); i++) {
      if (!messageArray->Get(i)->IsNumber()) {
        throw JSException("unexpected array element in array to send, expecting only integers");
      }
      message.push_back((unsigned char) messageArray->Get(i)->Int32Value());
    }
    hid->write(message);

    return Undefined();
  }
  catch (const JSException& e) {
    return e.asV8Exception();
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

Handle<Value>
HID::devices(const Arguments& args)
{
  int vendorId = 0;
  int productId = 0;

  try {
    switch (args.Length()) {
    case 0:
      break;
    case 2:
      vendorId = args[0]->Int32Value();
      productId = args[1]->Int32Value();
      break;
    default:
      throw JSException("unexpected number of arguments to HID.devices() call, expecting either no arguments or vendor and product ID");
    }
  }
  catch (JSException& e) {
    return e.asV8Exception();
  }
  
  hid_device_info* devs = hid_enumerate(vendorId, productId);
  Local<Array> retval = Array::New();
  int count = 0;
  for (hid_device_info* dev = devs; dev; dev = dev->next) {
    Local<Object> deviceInfo = Object::New();
    deviceInfo->Set(String::NewSymbol("vendorId"), Integer::New(dev->vendor_id));
    deviceInfo->Set(String::NewSymbol("productId"), Integer::New(dev->product_id));
    if (dev->path) {
      deviceInfo->Set(String::NewSymbol("path"), String::New(dev->path));
    }
    if (dev->serial_number) {
      deviceInfo->Set(String::NewSymbol("serialNumber"), String::New(narrow(dev->serial_number).c_str()));
    }
    if (dev->manufacturer_string) {
      deviceInfo->Set(String::NewSymbol("manufacturer"), String::New(narrow(dev->manufacturer_string).c_str()));
    }
    if (dev->product_string) {
      deviceInfo->Set(String::NewSymbol("product"), String::New(narrow(dev->product_string).c_str()));
    }
    deviceInfo->Set(String::NewSymbol("release"), Integer::New(dev->release_number));
    deviceInfo->Set(String::NewSymbol("interface"), Integer::New(dev->interface_number));
    deviceInfo->Set(String::NewSymbol("usagePage"), Integer::New(dev->usage_page));
    deviceInfo->Set(String::NewSymbol("usage"), Integer::New(dev->usage));
    retval->Set(count++, deviceInfo);
  }
  hid_free_enumeration(devs);
  return retval;
}

static void
deinitialize(void*)
{
  if (hid_exit()) {
    cerr << "cannot initialize hidapi (hid_init failed)" << endl;
    abort();
  }
}

void
HID::Initialize(Handle<Object> target)
{
  if (hid_init()) {
    cerr << "cannot initialize hidapi (hid_init failed)" << endl;
    abort();
  }

  node::AtExit(deinitialize, 0);

  HandleScope scope;

  Local<FunctionTemplate> hidTemplate = FunctionTemplate::New(HID::New);
  hidTemplate->InstanceTemplate()->SetInternalFieldCount(1);
  hidTemplate->SetClassName(String::New("HID"));

  NODE_SET_PROTOTYPE_METHOD(hidTemplate, "close", close);
  NODE_SET_PROTOTYPE_METHOD(hidTemplate, "read", read);
  NODE_SET_PROTOTYPE_METHOD(hidTemplate, "write", write);
  NODE_SET_PROTOTYPE_METHOD(hidTemplate, "getFeatureReport", getFeatureReport);
  NODE_SET_PROTOTYPE_METHOD(hidTemplate, "sendFeatureReport", sendFeatureReport);
  NODE_SET_PROTOTYPE_METHOD(hidTemplate, "setNonBlocking", setNonBlocking);

  target->Set(String::NewSymbol("HID"), hidTemplate->GetFunction());

  target->Set(String::NewSymbol("devices"), FunctionTemplate::New(HID::devices)->GetFunction());
}


extern "C" {
  
  static void init (Handle<Object> target)
  {
    HandleScope handleScope;
    
    HID::Initialize(target);
  }

  NODE_MODULE(HID, init);
}


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
  virtual _NAN_METHOD_RETURN_TYPE asV8Exception() const { return NanThrowError(NanNew<String>(message().c_str())); }

protected:
  string _message;
};

class HID
  : public ObjectWrap
{
public:
  static void Initialize(Handle<Object> target);
  static NAN_METHOD(devices);

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

  static NAN_METHOD(New);
  static NAN_METHOD(read);
  static NAN_METHOD(write);
  static NAN_METHOD(close);
  static NAN_METHOD(setNonBlocking);
  static NAN_METHOD(getFeatureReport);

  static NAN_METHOD(sendFeatureReport);

  class ReceiveWorker :
    public NanAsyncWorker
  {
  public:
	ReceiveWorker(HID* hid, NanCallback *callback)
      : NanAsyncWorker(callback),
	    _hidHandle(hid->_hidHandle),
        _error(0)
    {}

    ~ReceiveWorker()
    {
      if (_error) {
        delete _error;
      }
    }
	
	void
	Execute()
	{
		unsigned char buf[1024];
		int len = hid_read(_hidHandle, buf, sizeof buf);
		if (len < 0) {
			_error = new JSException("could not read from HID device");
		}
		else {
			_data = vector<unsigned char>(buf, buf + len);
		}
	}

	void
	HandleOKCallback()
	{
	  NanScope();

	  //Get "fast" node Buffer constructor
	  Local<Function> nodeBufConstructor = Local<Function>::Cast(
		NanGetCurrentContext()->Global()->Get(NanNew<String>("Buffer"))
	  );

	  //Construct a new Buffer
	  Handle<Value> nodeBufferArgs[1] = { NanNew<Integer>(_data.size()) };
	  Local<Object> buf = nodeBufConstructor->NewInstance(1, nodeBufferArgs);
	  char* data = Buffer::Data(buf);
	  int j = 0;
	  for (vector<unsigned char>::const_iterator k = _data.begin(); k != _data.end(); k++) {
		  data[j++] = *k;
	  }

	  Local<Value> argv[] = {
		  NanNull(),
		  buf
	  };

	  callback->Call(2, argv);
	}

  private:
    hid_device* _hidHandle;
    JSException* _error;
    vector<unsigned char> _data;
  };

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

NAN_METHOD(HID::read)
{
  NanScope();
  Persistent<Object> self;

  NanAssignPersistent(self, Local<Object>::Cast(args.This()));

  if (args.Length() != 1
      || !args[0]->IsFunction()) {
    return NanThrowError(NanNew<String>("need one callback function argument in read"));
  }

  HID* hid = ObjectWrap::Unwrap<HID>(args.This());

  NanAsyncQueueWorker(new ReceiveWorker(hid, new NanCallback(args[0].As<Function>())));

  NanReturnUndefined();
}

NAN_METHOD(HID::getFeatureReport)
{
  NanScope();

  if (args.Length() != 2
      || args[1]->ToUint32()->Value() == 0) {
    return NanThrowError(NanNew<String>("need report ID and non-zero length parameter in getFeatureReport"));
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
    return NanThrowError(NanNew<String>("could not get feature report from device"));
  }
  Local<Array> retval = NanNew<Array>();

  for (int i = 0; i < returnedLength; i++) {
    retval->Set(i, NanNew<Integer>(buf[i]));
  }
  delete[] buf;
  NanReturnValue(retval);
}


NAN_METHOD(HID::sendFeatureReport)
{
  NanScope();

  if (args.Length() != 1){
    return NanThrowError(NanNew<String>("need report (including id in first byte) only in sendFeatureReport"));
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
    return NanThrowError(NanNew<String>("could not send feature report to device"));
  }

  NanReturnValue(NanNew<Integer>(returnedLength));
}

NAN_METHOD(HID::New)
{
  if (!args.IsConstructCall()) {
    return NanThrowError(NanNew<String>("HID function can only be used as a constructor"));
  }

  if (args.Length() < 1) {
    return NanThrowError(NanNew<String>("HID constructor requires at least one argument"));
  }

  NanScope();

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
	NanReturnValue(args.This());
  }
  catch (const JSException& e) {
    return e.asV8Exception();
  }
}

NAN_METHOD(HID::close)
{
  try {
    HID* hid = ObjectWrap::Unwrap<HID>(args.This());
    hid->close();
    NanReturnUndefined();
  }
  catch (const JSException& e) {
    return e.asV8Exception();
  }
}

NAN_METHOD(HID::setNonBlocking)
{
  if (args.Length() != 1) {
    return NanThrowError(NanNew<String>("Expecting a 1 to enable, 0 to disable as the first argument."));
  }
  int blockStatus = 0;
  blockStatus = args[0]->Int32Value();
  try {
    HID* hid = ObjectWrap::Unwrap<HID>(args.This());
    hid->setNonBlocking(blockStatus);
    NanReturnUndefined();
  }
  catch (const JSException& e) {
    return e.asV8Exception();
  }
}

NAN_METHOD(HID::write)
{
  if (args.Length() != 1) {
    return NanThrowError(NanNew<String>("HID write requires one argument"));
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

    NanReturnUndefined();
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

NAN_METHOD(HID::devices)
{
  NanScope();
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
  Local<Array> retval = NanNew<Array>();
  int count = 0;
  for (hid_device_info* dev = devs; dev; dev = dev->next) {
    Local<Object> deviceInfo = NanNew<Object>();
    deviceInfo->Set(NanNew<String>("vendorId"), NanNew<Integer>(dev->vendor_id));
    deviceInfo->Set(NanNew<String>("productId"), NanNew<Integer>(dev->product_id));
    if (dev->path) {
      deviceInfo->Set(NanNew<String>("path"), NanNew<String>(dev->path));
    }
    if (dev->serial_number) {
      deviceInfo->Set(NanNew<String>("serialNumber"), NanNew<String>(narrow(dev->serial_number).c_str()));
    }
    if (dev->manufacturer_string) {
      deviceInfo->Set(NanNew<String>("manufacturer"), NanNew<String>(narrow(dev->manufacturer_string).c_str()));
    }
    if (dev->product_string) {
      deviceInfo->Set(NanNew<String>("product"), NanNew<String>(narrow(dev->product_string).c_str()));
    }
    deviceInfo->Set(NanNew<String>("release"), NanNew<Integer>(dev->release_number));
    deviceInfo->Set(NanNew<String>("interface"), NanNew<Integer>(dev->interface_number));
    deviceInfo->Set(NanNew<String>("usagePage"), NanNew<Integer>(dev->usage_page));
    deviceInfo->Set(NanNew<String>("usage"), NanNew<Integer>(dev->usage));
    retval->Set(count++, deviceInfo);
  }
  hid_free_enumeration(devs);
  NanReturnValue(retval);
}

static void
deinitialize(void*)
{
  if (hid_exit()) {
    cerr << "cannot deinitialize hidapi (hid_exit failed)" << endl;
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

  NanScope();

  Local<FunctionTemplate> hidTemplate = NanNew<FunctionTemplate>(HID::New);
  hidTemplate->InstanceTemplate()->SetInternalFieldCount(1);
  hidTemplate->SetClassName(NanNew<String>("HID"));

  NODE_SET_PROTOTYPE_METHOD(hidTemplate, "close", close);
  NODE_SET_PROTOTYPE_METHOD(hidTemplate, "read", read);
  NODE_SET_PROTOTYPE_METHOD(hidTemplate, "write", write);
  NODE_SET_PROTOTYPE_METHOD(hidTemplate, "getFeatureReport", getFeatureReport);
  NODE_SET_PROTOTYPE_METHOD(hidTemplate, "sendFeatureReport", sendFeatureReport);
  NODE_SET_PROTOTYPE_METHOD(hidTemplate, "setNonBlocking", setNonBlocking);

  target->Set(NanNew<String>("HID"), hidTemplate->GetFunction());
  target->Set(NanNew<String>("devices"), NanNew<FunctionTemplate>(HID::devices)->GetFunction());
}


extern "C" {
  
  static void init (Handle<Object> target)
  {
    NanScope();
    HID::Initialize(target);
  }

  NODE_MODULE(HID, init);
}


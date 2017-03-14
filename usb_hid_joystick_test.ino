#include <usbhid.h>
#include <hiduniversal.h>
#include <usbhub.h>


#define JOYSTICK
#define HID_IMP

//#define DEVICE_DESCRIPTOR _hidReportDescriptor_Thrust_Predator
#define DEVICE_DESCRIPTOR _hidReportDescriptor_Logitech_Wireless_Gamepad_F710
//#define DEVICE_DESCRIPTOR _hidReportDescriptor_Xbox_One
#define BLOCK_PIN A0


bool initSuccessful = false;
bool printOnce = false;
uint8_t    rcode;

HexDumper<USBReadParser, uint16_t, uint16_t>    Hex;
//ReportDescParser                                Rpt;

class HIDUniversal2 : public HIDUniversal
{
public:
	HIDUniversal2(USB *usb) : HIDUniversal(usb) {};

protected:
	uint8_t OnInitSuccessful()
	{

		GetReportDescr(0, &Hex);
		//GetReportDescr(0, &Rpt);

		initSuccessful = true;
		printOnce = true;
		Serial.println();
	}
};

#ifdef HID_IMP


#ifndef HID_h
#define HID_h

#include <stdint.h>
#include <Arduino.h>
#include "PluggableUSB.h"

#if defined(USBCON)

#define _USING_HID

// HID 'Driver'
// ------------
#define HID_GET_REPORT        0x01
#define HID_GET_IDLE          0x02
#define HID_GET_PROTOCOL      0x03
#define HID_SET_REPORT        0x09
#define HID_SET_IDLE          0x0A
#define HID_SET_PROTOCOL      0x0B

#define HID_HID_DESCRIPTOR_TYPE         0x21
#define HID_REPORT_DESCRIPTOR_TYPE      0x22
#define HID_PHYSICAL_DESCRIPTOR_TYPE    0x23

// HID subclass HID1.11 Page 8 4.2 Subclass
#define HID_SUBCLASS_NONE 0
#define HID_SUBCLASS_BOOT_INTERFACE 1

// HID Keyboard/Mouse bios compatible protocols HID1.11 Page 9 4.3 Protocols
#define HID_PROTOCOL_NONE 0
#define HID_PROTOCOL_KEYBOARD 1
#define HID_PROTOCOL_MOUSE 2

// Normal or bios protocol (Keyboard/Mouse) HID1.11 Page 54 7.2.5 Get_Protocol Request
// "protocol" variable is used for this purpose.
#define HID_BOOT_PROTOCOL	0
#define HID_REPORT_PROTOCOL	1

// HID Request Type HID1.11 Page 51 7.2.1 Get_Report Request
#define HID_REPORT_TYPE_INPUT   1
#define HID_REPORT_TYPE_OUTPUT  2
#define HID_REPORT_TYPE_FEATURE 3

typedef struct
{
	uint8_t len;      // 9
	uint8_t dtype;    // 0x21
	uint8_t addr;
	uint8_t versionL; // 0x101
	uint8_t versionH; // 0x101
	uint8_t country;
	uint8_t desctype; // 0x22 report
	uint8_t descLenL;
	uint8_t descLenH;
} HIDDescDescriptor;

typedef struct
{
	InterfaceDescriptor hid;
	HIDDescDescriptor   desc;
	EndpointDescriptor  in;
} HIDDescriptor;

class HIDSubDescriptor {
public:
	HIDSubDescriptor *next = NULL;
	HIDSubDescriptor(const void *d, const uint16_t l) : data(d), length(l) { }

	const void* data;
	const uint16_t length;
};
//class HIDSubDescriptor {
//public:
//	HIDSubDescriptor *next;
//	
//	uint8_t* data;
//	uint16_t length;
//};

//class HIDSubDescriptor {
//public:
//	HIDSubDescriptor *next = NULL;
//	HIDSubDescriptor(const void *d, const uint16_t l) : data(d), length(l) { }
//
//	const void* data;
//	const uint16_t length;
//};



class HID_ : public PluggableUSBModule
{
public:
	uint8_t epType[1];

	HIDSubDescriptor* rootNode;
	uint16_t descriptorSize;

	uint8_t protocol;
	uint8_t idle;

	HID_(void) : PluggableUSBModule(1, 1, epType),
		rootNode(NULL), descriptorSize(0),
		protocol(HID_REPORT_PROTOCOL), idle(1)
	{
		epType[0] = EP_TYPE_INTERRUPT_IN;
		PluggableUSB().plug(this);
	}

	int begin(void)
	{
		return 0;
	}

	int SendReport(uint8_t id, const void* data, int len);
	int SendReportWithoutId(const void* data, int len);
	void AppendDescriptor(HIDSubDescriptor* node);
	void AppendDescriptorTest(HIDSubDescriptor *node);

protected:
	// Implementation of the PluggableUSBModule
	int getInterface(uint8_t* interfaceCount);
	int getDescriptor(USBSetup& setup);
	bool setup(USBSetup& setup);
	uint8_t getShortName(char* name);



};

// Replacement for global singleton.
// This function prevents static-initialization-order-fiasco
// https://isocpp.org/wiki/faq/ctors#static-init-order-on-first-use
//HID_& HID();

#define D_HIDREPORT(length) { 9, 0x21, 0x01, 0x01, 0, 1, 0x22, lowByte(length), highByte(length) }

#endif // USBCON

#endif // HID_h

#if defined(USBCON)

//HID_& HID()
//{
//	static HID_ obj;
//	return obj;
//}

int HID_::getInterface(uint8_t* interfaceCount)
{
	*interfaceCount += 1; // uses 1
	HIDDescriptor hidInterface = {
		D_INTERFACE(pluggedInterface, 1, USB_DEVICE_CLASS_HUMAN_INTERFACE, HID_SUBCLASS_NONE, HID_PROTOCOL_NONE),
		D_HIDREPORT(descriptorSize),
		D_ENDPOINT(USB_ENDPOINT_IN(pluggedEndpoint), USB_ENDPOINT_TYPE_INTERRUPT, USB_EP_SIZE, 0x01)
	};
	return USB_SendControl(0, &hidInterface, sizeof(hidInterface));
}

int HID_::getDescriptor(USBSetup& setup)
{
	// Check if this is a HID Class Descriptor request
	if (setup.bmRequestType != REQUEST_DEVICETOHOST_STANDARD_INTERFACE) { return 0; }
	if (setup.wValueH != HID_REPORT_DESCRIPTOR_TYPE) { return 0; }

	// In a HID Class Descriptor wIndex cointains the interface number
	if (setup.wIndex != pluggedInterface) { return 0; }

	int total = 0;
	HIDSubDescriptor* node;
	for (node = rootNode; node; node = node->next) {
		int res = USB_SendControl(TRANSFER_PGM, node->data, node->length);
		if (res == -1)
			return -1;
		total += res;
	}

	// Reset the protocol on reenumeration. Normally the host should not assume the state of the protocol
	// due to the USB specs, but Windows and Linux just assumes its in report mode.
	protocol = HID_REPORT_PROTOCOL;

	return total;
}

uint8_t HID_::getShortName(char *name)
{
	name[0] = 'H';
	name[1] = 'I';
	name[2] = 'D';
	name[3] = 'A' + (descriptorSize & 0x0F);
	name[4] = 'A' + ((descriptorSize >> 4) & 0x0F);
	return 5;
}

void HID_::AppendDescriptor(HIDSubDescriptor *node)
{
	if (!rootNode) {
		rootNode = node;
	}
	else {
		HIDSubDescriptor *current = rootNode;
		while (current->next) {
			current = current->next;
		}
		current->next = node;
	}
	descriptorSize += node->length;
}

void HID_::AppendDescriptorTest(HIDSubDescriptor *node)
{
	rootNode = node;

	descriptorSize += node->length;
}

int HID_::SendReport(uint8_t id, const void* data, int len)
{
	auto ret = USB_Send(pluggedEndpoint, &id, 1);
	if (ret < 0) return ret;
	auto ret2 = USB_Send(pluggedEndpoint | TRANSFER_RELEASE, data, len);
	if (ret2 < 0) return ret2;
	return ret + ret2;
}

int HID_::SendReportWithoutId(const void* data, int len)
{
	auto ret2 = USB_Send(pluggedEndpoint | TRANSFER_RELEASE, data, len);
	if (ret2 < 0) return ret2;
	return ret2;
}

bool HID_::setup(USBSetup& setup)
{
	if (pluggedInterface != setup.wIndex) {
		return false;
	}

	uint8_t request = setup.bRequest;
	uint8_t requestType = setup.bmRequestType;

	if (requestType == REQUEST_DEVICETOHOST_CLASS_INTERFACE)
	{
		if (request == HID_GET_REPORT) {
			// TODO: HID_GetReport();
			return true;
		}
		if (request == HID_GET_PROTOCOL) {
			// TODO: Send8(protocol);
			return true;
		}
		if (request == HID_GET_IDLE) {
			// TODO: Send8(idle);
		}
	}

	if (requestType == REQUEST_HOSTTODEVICE_CLASS_INTERFACE)
	{
		if (request == HID_SET_PROTOCOL) {
			// The USB Host tells us if we are in boot or report mode.
			// This only works with a real boot compatible device.
			protocol = setup.wValueL;
			return true;
		}
		if (request == HID_SET_IDLE) {
			idle = setup.wValueL;
			return true;
		}
		if (request == HID_SET_REPORT)
		{
			//uint8_t reportID = setup.wValueL;
			//uint16_t length = setup.wLength;
			//uint8_t data[length];
			// Make sure to not read more data than USB_EP_SIZE.
			// You can read multiple times through a loop.
			// The first byte (may!) contain the reportID on a multreport.
			//USB_RecvControl(data, length);
		}
	}

	return false;
}



#endif /* if defined(USBCON) */

#endif

#ifdef JOYSTICK
#define JOYSTICK_REPORT_ID 0x01
#define JOYSTICK_STATE_SIZE 11
static const uint8_t _hidReportDescriptor[] PROGMEM = {

	// Joystick
	0x05, 0x01,			      // USAGE_PAGE (Generic Desktop)
	0x09, 0x04,			      // USAGE (Joystick)
	0xa1, 0x01,			      // COLLECTION (Application)
	0x85, JOYSTICK_REPORT_ID, //   REPORT_ID (3)

							  // 32 Buttons
							  0x05, 0x09,			      //   USAGE_PAGE (Button)
							  0x19, 0x01,			      //   USAGE_MINIMUM (Button 1)
							  0x29, 0x20,			      //   USAGE_MAXIMUM (Button 32)
							  0x15, 0x00,			      //   LOGICAL_MINIMUM (0)
							  0x25, 0x01,			      //   LOGICAL_MAXIMUM (1)
							  0x75, 0x01,			      //   REPORT_SIZE (1)
							  0x95, 0x20,			      //   REPORT_COUNT (32)
							  0x55, 0x00,			      //   UNIT_EXPONENT (0)
							  0x65, 0x00,			      //   UNIT (None)
							  0x81, 0x02,			      //   INPUT (Data,Var,Abs)

														  // 8 bit Throttle and Steering
														  0x05, 0x02,			      //   USAGE_PAGE (Simulation Controls)
														  0x15, 0x00,			      //   LOGICAL_MINIMUM (0)
														  0x26, 0xff, 0x00,	      //   LOGICAL_MAXIMUM (255)
														  0xA1, 0x00,			      //   COLLECTION (Physical)
														  0x09, 0xBB,			      //     USAGE (Throttle)
														  0x09, 0xBA,			      //     USAGE (Steering)
														  0x75, 0x08,			      //     REPORT_SIZE (8)
														  0x95, 0x02,			      //     REPORT_COUNT (2)
														  0x81, 0x02,			      //     INPUT (Data,Var,Abs)
														  0xc0,				      //   END_COLLECTION

																				  // Two Hat switches (8 Positions)
																				  0x05, 0x01,			      //   USAGE_PAGE (Generic Desktop)
																				  0x09, 0x39,			      //   USAGE (Hat switch)
																				  0x15, 0x00,			      //   LOGICAL_MINIMUM (0)
																				  0x25, 0x07,			      //   LOGICAL_MAXIMUM (7)
																				  0x35, 0x00,			      //   PHYSICAL_MINIMUM (0)
																				  0x46, 0x3B, 0x01,	      //   PHYSICAL_MAXIMUM (315)
																				  0x65, 0x14,			      //   UNIT (Eng Rot:Angular Pos)
																				  0x75, 0x04,			      //   REPORT_SIZE (4)
																				  0x95, 0x01,			      //   REPORT_COUNT (1)
																				  0x81, 0x02,			      //   INPUT (Data,Var,Abs)

																				  0x09, 0x39,			      //   USAGE (Hat switch)
																				  0x15, 0x00,			      //   LOGICAL_MINIMUM (0)
																				  0x25, 0x07,			      //   LOGICAL_MAXIMUM (7)
																				  0x35, 0x00,			      //   PHYSICAL_MINIMUM (0)
																				  0x46, 0x3B, 0x01,	      //   PHYSICAL_MAXIMUM (315)
																				  0x65, 0x14,			      //   UNIT (Eng Rot:Angular Pos)
																				  0x75, 0x04,			      //   REPORT_SIZE (4)
																				  0x95, 0x01,			      //   REPORT_COUNT (1)
																				  0x81, 0x02,			      //   INPUT (Data,Var,Abs)

																											  // X, Y, and Z Axis
																											  0x15, 0x00,			      //   LOGICAL_MINIMUM (0)
																											  0x26, 0xff, 0x00,	      //   LOGICAL_MAXIMUM (255)
																											  0x75, 0x08,			      //   REPORT_SIZE (8)
																											  0x09, 0x01,			      //   USAGE (Pointer)
																											  0xA1, 0x00,			      //   COLLECTION (Physical)
																											  0x09, 0x30,		          //     USAGE (x)
																											  0x09, 0x31,		          //     USAGE (y)
																											  0x09, 0x32,		          //     USAGE (z)
																											  0x09, 0x33,		          //     USAGE (rx)
																																		  //0x09, 0x34,		          //     USAGE (ry)
																																		  //0x09, 0x35,		          //     USAGE (rz)
																																		  //0x95, 0x06,		          //     REPORT_COUNT (6)
																																		  0x95, 0x04,
																																		  0x81, 0x02,		          //     INPUT (Data,Var,Abs)
																																		  0xc0,				      //   END_COLLECTION

																																		  0xc0				      // END_COLLECTION
};
static const uint8_t _hidReportDescriptor_Thrust_Predator[] PROGMEM = { 0x5, 0x1,
0x9, 0x4,
0xA1, 0x1,
0x9, 0x1,
0xA1, 0x0,
0x9, 0x30, 0x9, 0x31, 0x9, 0x35, 0x9, 0x36, 0x9, 0x32, 0x15,
0x0, 0x26, 0xFF, 0x0, 0x75, 0x8, 0x95, 0x5, 0x81, 0x2, 0xC0,
0x9, 0x39, 0x15, 0x0, 0x25, 0x7, 0x35, 0x0, 0x46, 0x3B, 0x1,
0x65, 0x14, 0x75, 0x4,0x95, 0x1, 0x81, 0x42, 0x75, 0x4, 0x95,
0x1, 0x81, 0x1, 0x5, 0x9, 0x19, 0x1, 0x29, 0xD, 0x15, 0x0,
0x25, 0x1, 0x75, 0x1, 0x95, 0xD, 0x55, 0x0, 0x65,0x0, 0x81,
0x2, 0x75, 0x1, 0x95, 0x3, 0x81, 0x1, 0x5, 0x8C, 0x9, 0x1,
0xA1, 0x0, 0x9, 0x2, 0x15, 0x0, 0x26, 0xFF, 0x0, 0x75, 0x8,
0x95, 0x3, 0x91, 0x2, 0x9, 0x2, 0xB1, 0x2, 0xC0, 0xC0 };
static  const uint8_t _hidReportDescriptor_Logitech_Wireless_Gamepad_F710[] PROGMEM = { 0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0xA1, 0x02, 0x85, 0x01, 0x75, 0x08, 0x95, 0x04, 0x15, 0x00,
0x26, 0xFF, 0x00, 0x35, 0x00, 0x46, 0xFF, 0x00, 0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35,
0x81, 0x02, 0x75, 0x04, 0x95, 0x01, 0x25, 0x07, 0x46, 0x3B, 0x01, 0x66, 0x14, 0x00, 0x09, 0x39,
0x81, 0x42, 0x66, 0x00, 0x00, 0x75, 0x01, 0x95, 0x0C, 0x25, 0x01, 0x45, 0x01, 0x05, 0x09, 0x19,
0x01, 0x29, 0x0C, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x06, 0x00, 0xFF, 0x26, 0xFF, 0x00, 0x46,
0xFF, 0x00, 0x09, 0x00, 0x81, 0x02, 0xC0, 0xA1, 0x02, 0x85, 0x02, 0x95, 0x07, 0x75, 0x08, 0x26,
0xFF, 0x00, 0x46, 0xFF, 0x00, 0x06, 0x00, 0xFF, 0x09, 0x03, 0x81, 0x02, 0xC0, 0xA1, 0x02, 0x85,
0x03, 0x09, 0x04, 0x91, 0x02, 0xC0, 0xC0 };
static const uint8_t _hidReportDescriptor_Xbox_One[] PROGMEM = {
	0x05, 0x01,
	0x09, 0x05,
	0xA1, 0x01,
	0x05, 0x01,
	0x09, 0x3A,
	0xA1, 0x02,
	0x75, 0x08,
	0x95, 0x01,
	0x81, 0x01,
	0x75, 0x08,
	0x95, 0x01,
	0x05, 0x01,
	0x09, 0x3B,
	0x81, 0x01,
	0x05, 0x01,
	0x09, 0x01,
	0xA1, 0x00,
	0x75, 0x01,
	0x15, 0x00,
	0x25, 0x01,
	0x35, 0x00,
	0x45, 0x01,
	0x95, 0x04,
	0x05, 0x01,
	0x09, 0x90,
	0x09, 0x91,
	0x09, 0x93,
	0x09, 0x92,
	0x81, 0x02,
	0xC0,
	0x75, 0x01,
	0x15, 0x00,
	0x25, 0x01,
	0x35, 0x00,
	0x45, 0x01,
	0x95, 0x04,
	0x05, 0x09,
	0x19, 0x07,
	0x29, 0x0A,
	0x81, 0x02,
	0x75, 0x01,
	0x95, 0x08,
	0x81, 0x01,
	0x75, 0x08,
	0x15, 0x00,
	0x26, 0xFF, 0x00,
	0x35, 0x00,
	0x46, 0xFF, 0x00,
	0x95, 0x06,
	0x05, 0x09,
	0x19, 0x01,
	0x29, 0x06,
	0x81, 0x02,
	0x75, 0x08,
	0x15, 0x00,
	0x26, 0xFF, 0x00,
	0x35, 0x00,
	0x46, 0xFF, 0x00,
	0x95, 0x02,
	0x05, 0x01,
	0x09, 0x32,
	0x09, 0x35,
	0x81, 0x02,
	0x75, 0x10,
	0x16, 0x00, 0x80,
	0x26, 0xFF, 0x7F,
	0x36, 0x00, 0x80,
	0x46, 0xFF, 0x7F,
	0x05, 0x01,
	0x09, 0x01,
	0xA1, 0x00,
	0x95, 0x02,
	0x05, 0x01,
	0x09, 0x30,
	0x09, 0x31,
	0x81, 0x02,
	0xC0,
	0x05, 0x01,
	0x09, 0x01,
	0xA1, 0x00,
	0x95, 0x02,
	0x05, 0x01,
	0x09, 0x33,
	0x09, 0x34,
	0x81, 0x02,
	0xC0,
	0xC0,
	0x05, 0x01,
	0x09, 0x3A,
	0xA1, 0x02,
	0x75, 0x08,
	0x95, 0x01,
	0x91, 0x01,
	0x75, 0x08,
	0x95, 0x01,
	0x05, 0x01,
	0x09, 0x3B,
	0x91, 0x01,
	0x75, 0x08,
	0x95, 0x01,
	0x91, 0x01,
	0x75, 0x08,
	0x15, 0x00,
	0x26, 0xFF, 0x00,
	0x35, 0x00,
	0x46, 0xFF, 0x00,
	0x95, 0x01,
	0x06, 0x01, 0x00,
	0x09, 0x01,
	0x91, 0x02,
	0x75, 0x08,
	0x95, 0x01,
	0x91, 0x01,
	0x75, 0x08,
	0x15, 0x00,
	0x26, 0xFF, 0x00,
	0x35, 0x00,
	0x46, 0xFF, 0x00,
	0x95, 0x01,
	0x06, 0x01, 0x00,
	0x09, 0x02,
	0x91, 0x02,
	0xC0,
	0xC0
};

class JoystickData : public HIDReportParser
{
private:
	HID_ _Hid;
	uint8_t* _data;

	uint8_t _dataLen = 0;
	bool _dataReady = false;

	virtual void Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf)
	{
		_data = buf;
		_dataLen = len;
		_dataReady = true;
	};
public:

	JoystickData()
	{
		//// Setup HID report structure
		static  HIDSubDescriptor node(DEVICE_DESCRIPTOR, sizeof(DEVICE_DESCRIPTOR));
		_Hid.AppendDescriptor(&node);

	}

	void SendRuntimeData()
	{
		if(_dataReady)
		{
			_Hid.SendReportWithoutId(_data, _dataLen);
			_dataReady = false;
		}
	}
	

	HID_ GetHIDInstance()
	{
		return _Hid;
	}

	uint8_t GetDataLenght()
	{
		return _dataLen;
	}

	uint8_t* GetData()
	{
		return _data;
	}

	bool DataReady()
	{
		return _dataReady;
	}


};
#endif //JOYSTICK

USB Usb;
USBHub Hub(&Usb);
HIDUniversal2 Hid(&Usb);
JoystickData joystickData;



void setup()
{
	pinMode(BLOCK_PIN, INPUT);


	Serial.begin(115200);
	
	Serial.println("Starting...");
	delay(1000);

	if (Usb.Init() == -1)
		Serial.println("OSC did not start.");

	delay(200);

	if (!Hid.SetReportParser(0, &joystickData))
		ErrorMessage<uint8_t >(PSTR("SetReportParser"), 1);


	Serial.print("sizeof : "); Serial.println(sizeof(DEVICE_DESCRIPTOR));
	Serial.print("descSize : "); Serial.println(joystickData.GetHIDInstance().descriptorSize);
	Serial.print("rootNode Len : "); Serial.println(joystickData.GetHIDInstance().rootNode->length);
	Serial.print("next Len : "); Serial.println(joystickData.GetHIDInstance().rootNode->next->length);
}

void loop() {
	Usb.Task();
	if (initSuccessful)
	{
		if (printOnce)
		{
			Serial.println("Initialization Succesful!");
			//joystickData.Initialize(_hidReportDescriptorSample_2, 119);
			printOnce = false;
		}

		else
		{

			if(digitalRead(BLOCK_PIN))
			{
				//send parsed data through HID
				joystickData.SendRuntimeData();

			}
	

			
		}

	}
}





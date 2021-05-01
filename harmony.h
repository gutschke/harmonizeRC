#pragma once

#include <linux/hid.h>
#include <libusb-1.0/libusb.h>

#include <functional>
#include <map>

#include "event.h"

// Handles USB hotplugging, and can support multiple remotes. But only works
// with a single Logitech Unifying receiver. If more than one receiver is
// attached, the behavior is undefined (but shouldn't crash).
// There is both a synchronous and an asynchronous API. Internally, the latter
// is implemented in terms of the former. Mixing both modes isn't recommended.
// At any given time, there should only be a single active USB request in
// flight. This means that special care must be taken if using this class
// from multiple threads.
class Harmony {
public:
  Harmony(Event *event = NULL);
  ~Harmony();
  unsigned int getKey();
  void setKeyCallback(std::function<void (int key)> cb);
  bool sendHIDppRequest(const unsigned char *buf,
                   std::function<void (int, const unsigned char *)> cb = NULL,
                   std::function<void (int, const unsigned char *)> err = NULL);
  bool sendHIDppRequestAndWait(const unsigned char *buf,
                   std::function<void (int, const unsigned char *)> cb = NULL,
                   std::function<void (int, const unsigned char *)> err = NULL);
  static const char *toString(int key);

  enum {
    KEY_OFF       = 0x3EC01, KEY_LONG_OFF      = 0x7EC01,
    KEY_DEVICE_1  = 0x3E801, KEY_LONG_DEVICE_1 = 0x7E801,
    KEY_DEVICE_2  = 0x3ED01, KEY_LONG_DEVICE_2 = 0x7ED01,
    KEY_DEVICE_3  = 0x3E901, KEY_LONG_DEVICE_3 = 0x7E901,
    KEY_DIM_UP    = 0x3F00f, KEY_LONG_DIM_UP   = 0x7F00f,
    KEY_DIM_DOWN  = 0x3F10f, KEY_LONG_DIM_DOWN = 0x7F10f,
    KEY_BULB_1    = 0x3F20F, KEY_LONG_BULB_1   = 0x7F20F,
    KEY_BULB_2    = 0x3F30F, KEY_LONG_BULB_2   = 0x7F30F,
    KEY_PLUG_1    = 0x3F40F, KEY_LONG_PLUG_1   = 0x7F40F,
    KEY_PLUG_2    = 0x3F50F, KEY_LONG_PLUG_2   = 0x7F50F,
    KEY_RED       = 0x3F701, KEY_LONG_RED      = 0x7F701,
    KEY_GREEN     = 0x3F601, KEY_LONG_GREEN    = 0x7F601,
    KEY_YELLOW    = 0x3F501, KEY_LONG_YELLOW   = 0x7F501,
    KEY_BLUE      = 0x3F401, KEY_LONG_BLUE     = 0x7F401,
    KEY_DVR       = 0x39A00, KEY_LONG_DVR      = 0x79A00,
    KEY_GUIDE     = 0x38D00, KEY_LONG_GUIDE    = 0x78D00,
    KEY_INFO      = 0x3FF01, KEY_LONG_INFO     = 0x7FF01,
    KEY_EXIT      = 0x39400, KEY_LONG_EXIT     = 0x79400,
    KEY_MENU      = 0x10065, KEY_LONG_MENU     = 0x50065,
    KEY_VOL_UP    = 0x3E900, KEY_LONG_VOL_UP   = 0x7E900,
    KEY_VOL_DOWN  = 0x3EA00, KEY_LONG_VOL_DOWN = 0x7EA00,
    KEY_CHN_UP    = 0x39C00, KEY_LONG_CHN_UP   = 0x79C00,
    KEY_CHN_DOWN  = 0x39D00, KEY_LONG_CHN_DOWN = 0x79D00,
    KEY_UP        = 0x10052, KEY_LONG_UP       = 0x50052,
    KEY_DOWN      = 0x10051, KEY_LONG_DOWN     = 0x50051,
    KEY_LEFT      = 0x10050, KEY_LONG_LEFT     = 0x50050,
    KEY_RIGHT     = 0x1004F, KEY_LONG_RIGHT    = 0x5004F,
    KEY_OK        = 0x10058, KEY_LONG_OK       = 0x50058,
    KEY_MUTE      = 0x3E200, KEY_LONG_MUTE     = 0x7E200,
    KEY_BACK      = 0x32402, KEY_LONG_BACK     = 0x72402,
    KEY_RWD       = 0x3B400, KEY_LONG_RWD      = 0x7B400,
    KEY_FWD       = 0x3B300, KEY_LONG_FWD      = 0x7B300,
    KEY_RECORD    = 0x3B200, KEY_LONG_RECORD   = 0x7B200,
    KEY_STOP      = 0x3B700, KEY_LONG_STOP     = 0x7B700,
    KEY_PLAY      = 0x3B000, KEY_LONG_PLAY     = 0x7B000,
    KEY_PAUSE     = 0x3B100, KEY_LONG_PAUSE    = 0x7B100,
    KEY_NUM1      = 0x1001E, KEY_LONG_NUM1     = 0x5001E,
    KEY_NUM2      = 0x1001F, KEY_LONG_NUM2     = 0x5001F,
    KEY_NUM3      = 0x10020, KEY_LONG_NUM3     = 0x50020,
    KEY_NUM4      = 0x10021, KEY_LONG_NUM4     = 0x50021,
    KEY_NUM5      = 0x10022, KEY_LONG_NUM5     = 0x50022,
    KEY_NUM6      = 0x10023, KEY_LONG_NUM6     = 0x50023,
    KEY_NUM7      = 0x10024, KEY_LONG_NUM7     = 0x50024,
    KEY_NUM8      = 0x10025, KEY_LONG_NUM8     = 0x50025,
    KEY_NUM9      = 0x10026, KEY_LONG_NUM9     = 0x50026,
    KEY_NUM0      = 0x10027, KEY_LONG_NUM0     = 0x50027,
    KEY_CLEAR     = 0x10056, KEY_LONG_CLEAR    = 0x50056,
    KEY_ENTER     = 0x10028, KEY_LONG_ENTER    = 0x50028,
    KEY_LONGPRESS = 0x40000,
  };

private:
  enum {
    HARMONY_TRANSFER_SIZE      = 32,
    HARMONY_CONFIG_INDEX       = 0,
    HARMONY_DJ_INDEX           = 2,
    HARMONY_ALT_SETTING_INDEX  = 0,
    HARMONY_ENDPOINT_INDEX     = 0,
    HARMONY_VENDOR_ID          = 0x46d,
    HARMONY_PRODUCT_ID         = 0xc52b,
    HARMONY_TIMEOUT            = 10*1000,
    HARMONY_LONGPRESS          = 250,
    HARMONY_REPORT_ID_IDX      = 0,
    HARMONY_REPORT_HIDPP_SHORT = 0x10,
    HARMONY_REPORT_HIDPP_LONG  = 0x11,
    HARMONY_REPORT_DJ_SHORT    = 0x20,
    HARMONY_REPORT_DJ_LONG     = 0x21,
    HARMONY_HIDPP_SHORT_COUNT  = 3 + 3,  // 0x10:  7 bytes
    HARMONY_HIDPP_LONG_COUNT   = 3 + 16, // 0x11: 20 bytes
    HARMONY_DJ_SHORT_COUNT     = 3 + 11, // 0x20: 15 bytes
    HARMONY_DJ_LONG_COUNT      = 3 + 28, // 0x21: 32 bytes
    HARMONY_DEVICE_IDX         = 1,
    HARMONY_SUBID_IDX          = 2,
    HARMONY_SUBID_ERROR        = 0x8F,
    HARMONY_SUBID_ERROR2       = 0xFF,
    HARMONY_SUBID_KEYBOARD     = 1,
    HARMONY_SUBID_CONSUMER_CTRL= 3,
    HARMONY_SUBID_CONN_NOTIF   = 0x42,
    HARMONY_KEY_MSB_IDX        = 3,
    HARMONY_KEY_LSB_IDX        = 4,
    HARMONY_ERROR_IDX          = 6
  };

  Event *event;
  libusb_context *ctx = NULL;
  libusb_device_handle *deviceHandle = NULL;
  unsigned firmware = 0;
  libusb_hotplug_callback_handle hotplugHandleAttach = 0;
  libusb_hotplug_callback_handle hotplugHandleDetach = 0;
  std::map<int, void *> pollHandlers;
  const libusb_interface_descriptor *ifaceDJDesc = NULL;
  unsigned tm = 0;
  int key = 0;
  unsigned char buffer[HARMONY_TRANSFER_SIZE];
  libusb_transfer *transfer = NULL;
  int completed = 1;
  std::function<void (int key)> keyCallback = NULL;
  unsigned char hidPPBuffer[HARMONY_HIDPP_LONG_COUNT + 1];
  std::function<void (int len, const unsigned char *buf)> hidPPCallback = NULL;
  std::function<void (int len, const unsigned char *buf)> hidPPError = NULL;

  static const struct Map { int code; const char *str; } map[];

  static int getReportLength(unsigned char ch);
  static int hotplugAttach(libusb_context *ctx, libusb_device *dev,
                           libusb_hotplug_event event, void *data);
  static int hotplugDetach(libusb_context *ctx, libusb_device *dev,
                           libusb_hotplug_event event, void *data);
  void getFirmwareVersion(int retries = 10);
  void initializeReceiver();
  static bool openInterface(libusb_device_handle *handle,
                            const libusb_interface_descriptor **ifaceDJDesc);
  libusb_device_handle *openDevice();
  static void transferCompleted(libusb_transfer *transfer);
  void cancelPendingTransfer();
  void handleUsbPollFdEvent();
};

#include <stdlib.h>
#include <string.h>

#include <algorithm>

#if !defined(NDEBUG)
#include <iomanip>
#include <iostream>
#endif

#include "harmony.h"
#include "util.h"

const Harmony::Map Harmony::map[] = {
  { 0x1001E, "LONG NUM 1" },
  { 0x1001F, "LONG NUM 2" },
  { 0x10020, "LONG NUM 3" },
  { 0x10021, "LONG NUM 4" },
  { 0x10022, "LONG NUM 5" },
  { 0x10023, "LONG NUM 6" },
  { 0x10024, "LONG NUM 7" },
  { 0x10025, "LONG NUM 8" },
  { 0x10026, "LONG NUM 9" },
  { 0x10027, "LONG NUM 0" },
  { 0x10028, "LONG ENTER" },
  { 0x1004F, "LONG RIGHT" },
  { 0x10050, "LONG LEFT" },
  { 0x10051, "LONG DOWN" },
  { 0x10052, "LONG UP" },
  { 0x10056, "LONG CLEAR" },
  { 0x10058, "LONG OK" },
  { 0x10065, "LONG MENU" },
  { 0x32402, "LONG BACK" },
  { 0x38D00, "LONG GUIDE" },
  { 0x39400, "LONG EXIT" },
  { 0x39A00, "LONG DVR" },
  { 0x39C00, "LONG CHN UP" },
  { 0x39D00, "LONG CHN DOWN" },
  { 0x3B000, "LONG PLAY" },
  { 0x3B100, "LONG PAUSE" },
  { 0x3B200, "LONG RECORD" },
  { 0x3B300, "LONG FWD" },
  { 0x3B400, "LONG RWD" },
  { 0x3B700, "LONG STOP" },
  { 0x3E200, "LONG MUTE" },
  { 0x3E801, "LONG DEVICE 1" },
  { 0x3E900, "LONG VOL UP" },
  { 0x3E901, "LONG DEVICE 3" },
  { 0x3EA00, "LONG VOL DOWN" },
  { 0x3EC01, "LONG OFF" },
  { 0x3ED01, "LONG DEVICE 2" },
  { 0x3F00f, "LONG DIM UP" },
  { 0x3F10f, "LONG DIM DOWN" },
  { 0x3F20F, "LONG BULB 1" },
  { 0x3F30F, "LONG BULB 2" },
  { 0x3F401, "LONG BLUE" },
  { 0x3F40F, "LONG PLUG 1" },
  { 0x3F501, "LONG YELLOW" },
  { 0x3F50F, "LONG PLUG 2" },
  { 0x3F601, "LONG GREEN" },
  { 0x3F701, "LONG RED" },
  { 0x3FF01, "LONG INFO" },
};

Harmony::Harmony(Event *event) : event(event) {
  libusb_init(&ctx);
#ifdef NDEBUG
# ifdef LIBUSB_OPTION_LOG_LEVEL
    libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_NONE);
# else
    libusb_set_debug(ctx, LIBUSB_LOG_LEVEL_NONE);
# endif
#else
# ifdef LIBUSB_OPTION_LOG_LEVEL
    libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);
# else
    libusb_set_debug(ctx, LIBUSB_LOG_LEVEL_WARNING);
# endif
#endif
  if (event) {
    auto pollFds = libusb_get_pollfds(ctx);
    for (auto it = pollFds; *it; it++) {
      pollHandlers[(*it)->fd] =
        event->addPollFd((*it)->fd, (*it)->events, [this]() {
            handleUsbPollFdEvent(); });
    }
    free(pollFds);
    libusb_set_pollfd_notifiers(ctx,
      [](int fd, short events, void *data) {
        Harmony *that = (Harmony *)data;
        Event *event = that->event;
        that->pollHandlers[fd] = event->addPollFd(fd, events, [that]() {
                                            that->handleUsbPollFdEvent(); }); },
      [](int fd, void *data) {
        Harmony *that = (Harmony *)data;
        Event *event = that->event;
        event->removePollFd(that->pollHandlers[fd]); },
      this);
  }
  openDevice();
  libusb_hotplug_register_callback(
    NULL, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, LIBUSB_HOTPLUG_NO_FLAGS,
    HARMONY_VENDOR_ID, HARMONY_PRODUCT_ID, LIBUSB_HOTPLUG_MATCH_ANY,
    hotplugAttach, (void *)this, &hotplugHandleAttach);
  libusb_hotplug_register_callback (
    NULL, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, LIBUSB_HOTPLUG_NO_FLAGS,
    HARMONY_VENDOR_ID, HARMONY_PRODUCT_ID, LIBUSB_HOTPLUG_MATCH_ANY,
    hotplugDetach, (void *)this, &hotplugHandleDetach);
  initializeReceiver();
}

Harmony::~Harmony() {
  setKeyCallback(NULL);
  if (event) {
    libusb_set_pollfd_notifiers(ctx, NULL, NULL, NULL);
    auto pollFds = libusb_get_pollfds(ctx);
    for (auto it = pollFds; *it; it++) {
      event->removePollFd(pollHandlers[(*it)->fd]);
    }
    free(pollFds);
  }
  if (deviceHandle) {
    libusb_release_interface(deviceHandle, HARMONY_DJ_INDEX);
    libusb_attach_kernel_driver(deviceHandle, HARMONY_DJ_INDEX);
    libusb_close(deviceHandle);
  }
  if (hotplugHandleAttach) {
    libusb_hotplug_deregister_callback(ctx, hotplugHandleAttach);
  }
  if (hotplugHandleDetach) {
    libusb_hotplug_deregister_callback(ctx, hotplugHandleDetach);
  }
  libusb_exit(ctx);
}

unsigned int Harmony::getKey() {
  bool isHIDpp = hidPPCallback || hidPPError;
  auto oldCallback = keyCallback;
  int input = 0;
  while ((!isHIDpp && !input) ||
         (isHIDpp && (hidPPCallback || hidPPError))) {
    if (completed) {
      setKeyCallback([&input](int key) { input = key; });
    }
    libusb_handle_events_completed(ctx, &completed);
  }
  setKeyCallback(oldCallback);
  return input;
}

void Harmony::setKeyCallback(std::function<void (int key)> cb) {
  keyCallback = cb;
  if (cb == NULL) {
    cancelPendingTransfer();
    key = 0;
    return;
  }
  openDevice();
  if (!deviceHandle) {
    cancelPendingTransfer();
    key = 0;
  } else if (completed) {
    int waitTime = HARMONY_TIMEOUT;
    if (key) {
      waitTime = HARMONY_LONGPRESS - (int)(Util::millis() - tm);
      waitTime = std::min((int)HARMONY_TIMEOUT, std::max(1, waitTime));
    }
    transfer = libusb_alloc_transfer(0);
    completed = 0;
    libusb_fill_interrupt_transfer(transfer, deviceHandle,
      ifaceDJDesc->endpoint[HARMONY_ENDPOINT_INDEX].bEndpointAddress,
      buffer, sizeof(buffer), transferCompleted, this, waitTime);
    if (transfer && libusb_submit_transfer(transfer) != LIBUSB_SUCCESS) {
      // Maybe the device doesn't currently exist. Let's hope that a hotplug
      // event is going to fix things for us. There really isn't any other
      // error recovery that we could do here.
      libusb_free_transfer(transfer);
      transfer = NULL;
      completed = 1;
    }
  }
}

int Harmony::getReportLength(unsigned char ch) {
  if (ch == HARMONY_REPORT_HIDPP_SHORT) {
    return HARMONY_HIDPP_SHORT_COUNT + 1;
  } else if (ch == HARMONY_REPORT_HIDPP_LONG) {
    return HARMONY_HIDPP_LONG_COUNT + 1;
  } else if (ch == HARMONY_REPORT_DJ_SHORT) {
    return HARMONY_DJ_SHORT_COUNT + 1;
  } else if (ch == HARMONY_REPORT_DJ_LONG) {
    return HARMONY_DJ_LONG_COUNT + 1;
  } else {
    return 0;
  }
}

bool Harmony::sendHIDppRequest(const unsigned char *buf,
                        std::function<void (int, const unsigned char *)> cb,
                        std::function<void (int, const unsigned char *)> err) {
  const bool isDJ = buf[0] == HARMONY_REPORT_DJ_SHORT ||
                    buf[0] == HARMONY_REPORT_DJ_LONG;
  int len = getReportLength(buf[HARMONY_REPORT_ID_IDX]);
  if (!len || (!isDJ && (hidPPCallback || hidPPError)) || !openDevice()) {
    return false;
  }
  if (!isDJ) {
    hidPPCallback = cb;
    hidPPError = err;
    if (cb || err) {
      memset(hidPPBuffer, 0, sizeof(hidPPBuffer));
      memcpy(hidPPBuffer, buf, std::min((int)sizeof(hidPPBuffer), len));
    }
  }
  for (;;) {
#if !defined(NDEBUG)
    std::cout << "[ ";
    for (int i = 0; i < len; i++) {
      std::cout << std::hex << std::setw(2) << std::setfill('0')
                << (0xFF & (unsigned)buf[i])
                << std::dec << std::setw(0);
      if (i != len - 1) {
        std::cout << ", ";
      }
    }
    std::cout << " ]" << std::endl;
#endif

    int rc = libusb_control_transfer(deviceHandle,
      LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE|LIBUSB_ENDPOINT_OUT,
      0x09 /* HID Set_Report */, (2 /* HID output */ << 8) | buf[0],
      HARMONY_DJ_INDEX, (unsigned char *)buf, len, HARMONY_TIMEOUT);
    if (rc != len) {
      memset(hidPPBuffer, 0, sizeof(hidPPBuffer));
      if (!isDJ) {
        hidPPCallback = NULL;
        hidPPError = NULL;
      }
      return false;
    }
    break;
  }
  return true;
}

bool Harmony::sendHIDppRequestAndWait(const unsigned char *buf,
                        std::function<void (int, const unsigned char *)> cb,
                        std::function<void (int, const unsigned char *)> err) {
  const bool isDJ = buf[0] == HARMONY_REPORT_DJ_SHORT ||
                    buf[0] == HARMONY_REPORT_DJ_LONG;
  if ((!isDJ && (hidPPCallback || hidPPError)) ||
      !sendHIDppRequest(buf, cb, err)) {
    return false;
  }
  while (!isDJ && (hidPPCallback || hidPPError)) {
    getKey();
  }
  return true;
}


const char *Harmony::toString(int key) {
  int code = key & ~KEY_LONGPRESS;
  struct Map *entry =
    (struct Map *)bsearch(&code, &map,
                          sizeof(map)/sizeof(struct Map),
                          sizeof(struct Map),
                          [](const void *a, const void *b) -> int {
                            return *(int *)a - *(int *)b; });
  if (!entry) {
    return "UNKNOWN KEY";
  } else {
    return key & KEY_LONGPRESS ? entry->str : entry->str + 5;
  }
}

int Harmony::hotplugAttach(libusb_context *ctx,
                           libusb_device *dev,
                           libusb_hotplug_event event,
                           void *data) {
  // Attach event received
  class Harmony *that = (class Harmony *)data;
  libusb_device_descriptor desc;
  if (libusb_get_device_descriptor(dev, &desc) == LIBUSB_SUCCESS) {
    libusb_device_handle *handle;
    if (libusb_open(dev, &handle) == LIBUSB_SUCCESS &&
        openInterface(handle, &that->ifaceDJDesc)) {
      // New Unifying receiver detected
      if (that->deviceHandle) {
        that->cancelPendingTransfer();
        libusb_close(that->deviceHandle);
      }
      that->deviceHandle = handle;
      that->setKeyCallback(that->keyCallback);
      that->initializeReceiver();
    }
  }
  return 0;
}

int Harmony::hotplugDetach(libusb_context *ctx,
                           libusb_device *dev,
                           libusb_hotplug_event event,
                           void *data) {
  // Detach event received
  class Harmony *that = (class Harmony *)data;
  if (that->deviceHandle &&
      libusb_get_device(that->deviceHandle) == dev) {
    // Unifying receiver removed
    that->cancelPendingTransfer();
    libusb_close(that->deviceHandle);
    that->deviceHandle = NULL;
    that->ifaceDJDesc = NULL;
  }
  return 0;
}

void Harmony::getFirmwareVersion(int retries) {
  for (;;) {
    // Request major and minor version numbers
    sendHIDppRequestAndWait(
      (unsigned char *)"\x10\xFF\x81\xF1\x01\x00\x00",
      [this](int, const unsigned char *buffer) {
        firmware = ((unsigned)buffer[5] << 24) |
                   ((unsigned)buffer[6] << 16);
      }, [](int, const unsigned char *){ });
    if (firmware) {
      sendHIDppRequestAndWait(
        (unsigned char *)"\x10\xFF\x81\xF1\x02\x00\x00",
        [this](int, const unsigned char *buffer) {
          firmware |= ((unsigned)buffer[5] << 8) |
                       (unsigned)buffer[6];
        }, [](int, const unsigned char *){ });
    }
    // Sometimes, the USB device isn't quite ready to respond. Retry a couple
    // of times. Depending on whether we have an event loop, this is either a
    // synchronous or asynchronous operation
    if (event) {
      if (!firmware && retries-- > 0) {
        event->addTimeout(1000, [this, retries]() {
                                  getFirmwareVersion(retries);
                                });
      }
      break;
    } else {
      if (firmware || !retries--) {
        break;
      } else {
        poll(0, 0, 1000);
      }
    }
  }
  // In debug builds, warn about unsupported firmware versions. Only older
  // unifying receivers can report all the keys on the Harmony remote. More
  // modern firmware broke this feature and all "media" keys are silently
  // ignored. This means all the directional keys, the menu key, and the
  // numbers. Unfortunately, once upgraded there is no way to downgrade a
  // unifying receiver. Scour EBay until you find an old device.
#if !defined(NDEBUG)
  if (firmware && firmware > 0x12030025) {
    std::cout << "Firmware version of unifying receiver " << std::hex
              << firmware << " is probably too new. Downgrade, if direction "
                 "keys on remote don't work." << std::endl;
  }
#endif
}

void Harmony::initializeReceiver() {
  // Enable DJ mode & notifications
  sendHIDppRequestAndWait(
    (unsigned char *)"\x20\xFF\x80\x3F\x00\x00\x00\x00"
                     "\x00\x00\x00\x00\x00\x00\x00");
  sendHIDppRequestAndWait(
    (unsigned char *)"\x10\xFF\x80\x00\x00\x09\x00",
    [](int, const unsigned char *){ });
  // Determine firmware version of unifying receiver
  firmware = 0;
  getFirmwareVersion();
}

bool Harmony::openInterface(libusb_device_handle *handle,
                            const libusb_interface_descriptor **ifaceDJDesc) {
  libusb_device *device;
  libusb_device_descriptor desc;
  libusb_config_descriptor *config;
  if (!handle ||
      (device = libusb_get_device(handle)) == NULL ||
      libusb_get_device_descriptor(device, &desc) != LIBUSB_SUCCESS ||
      libusb_get_config_descriptor(device, HARMONY_CONFIG_INDEX, &config) !=
      LIBUSB_SUCCESS) {
    return false;
  }
  *ifaceDJDesc = &config->interface[HARMONY_DJ_INDEX].
                 altsetting[HARMONY_ALT_SETTING_INDEX];
  libusb_detach_kernel_driver(handle, HARMONY_DJ_INDEX);
  libusb_claim_interface(handle, HARMONY_DJ_INDEX);

  return true;
}

libusb_device_handle *Harmony::openDevice() {
  if (!deviceHandle) {
    // Look for Logitech Unifying receiver
    libusb_device_handle *handle;
    if ((handle = libusb_open_device_with_vid_pid(
           ctx, HARMONY_VENDOR_ID, HARMONY_PRODUCT_ID)) != NULL &&
        openInterface(handle, &ifaceDJDesc)) {
      // Opened Unifying receiver
      deviceHandle = handle;
    }
  }
  return deviceHandle;
}

void Harmony::transferCompleted(libusb_transfer *transfer) {
  Harmony *that = (Harmony *)transfer->user_data;
  if (transfer != that->transfer) {
    // What just happened?! There should only ever be a single transfer
    // in flight!
#if !defined(NDEBUG)
    std::cout << "Unexpected transfer completion" << std::endl;
#endif
    return;
  }
  const auto status = transfer->status;
  const auto actual_length = transfer->actual_length;
  libusb_free_transfer(transfer);
  that->transfer = NULL;
  that->completed = 1;
  if (status == LIBUSB_TRANSFER_TIMED_OUT && that->key) {
    if (that->keyCallback) {
      that->keyCallback(that->key | KEY_LONGPRESS);
    }
    that->key = 0;
  } else if (status != LIBUSB_TRANSFER_COMPLETED) {
    that->key = 0;
  } else {
    const auto buffer = that->buffer;
#if !defined(NDEBUG)
    std::cout << "[ ";
    for (int i = 0; i < actual_length; i++) {
      std::cout << std::hex << std::setw(2) << std::setfill('0')
                << (0xFF & (unsigned)buffer[i])
                << std::dec << std::setw(0);
      if (i != actual_length - 1) {
        std::cout << ", ";
      }
    }
    std::cout << " ]" << std::endl;
 #endif

    that->tm = Util::millis();
    const unsigned char *hidPPBuffer = that->hidPPBuffer;
    if (actual_length > 0 && actual_length ==
        getReportLength(buffer[HARMONY_REPORT_ID_IDX])) {
      if (buffer[HARMONY_REPORT_ID_IDX] == HARMONY_REPORT_DJ_SHORT) {
        if (buffer[HARMONY_SUBID_IDX] == HARMONY_SUBID_KEYBOARD ||
            buffer[HARMONY_SUBID_IDX] == HARMONY_SUBID_CONSUMER_CTRL) {
          if (buffer[HARMONY_KEY_MSB_IDX] ||
              buffer[HARMONY_KEY_LSB_IDX]) {
            // Key pressed
            that->key = ((buffer[HARMONY_SUBID_IDX] & 0x3) << 16) |
                         (buffer[HARMONY_KEY_MSB_IDX] << 8) |
                          buffer[HARMONY_KEY_LSB_IDX];
          } else if (that->key) {
            // Key released
            if (that->keyCallback) {
              that->keyCallback(that->key);
            }
            that->key = 0;
          }
        } else if (buffer[HARMONY_SUBID_IDX] == HARMONY_SUBID_CONN_NOTIF) {
          if (buffer[HARMONY_KEY_MSB_IDX]) {
            // Remote was disconnected or maybe lost RF connectivity. Clear
            // any pending depressed keys.
#if !defined(NDEBUG)
            if (that->key) {
              std::cout << "Lost key: " << toString(that->key) << std::endl;
            } else {
              std::cout << "RF connectivity lost" << std::endl;
            }
#endif
            that->key = 0;
          }
        }
      } else if ((buffer[HARMONY_REPORT_ID_IDX] == HARMONY_REPORT_HIDPP_SHORT ||
                  buffer[HARMONY_REPORT_ID_IDX] == HARMONY_REPORT_HIDPP_LONG) &&
                 buffer[HARMONY_DEVICE_IDX] == hidPPBuffer[HARMONY_DEVICE_IDX]&&
                 (that->hidPPCallback || that->hidPPError)) {
        if ((buffer[HARMONY_SUBID_IDX] == HARMONY_SUBID_ERROR ||
             buffer[HARMONY_SUBID_IDX] == HARMONY_SUBID_ERROR2) &&
            buffer[HARMONY_SUBID_IDX + 1] == hidPPBuffer[HARMONY_SUBID_IDX]) {
          // Positively identified report to be an error message for our
          // most recent request
          if (that->hidPPError) {
            that->hidPPError(actual_length, buffer);
          } else if (that->hidPPCallback) {
              that->hidPPCallback(actual_length, buffer);
          }
          goto clearHIDppCallback;
        } else if (buffer[HARMONY_SUBID_IDX] == hidPPBuffer[HARMONY_SUBID_IDX]){
          // Positively identified report to be a response to our most recent
          // request
          if (that->hidPPCallback) {
            that->hidPPCallback(actual_length, buffer);
          }
        clearHIDppCallback:
          memset(that->hidPPBuffer, 0, sizeof(that->hidPPBuffer));
          that->hidPPCallback = NULL;
          that->hidPPError = NULL;
        }
      }
    }
  }
  if (that->event) {
    that->event->runLater([that]() {
      that->setKeyCallback(that->keyCallback);
    });
  } else {
    that->setKeyCallback(that->keyCallback);
  }
}

void Harmony::cancelPendingTransfer() {
  if (!completed) {
    libusb_cancel_transfer(transfer);
    while (!completed) {
      libusb_handle_events_completed(ctx, &completed);
    }
    memset(hidPPBuffer, 0, sizeof(hidPPBuffer));
    hidPPCallback = NULL;
    hidPPError = NULL;
  }
}

void Harmony::handleUsbPollFdEvent() {
  struct timeval zero_tv = { };
  libusb_handle_events_timeout(ctx, &zero_tv);
}

#include <string.h>

#include <iostream>

#include "event.h"
#include "harmony.h"

// Modern (non-working) receiver: 0x24110026
// Old (working) receiver:        0x12030025

// HID++ version: 2.0
// Name: Harmony 20+
// Total number of HID++ 2.0 features: 19
//   0: [0000]     Root
//   1: [0001]     FeatureSet
//   2: [0002]     FeatureInfo
//   3: [0003]     DeviceFwVersion
//   4: [0005]     DeviceName
//   5: [1000]     BatteryStatus
//   6: [1820]  HI unknown
//   7: [1B00]     ReprogControls
//   8: [1B01]     ReprogControlsV2
//   9: [1D4B]     WirelessDeviceStatus
//  10: [1DF0]  H  unknown RemainingPairings
//  11: [1DF3]  H  unknown
//  12: [4100]     Encryption
//  13: [4520]     KeyboardLayout
//  14: [1E00]  H  unknown EnableHiddenFeatures
//  15: [1830]  HI unknown
//  16: [1860]  HI unknown
//  17: [1890]  HI unknown
//  18: [1E90]  HI unknown
//  19: [18B0]  HI unknown

static void readName(Harmony *harmony, int idx) {
  unsigned char buf[16] = "\x10\xFF\x83\xB5\x40\x00\x00";
  buf[4] = 0x40 + idx;
  harmony->sendHIDppRequestAndWait(buf, [idx](
    int len, const unsigned char *buf) {
      unsigned char out[16] = { };
      std::cout << "Device name #" << idx << ": ";
      memcpy(out, buf + 6,
             std::max(0, std::min((int)buf[5],
                                  std::min(len-6, (int)sizeof(out)-1))));
      std::cout << out << std::endl;
    },
    [idx](
    int len, const unsigned char *buf) {
      std::cout << "Failed to get device name #" << idx << ": "
                << buf[6] << std::endl;
    });
}

static void handleHarmonyKey(Event *event, Harmony *harmony, int key) {
  std::cout << std::hex << "KEY => " << key
            << ", " << Harmony::toString(key) << std::endl << std::dec;
  if (event) {
    if (key == Harmony::KEY_LONG_OFF) {
      event->exitLoop();
    }
  }
}

int main() {
#if 1
  Event event;
  Harmony harmony(&event);
  event.runLater([&]() {
    for (int i = 0; i < 6; i++) {
      readName(&harmony, i);
    }
  });
  harmony.setKeyCallback([&event, &harmony](int key) {
    handleHarmonyKey(&event, &harmony, key);
  });
  event.loop();
#else
  Harmony harmony;
  int key;

  for (int i = 0; i < 6; i++) {
    readName(&harmony, i);
  }
  do {
    key = harmony.getKey();
    handleHarmonyKey(NULL, &harmony, key);
  } while (key != Harmony::KEY_LONG_OFF);
#endif

  return 0;
}

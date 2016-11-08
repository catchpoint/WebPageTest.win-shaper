// exe.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "../driver/interface.h"

int main(int argc, char **argv) {
  HANDLE shaper = CreateFile(SHAPER_DOS_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
  if (shaper != INVALID_HANDLE_VALUE) {
    if (argc > 1) {
      DWORD bytesReturned = 0;
      if (!lstrcmpiA(argv[1], "on")) {
        SHAPER_PARAMS settings;
        settings.plr = 0;
        settings.inBps = 5000000;
        settings.outBps = 1000000;
        settings.inLatency = 14000;
        settings.outLatency = 14000;
        settings.inBufferBytes = 150000;
        settings.outBufferBytes = 150000;
        if (DeviceIoControl(shaper, SHAPER_IOCTL_ENABLE, &settings, sizeof(settings), NULL, 0, &bytesReturned, NULL)) {
          printf("Shaper enabled\n");
        } else {
          printf("SHAPER_IOCTL_ENABLE failed: 0x%08X\n", GetLastError());
        }
      } else if (!lstrcmpiA(argv[1], "off")) {
        if (DeviceIoControl(shaper, SHAPER_IOCTL_DISABLE, NULL, 0, NULL, 0, &bytesReturned, NULL))
          printf("Shaper disabled\n");
        else
          printf("SHAPER_IOCTL_DISABLE failed: 0x%08X\n", GetLastError());
      } else {
        printf("Usage: shaper [on|off]\n");
      }
    } else {
      printf("Usage: shaper [on|off]\n");
    }
    CloseHandle(shaper);
  } else {
    printf("Failed to open Traffic Shaper driver\n");
  }
  return 0;
}


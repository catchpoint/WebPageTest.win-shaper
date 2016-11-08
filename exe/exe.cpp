// exe.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "../driver/interface.h"

int main() {
  HANDLE shaper = CreateFile(SHAPER_DOS_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
  if (shaper != INVALID_HANDLE_VALUE) {
    SHAPER_PARAMS settings;
    settings.plr = 0;
    settings.inBps = 5000000;
    settings.outBps = 1000000;
    DWORD bytesReturned = 0;
    if (DeviceIoControl(shaper, SHAPER_IOCTL_ENABLE, &settings, sizeof(settings), NULL, 0, &bytesReturned, NULL)) {
      printf("Shaper enabled\n");
      if (DeviceIoControl(shaper, SHAPER_IOCTL_DISABLE, NULL, 0, NULL, 0, &bytesReturned, NULL)) {
        printf("Shaper disabled\n");
      } else {
        printf("SHAPER_IOCTL_DISABLE failed: 0x%08X\n", GetLastError());
      }
    } else {
      printf("SHAPER_IOCTL_ENABLE failed: 0x%08X\n", GetLastError());
    }
    CloseHandle(shaper);
  } else {
    printf("Failed to open driver\n");
  }
  return 0;
}


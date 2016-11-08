#pragma once

#define SHAPER_DEVICE_NAME     L"\\Device\\TrafficShaper"
#define SHAPER_SYMBOLIC_NAME   L"\\DosDevices\\Global\\TrafficShaper"
#define SHAPER_DOS_NAME        L"\\\\.\\TrafficShaper"

#pragma pack(push, 8)
typedef struct {
  double              plr;
  unsigned __int64    inBps;
  unsigned __int64    outBps;
  unsigned long       rtt;
} SHAPER_PARAMS;
#pragma pack(pop)

#define	SHAPER_IOCTL_DISABLE  CTL_CODE(FILE_DEVICE_NETWORK, 0x801, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define	SHAPER_IOCTL_ENABLE CTL_CODE(FILE_DEVICE_NETWORK, 0x802, METHOD_BUFFERED, FILE_WRITE_ACCESS)

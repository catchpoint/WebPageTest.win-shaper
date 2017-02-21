// exe.cpp : Defines the entry point for the console application.
//

#include "targetver.h"
#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include <Shlwapi.h>

#include "../driver/interface.h"

bool Start() {
  bool ok = false;
  SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS); 
  if (scm) {
    SC_HANDLE service = OpenService(scm, SHAPER_SERVICE_NAME, SERVICE_ALL_ACCESS); 
    if (service) {
      DWORD dwBytesNeeded;
      SERVICE_STATUS_PROCESS status;
      if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(SERVICE_STATUS_PROCESS), &dwBytesNeeded)) {
        if (status.dwCurrentState == SERVICE_STOPPED) {
          if (StartService(service, 0, NULL)) {
            DWORD count = 0;
            do {
              QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(SERVICE_STATUS_PROCESS), &dwBytesNeeded);
              if (status.dwCurrentState == SERVICE_START_PENDING)
                Sleep(100);
              count++;
            } while(status.dwCurrentState == SERVICE_START_PENDING && count < 600);
            if (status.dwCurrentState == SERVICE_RUNNING) {
              ok = true;
            } else {
              printf("Error waiting for service to start\n");
            }
          } else {
            printf("Failed to start the service\n");
          }
        } else {
          ok = true;
        }
      } else {
        printf("Failed to query the current service status\n");
      }
      CloseServiceHandle(service);
    } else {
      printf("Failed to open the shaper service\n");
    }
    CloseServiceHandle(scm);
  } else {
    printf("Failed to open the Service Control Manager\n");
  }
  return ok;
}

bool Install() {
  bool ok = false;
  SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS); 
  if (scm) {
    SC_HANDLE service = OpenService(scm, SHAPER_SERVICE_NAME, SERVICE_ALL_ACCESS); 
    if (!service) {
      TCHAR driver_path[MAX_PATH];
      GetModuleFileName(NULL, driver_path, MAX_PATH);
      BOOL is64bit = FALSE;
      IsWow64Process(GetCurrentProcess(), &is64bit);
      if (is64bit)
        lstrcpy(PathFindFileName(driver_path), _T("shaper64.sys"));
      else
        lstrcpy(PathFindFileName(driver_path), _T("shaper32.sys"));
      service = CreateService(scm,
                              SHAPER_SERVICE_NAME,
                              SHAPER_SERVICE_DISPLAY_NAME,
                              SERVICE_ALL_ACCESS,
                              SERVICE_KERNEL_DRIVER,
                              SERVICE_DEMAND_START,
                              SERVICE_ERROR_NORMAL,
                              driver_path,
                              NULL,
                              NULL,
                              NULL,
                              NULL,
                              NULL);
      if (service) {
        CloseServiceHandle(service);
        ok = Start();
      } else {
        printf("Failed to install shaper driver\n");
      }
    } else {
      CloseServiceHandle(service);
      ok = Start();
    }
    CloseServiceHandle(scm);
  } else {
    printf("Failed to open the Service Control Manager\n");
  }
  return ok;
}


bool Stop(bool silent = false) {
  bool ok = false;
  SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS); 
  if (scm) {
    SC_HANDLE service = OpenService(scm, SHAPER_SERVICE_NAME, SERVICE_ALL_ACCESS); 
    if (service) {
      DWORD dwBytesNeeded;
      SERVICE_STATUS_PROCESS status;
      if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(SERVICE_STATUS_PROCESS), &dwBytesNeeded)) {
        if (status.dwCurrentState == SERVICE_RUNNING) {
          SERVICE_STATUS s;
          if (ControlService(service, SERVICE_CONTROL_STOP, &s)) {
            DWORD count = 0;
            do {
              QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(SERVICE_STATUS_PROCESS), &dwBytesNeeded);
              if (status.dwCurrentState == SERVICE_STOP_PENDING)
                Sleep(100);
              count++;
            } while(status.dwCurrentState == SERVICE_STOP_PENDING && count < 600);
            if (status.dwCurrentState == SERVICE_STOPPED) {
              ok = true;
            } else if (!silent) {
              printf("Error waiting for service to stop\n");
            }
          } else if (!silent) {
            printf("Failed to stop the service\n");
          }
        } else {
          ok = true;
        }
      } else if (!silent) {
        printf("Failed to query the current service status\n");
      }
      CloseServiceHandle(service);
    } else if (!silent) {
      printf("Failed to open the shaper service\n");
    }
    CloseServiceHandle(scm);
  } else if (!silent) {
    printf("Failed to open the Service Control Manager\n");
  }
  return ok;
}

bool Remove(bool silent = false) {
  bool ok = Stop(silent);
  if (ok) {
    ok = false;
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS); 
    if (scm) {
      SC_HANDLE service = OpenService(scm, SHAPER_SERVICE_NAME, SERVICE_ALL_ACCESS); 
      if (service) {
        if (DeleteService(service)) {
          ok = true;
        } else if (!silent) {
          printf("DeleteService failed\n");
        }
        CloseServiceHandle(service);
      } else if (!silent) {
        printf("Failed to open the shaper service\n");
      }
      CloseServiceHandle(scm);
    } else if (!silent) {
      printf("Failed to open the Service Control Manager\n");
    }
  }
  return ok;
}


bool Set(SHAPER_PARAMS &settings) {
  bool ok = false;
  HANDLE shaper = CreateFile(SHAPER_DOS_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
  if (shaper == INVALID_HANDLE_VALUE) {
    Start();
    shaper = CreateFile(SHAPER_DOS_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
  }
  if (shaper != INVALID_HANDLE_VALUE) {
    DWORD bytesReturned = 0;
    if (DeviceIoControl(shaper, SHAPER_IOCTL_ENABLE, &settings, sizeof(settings), NULL, 0, &bytesReturned, NULL)) {
      ok = true;
    } else {
      printf("SHAPER_IOCTL_ENABLE failed: 0x%08X\n", GetLastError());
    }
    CloseHandle(shaper);
  } else {
    printf("Failed to open Traffic Shaper driver\n");
  }
  return ok;
}

bool Reset() {
  bool ok = false;
  HANDLE shaper = CreateFile(SHAPER_DOS_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
  if (shaper != INVALID_HANDLE_VALUE) {
    DWORD bytesReturned = 0;
    if (DeviceIoControl(shaper, SHAPER_IOCTL_DISABLE, NULL, 0, NULL, 0, &bytesReturned, NULL))
      ok = true;
    else
      printf("SHAPER_IOCTL_DISABLE failed: 0x%08X\n", GetLastError());
    CloseHandle(shaper);
  } else {
    printf("Failed to open Traffic Shaper driver\n");
  }
  return ok;
}

void usage() {
  printf("Usage: shaper [install|remove|set|reset] <options>\n"
         "\n"
         "  install: Installs the shaper driver and starts the service.\n"
         "  remove: Stops the service and uninstalls the driver.\n"
         "  set: Enable traffic-shaping with the supplied configuration (see set options below).\n"
         "  reset: Disable traffic-shaping.\n"
         "\n"
         " Options for set command (specified as option=value):\n"
         "  inbps: Inbound bandwidth in bits-per-second.\n"
         "  outbps: Outbound bandwidth in bits-per-second.\n"
         "  rtt: Connection latency in milliseconds (half applied inbound and half applied outbound).\n"
         "  plr: Random packet loss in percent (accurate to 0.01 percent, defaults to 0)"
         "  inbuff: Inbound buffer size in bytes (defaults to 150,000)\n"
         "  outbuff: Outbound buffer size in bytes (defaults to 150,000)\n"
         "\n"
         " Examples:\n"
         "  shaper install\n"
         "  shaper set inbps=5000000 outbps=1000000 rtt=28\n"
         "  shaper reset\n"
         "  shaper remove\n");
}

int main(int argc, char **argv) {
  bool ok = false;
  if (argc > 1) {
    DWORD bytesReturned = 0;
    if (!lstrcmpiA(argv[1], "install")) {
      Remove(true); // uninstall first in case an older version is registered
      ok = Install();
    } else if (!lstrcmpiA(argv[1], "remove")) {
      ok = Remove();
    } else if (!lstrcmpiA(argv[1], "set")) {
      ok = true;
      SHAPER_PARAMS settings;
      settings.plr = 0;
      settings.inBps = 0;
      settings.outBps = 0;
      settings.inLatency = 0;
      settings.outLatency = 0;
      settings.inBufferBytes = 150000;
      settings.outBufferBytes = 150000;
      for (int i = 2; i < argc; i++) {
        char * separator = strchr(argv[i], '=');
        if (separator) {
          separator[0] = NULL;
          const char * option = argv[i];
          const char * value = &separator[1];
          if (!strcmp(option, "inbps")) {
            settings.inBps = _atoi64(value);
          } else if (!strcmp(option, "outbps")) {
            settings.outBps = _atoi64(value);
          } else if (!strcmp(option, "rtt")) {
            unsigned long rtt = atol(value);
            settings.inLatency = rtt / 2;
            settings.outLatency = rtt / 2;
            if (rtt % 2)
              settings.inLatency += 1;
          } else if (!strcmp(option, "plr")) {
            settings.plr = (unsigned short)(atof(value) * 100.0);
          } else if (!strcmp(option, "inbuff")) {
            settings.inBufferBytes = _atoi64(value);
          } else if (!strcmp(option, "outbuff")) {
            settings.outBufferBytes = _atoi64(value);
          } else {
            ok = false;
            printf("Unrecognized option: %s\n", option);
          }
        } else {
          ok = false;
          printf("Incorrect option: %s\n", argv[i]);
        }
      }
      if (ok) {
        ok = Set(settings);
      }
    } else if (!lstrcmpiA(argv[1], "reset")) {
      ok = Reset();
    }
  }
  if (!ok) {
    usage();
  }
  return ok ? 0 : 1;
}


// exe.cpp : Defines the entry point for the console application.
//

#include "targetver.h"
#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include <Shlwapi.h>

#include "../driver/interface.h"

void Start() {
  SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS); 
  if (scm) {
    SC_HANDLE service = OpenService(scm, SHAPER_SERVICE_NAME, SERVICE_ALL_ACCESS); 
    if (service) {
      DWORD dwBytesNeeded;
      SERVICE_STATUS_PROCESS status;
      if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(SERVICE_STATUS_PROCESS), &dwBytesNeeded)) {
        if (status.dwCurrentState == SERVICE_STOPPED) {
          if (StartService(service, 0, NULL)) {
            printf("Service starting...\n");
            DWORD count = 0;
            do {
              QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(SERVICE_STATUS_PROCESS), &dwBytesNeeded);
              if (status.dwCurrentState == SERVICE_START_PENDING)
                Sleep(100);
              count++;
            } while(status.dwCurrentState == SERVICE_START_PENDING && count < 600);
            if (status.dwCurrentState == SERVICE_RUNNING)
              printf("Service Started\n");
            else
              printf("Error waiting for service to start\n");
          } else {
            printf("Failed to start the service\n");
          }
        } else {
          printf("Service is already running\n");
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
}

void Stop() {
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
            printf("Service stopping...\n");
            DWORD count = 0;
            do {
              QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(SERVICE_STATUS_PROCESS), &dwBytesNeeded);
              if (status.dwCurrentState == SERVICE_STOP_PENDING)
                Sleep(100);
              count++;
            } while(status.dwCurrentState == SERVICE_STOP_PENDING && count < 600);
            if (status.dwCurrentState == SERVICE_STOPPED)
              printf("Service Stopped\n");
            else
              printf("Error waiting for service to stop\n");
          } else {
            printf("Failed to stop the service\n");
          }
        } else {
          printf("Service is already stopped\n");
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
}

void Enable() {
  HANDLE shaper = CreateFile(SHAPER_DOS_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
  if (shaper == INVALID_HANDLE_VALUE) {
    Start();
    shaper = CreateFile(SHAPER_DOS_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
  }
  if (shaper != INVALID_HANDLE_VALUE) {
    DWORD bytesReturned = 0;
    SHAPER_PARAMS settings;
    settings.plr = 0;
    settings.inBps = 5000000;
    settings.outBps = 1000000;
    settings.inLatency = 14;
    settings.outLatency = 14;
    settings.inBufferBytes = 150000;
    settings.outBufferBytes = 150000;
    if (DeviceIoControl(shaper, SHAPER_IOCTL_ENABLE, &settings, sizeof(settings), NULL, 0, &bytesReturned, NULL)) {
      printf("Shaper enabled\n");
    } else {
      printf("SHAPER_IOCTL_ENABLE failed: 0x%08X\n", GetLastError());
    }
    CloseHandle(shaper);
  } else {
    printf("Failed to open Traffic Shaper driver\n");
  }
}

void Disable() {
  HANDLE shaper = CreateFile(SHAPER_DOS_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
  if (shaper != INVALID_HANDLE_VALUE) {
    DWORD bytesReturned = 0;
    if (DeviceIoControl(shaper, SHAPER_IOCTL_DISABLE, NULL, 0, NULL, 0, &bytesReturned, NULL))
      printf("Shaper disabled\n");
    else
      printf("SHAPER_IOCTL_DISABLE failed: 0x%08X\n", GetLastError());
    CloseHandle(shaper);
  } else {
    printf("Failed to open Traffic Shaper driver\n");
  }
}

void Install() {
  SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS); 
  if (scm) {
    SC_HANDLE service = OpenService(scm, SHAPER_SERVICE_NAME, SERVICE_ALL_ACCESS); 
    if (!service) {
      TCHAR driver_path[MAX_PATH];
      GetModuleFileName(NULL, driver_path, MAX_PATH);
      lstrcpy(PathFindFileName(driver_path), _T("shaper.sys"));
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
        printf("Shaper driver installed\n");
      } else {
        printf("Failed to install shaper driver\n");
      }
    } else {
      CloseServiceHandle(service);
      printf("Shaper is already installed\n");
    }
    CloseServiceHandle(scm);
  } else {
    printf("Failed to open the Service Control Manager\n");
  }
}

void Uninstall() {
  Stop();
  SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS); 
  if (scm) {
    SC_HANDLE service = OpenService(scm, SHAPER_SERVICE_NAME, SERVICE_ALL_ACCESS); 
    if (service) {
      if (DeleteService(service)) {
        printf("Driver uninstalled\n");
      } else {
        printf("DeleteService failed\n");
      }
      CloseServiceHandle(service);
    } else {
      printf("Failed to open the shaper service\n");
    }
    CloseServiceHandle(scm);
  } else {
    printf("Failed to open the Service Control Manager\n");
  }
}

int main(int argc, char **argv) {
  if (argc > 1) {
    DWORD bytesReturned = 0;
    if (!lstrcmpiA(argv[1], "start"))
      Start();
    else if (!lstrcmpiA(argv[1], "stop"))
      Stop();
    else if (!lstrcmpiA(argv[1], "on"))
      Enable();
    else if (!lstrcmpiA(argv[1], "off"))
      Disable();
    else if (!lstrcmpiA(argv[1], "install"))
      Install();
    else if (!lstrcmpiA(argv[1], "uninstall"))
      Uninstall();
    else
      printf("Usage: shaper [install|uninstall|start|stop|on|off]\n");
  } else {
    printf("Usage: shaper [install|uninstall|start|stop|on|off]\n");
  }
  return 0;
}


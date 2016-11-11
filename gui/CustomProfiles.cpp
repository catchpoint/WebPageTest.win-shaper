#include "stdafx.h"
#include "CustomProfiles.h"


/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
CustomProfiles::CustomProfiles() {
  PWSTR path;
  if (SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, NULL, &path) == S_OK) {
    CString dir(path);
    CoTaskMemFree(path);
    dir += L"\\winShaper";
    CreateDirectory(dir, NULL);
    data_file_ = dir + L"profiles.txt";
  }
  Load();
}


/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
CustomProfiles::~CustomProfiles() {
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
void CustomProfiles::Save() {
  CString out = L"";
  int count = (int)profiles_.GetCount();
  for (int i = 0; i < count; i++) {
    CString profile = profiles_[i].Serialize();
    if (!profile.IsEmpty())
      out += profile + L"\n";
  }
  if (out.GetLength()) {
    HANDLE hFile = CreateFile(data_file_, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (hFile != INVALID_HANDLE_VALUE) {
      DWORD written = 0;
      DWORD len = out.GetLength() * sizeof(TCHAR);
      WriteFile(hFile, (LPCTSTR)out, len, &written, 0);
      CloseHandle(hFile);
    }
  }
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
void CustomProfiles::Load() {
  if (!profiles_.IsEmpty())
    profiles_.RemoveAll();
  HANDLE hFile = CreateFile(data_file_, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
  if (hFile != INVALID_HANDLE_VALUE) {
    DWORD len = GetFileSize(hFile, NULL);
    if (len > 0) {
      DWORD buff_len = len + sizeof(TCHAR);
      TCHAR * buff = (TCHAR *)malloc(buff_len);
      if (buff) {
        memset(buff, buff_len, 0);
        DWORD bytes_read;
        if (ReadFile(hFile, buff, len, &bytes_read, 0) && len == bytes_read) {
          CString in(buff);
          int pos = 0;
          CString line = in.Tokenize(L"\n", pos);
          while (line != L"") {
            ConnectionProfile profile(line);
            if (!profile.name_.IsEmpty())
              profiles_.Add(profile);
            line = in.Tokenize(L"\n", pos);
          }
        }
        free(buff);
      }
    }
    CloseHandle(hFile);
  }
}

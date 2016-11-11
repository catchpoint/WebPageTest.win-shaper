#pragma once
class ConnectionProfile {
public:
  ConnectionProfile(){}
  ConnectionProfile(CString          name,
                    unsigned __int64 inBps,
                    unsigned __int64 outBps,
                    unsigned long    rtt,
                    unsigned short   plr,
                    unsigned __int64 inBufferLen,
                    unsigned __int64 outBufferLen){
    name_ = name;
    inBps_ = inBps;
    outBps_ = outBps;
    rtt_ = rtt;
    plr_ = plr;
    inBufferLen_ = inBufferLen;
    outBufferLen_ = outBufferLen;
  }
  ConnectionProfile(const ConnectionProfile &src){*this = src;}
  ConnectionProfile(CString serialized) {
    CStringArray parts;
    int pos = 0;
    CString token = serialized.Tokenize(L"\t", pos);
    while (token != L"") {
      parts.Add(token);
      token = serialized.Tokenize(L"\t", pos);
    }
    if (parts.GetCount() >= 7) {
      name_ = parts[0];
      inBps_ = _ttoi64(parts[1]);
      outBps_ = _ttoi64(parts[2]);
      rtt_ = _ttol(parts[3]);
      plr_ = _ttoi(parts[4]);
      inBufferLen_ = _ttoi64(parts[5]);
      outBufferLen_ = _ttoi64(parts[6]);
    }
  }
  ~ConnectionProfile(){}
  const ConnectionProfile &operator =(const ConnectionProfile &src) {
    name_ = src.name_;
    inBps_ = src.inBps_;
    outBps_ = src.outBps_;
    rtt_ = src.rtt_;
    plr_ = src.plr_;
    inBufferLen_ = src.inBufferLen_;
    outBufferLen_ = src.outBufferLen_;
    return src;
  }
  CString DisplayString() {
    // Format the parameters into a string that includes the name and bandwidth settings
    CString display, buff;
    display = name_ + L" (";
    if (inBps_ >= 1000000) {
      if (inBps_ % 1000000) {
        buff.Format(L"%0.1f Mbps", (double)inBps_ / 1000000.0);
      } else {
        buff.Format(L"%d Mbps", (DWORD)(inBps_ / 1000000));
      }
    } else if (inBps_ >= 1000) {
      if (inBps_ % 1000) {
        buff.Format(L"%0.1f Kbps", (double)inBps_ / 1000.0);
      } else {
        buff.Format(L"%d Kbps", (DWORD)(inBps_ / 1000));
      }
    } else {
      buff.Format(L"%d bps", (DWORD)inBps_);
    }
    display += buff + L"/";
    if (outBps_ >= 1000000) {
      if (outBps_ % 1000000) {
        buff.Format(L"%0.1f Mbps", (double)outBps_ / 1000000.0);
      } else {
        buff.Format(L"%d Mbps", (DWORD)(outBps_ / 1000000));
      }
    } else if (outBps_ >= 1000) {
      if (outBps_ % 1000) {
        buff.Format(L"%0.1f Kbps", (double)outBps_ / 1000.0);
      } else {
        buff.Format(L"%d Kbps", (DWORD)(outBps_ / 1000));
      }
    } else {
      buff.Format(L"%d bps", (DWORD)outBps_);
    }
    display += buff + L" ";
    buff.Format(L"%dms RTT", rtt_);
    display += buff;
    if (plr_ > 0) {
      buff.Format(L", %0.2f%% plr", (double)plr_ / 100.0);
      display += buff;
    }
    display += ")";
    return display;
  }
  CString Serialize() {
    CString buff;
    if (name_.GetLength() && name_.Find(L"\t") == -1)
      buff.Format(L"%s\t%I64u\t%I64u\t%lu\t%u\t%I64u\t%I64u", (LPCTSTR)name_, inBps_, outBps_, rtt_, plr_, inBufferLen_, outBufferLen_);
    return buff;
  }

  CString          name_;
  unsigned __int64 inBps_;
  unsigned __int64 outBps_;
  unsigned long    rtt_;
  unsigned short   plr_; // 0 - 10000 (hundreths)
  unsigned __int64 inBufferLen_;
  unsigned __int64 outBufferLen_;
};


class CustomProfiles {
public:
  CustomProfiles();
  ~CustomProfiles();
  void Load();
  void Save();
  CArray<ConnectionProfile> profiles_;

protected:
  CString data_file_;
};


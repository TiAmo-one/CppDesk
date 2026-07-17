#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace clipboard {

struct ClipboardData {
    std::wstring text;
    std::vector<std::wstring> filePaths;
    bool hasText  = false;
    bool hasFiles = false;
};

class Monitor {
public:
    bool Check(ClipboardData& data);
    static bool Write(const ClipboardData& data);
    void SetIgnoreNext() { ignoreNext_ = true; }
private:
    DWORD lastSeq_ = 0;
    bool  ignoreNext_ = false;
};

} // namespace clipboard

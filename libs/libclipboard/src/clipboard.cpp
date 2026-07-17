#include "libclipboard.h"

namespace clipboard {

bool Monitor::Check(ClipboardData& data) {
    if (ignoreNext_) { ignoreNext_ = false; lastSeq_ = GetClipboardSequenceNumber(); return false; }
    DWORD seq = GetClipboardSequenceNumber();
    if (seq == lastSeq_) return false;
    lastSeq_ = seq;

    data = {};
    if (!OpenClipboard(nullptr)) return false;

    HANDLE hText = GetClipboardData(CF_UNICODETEXT);
    if (hText) {
        auto* txt = (wchar_t*)GlobalLock(hText);
        if (txt) { data.text = txt; data.hasText = true; GlobalUnlock(hText); }
    }

    HANDLE hDrop = GetClipboardData(CF_HDROP);
    if (hDrop) {
        auto* drop = (HDROP)GlobalLock(hDrop);
        if (drop) {
            UINT count = DragQueryFile(drop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < count; i++) {
                wchar_t path[MAX_PATH];
                if (DragQueryFile(drop, i, path, MAX_PATH) > 0)
                    data.filePaths.push_back(path);
            }
            data.hasFiles = !data.filePaths.empty();
            GlobalUnlock(hDrop);
        }
    }
    CloseClipboard();
    return data.hasText || data.hasFiles;
}

bool Monitor::Write(const ClipboardData& data) {
    if (!OpenClipboard(nullptr)) return false;
    EmptyClipboard();

    if (data.hasText) {
        size_t sz = (data.text.length() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, sz);
        if (hMem) {
            memcpy(GlobalLock(hMem), data.text.c_str(), sz);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
    }

    if (data.hasFiles) {
        size_t totalLen = sizeof(DROPFILES);
        for (auto& f : data.filePaths) totalLen += (f.length() + 1) * sizeof(wchar_t);
        totalLen += sizeof(wchar_t);

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, totalLen);
        if (hMem) {
            auto* df = (DROPFILES*)GlobalLock(hMem);
            df->pFiles = sizeof(DROPFILES);
            df->fWide  = TRUE;
            wchar_t* dst = (wchar_t*)((uint8_t*)df + sizeof(DROPFILES));
            for (auto& f : data.filePaths) {
                wcscpy(dst, f.c_str());
                dst += f.length() + 1;
            }
            *dst = 0;
            GlobalUnlock(hMem);
            SetClipboardData(CF_HDROP, hMem);
        }
    }
    CloseClipboard();
    return true;
}

} // namespace clipboard

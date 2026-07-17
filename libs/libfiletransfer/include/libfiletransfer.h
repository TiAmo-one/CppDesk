#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <functional>

namespace filetransfer {

constexpr size_t BLOCK_SIZE   = 60 * 1024;
constexpr int    WINDOW_SIZE  = 32;

struct FileMeta {
    uint32_t transferId;
    std::wstring fileName;
    uint64_t fileSize;
    uint32_t totalBlocks;
};

class FileSender {
public:
    uint32_t StartSend(const std::wstring& filePath);
    std::vector<std::vector<uint8_t>> GetNextBlocks(int maxBlocks);
    void OnAck(uint32_t transferId, uint32_t blockIndex);
    bool IsDone(uint32_t transferId) const;
    const FileMeta* GetMeta(uint32_t transferId) const;

private:
    struct Transfer {
        std::ifstream file;
        FileMeta meta;
        uint32_t nextBlock = 0;
        uint32_t ackedUpTo = 0;
        std::vector<bool> acked;
    };
    std::vector<Transfer> transfers_;
    uint32_t nextId_ = 1;
};

class FileReceiver {
public:
    void StartReceive(const FileMeta& meta, const std::wstring& savePath);
    void OnBlock(uint32_t transferId, uint32_t blockIndex,
                 const uint8_t* data, uint32_t len);
    std::vector<std::pair<uint32_t, uint32_t>> GetPendingAcks();
    bool IsDone(uint32_t transferId) const;
    const std::wstring& GetSavedPath(uint32_t transferId) const;

private:
    struct ReceiveTransfer {
        std::ofstream file;
        FileMeta meta;
        std::wstring savePath;
        std::vector<bool> received;
        uint32_t blocksReceived = 0;
    };
    std::vector<ReceiveTransfer> transfers_;
    std::vector<std::pair<uint32_t, uint32_t>> pendingAcks_;
};

} // namespace filetransfer

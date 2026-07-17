#include "libfiletransfer.h"

namespace filetransfer {

uint32_t FileSender::StartSend(const std::wstring& filePath) {
    Transfer t;
    t.file.open(filePath, std::ios::binary);
    if (!t.file.is_open()) return 0;

    t.file.seekg(0, std::ios::end);
    t.meta.fileSize = t.file.tellg();
    t.file.seekg(0, std::ios::beg);

    t.meta.transferId  = nextId_++;
    t.meta.fileName    = filePath.substr(filePath.find_last_of(L"\\") + 1);
    t.meta.totalBlocks = (uint32_t)((t.meta.fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE);
    t.acked.resize(t.meta.totalBlocks, false);

    transfers_.push_back(std::move(t));
    return transfers_.back().meta.transferId;
}

std::vector<std::vector<uint8_t>> FileSender::GetNextBlocks(int maxBlocks) {
    std::vector<std::vector<uint8_t>> blocks;
    if (transfers_.empty()) return blocks;
    auto& t = transfers_.back();

    int sent = 0;
    while (t.nextBlock < t.meta.totalBlocks &&
           (t.nextBlock - t.ackedUpTo) < WINDOW_SIZE && sent < maxBlocks) {
        size_t offset = (size_t)t.nextBlock * BLOCK_SIZE;
        size_t remain = t.meta.fileSize - offset;
        size_t blockSize = (remain < BLOCK_SIZE) ? remain : BLOCK_SIZE;

        std::vector<uint8_t> data(blockSize);
        t.file.seekg(offset);
        t.file.read((char*)data.data(), blockSize);
        data.resize(t.file.gcount());

        blocks.push_back(std::move(data));
        t.nextBlock++; sent++;
    }
    return blocks;
}

void FileSender::OnAck(uint32_t transferId, uint32_t blockIndex) {
    for (auto& t : transfers_) {
        if (t.meta.transferId == transferId && blockIndex < t.acked.size()) {
            t.acked[blockIndex] = true;
            while (t.ackedUpTo < t.meta.totalBlocks && t.acked[t.ackedUpTo])
                t.ackedUpTo++;
            return;
        }
    }
}

bool FileSender::IsDone(uint32_t transferId) const {
    for (auto& t : transfers_)
        if (t.meta.transferId == transferId) return t.ackedUpTo >= t.meta.totalBlocks;
    return false;
}

const FileMeta* FileSender::GetMeta(uint32_t transferId) const {
    for (auto& t : transfers_)
        if (t.meta.transferId == transferId) return &t.meta;
    return nullptr;
}

// --- FileReceiver ---

void FileReceiver::StartReceive(const FileMeta& meta, const std::wstring& savePath) {
    ReceiveTransfer rt;
    rt.meta = meta; rt.savePath = savePath;
    rt.file.open(savePath, std::ios::binary);
    rt.received.resize(meta.totalBlocks, false);
    transfers_.push_back(std::move(rt));
}

void FileReceiver::OnBlock(uint32_t transferId, uint32_t blockIndex,
                            const uint8_t* data, uint32_t len) {
    for (auto& rt : transfers_) {
        if (rt.meta.transferId != transferId) continue;
        if (blockIndex >= rt.received.size() || rt.received[blockIndex]) return;

        size_t offset = (size_t)blockIndex * BLOCK_SIZE;
        rt.file.seekp(offset);
        rt.file.write((const char*)data, len);
        rt.file.flush();
        rt.received[blockIndex] = true;
        rt.blocksReceived++;
        pendingAcks_.push_back({transferId, blockIndex});
        return;
    }
}

std::vector<std::pair<uint32_t, uint32_t>> FileReceiver::GetPendingAcks() {
    auto acks = std::move(pendingAcks_);
    pendingAcks_.clear();
    return acks;
}

bool FileReceiver::IsDone(uint32_t transferId) const {
    for (auto& rt : transfers_)
        if (rt.meta.transferId == transferId) return rt.blocksReceived >= rt.meta.totalBlocks;
    return false;
}

const std::wstring& FileReceiver::GetSavedPath(uint32_t transferId) const {
    static std::wstring empty;
    for (auto& rt : transfers_)
        if (rt.meta.transferId == transferId) return rt.savePath;
    return empty;
}

} // namespace filetransfer

#pragma once
#include <cstdint>
#include <stdint.h>
#include <x264.h>

namespace encode {

struct EncodedPacket {
    uint8_t* data;
    int      size;
    bool     isKeyFrame;
    int64_t  pts;
};

class Encoder {
public:
    Encoder();
    ~Encoder();

    bool Init(int width, int height, int fps = 30, int bitrateKbps = 2000);
    EncodedPacket Encode(const uint8_t* bgra, int stride, int64_t pts);
    void RequestKeyFrame();
    void SetBitrate(int bitrateKbps);

    int width()  const { return width_; }
    int height() const { return height_; }

private:
    x264_t*        enc_ = nullptr;
    x264_param_t   param_;
    x264_picture_t picIn_;
    x264_picture_t picOut_;
    int width_  = 0;
    int height_ = 0;
    int frameCount_ = 0;
};

} // namespace encode

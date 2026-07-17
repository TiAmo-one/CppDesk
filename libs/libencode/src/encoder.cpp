#include "libencode.h"
#include <cstring>

namespace encode {

Encoder::Encoder() { x264_param_default(&param_); }

Encoder::~Encoder() {
    if (enc_) {
        x264_encoder_close(enc_);
        enc_ = nullptr;
    }
    if (picIn_.img.plane[0]) {
        x264_picture_clean(&picIn_);
    }
}

bool Encoder::Init(int width, int height, int fps, int bitrateKbps) {
    width_  = width;
    height_ = height;

    param_.i_width    = width;
    param_.i_height   = height;
    param_.i_fps_num  = fps;
    param_.i_fps_den  = 1;
    param_.i_bitrate  = bitrateKbps;
    param_.rc.i_rc_method = X264_RC_ABR;

    // Zero latency tuning
    param_.i_bframe          = 0;
    param_.i_threads         = 4;
    param_.i_sync_lookahead  = 0;
    param_.rc.i_lookahead    = 0;
    param_.b_vfr_input       = 0;
    param_.b_repeat_headers  = 1;
    param_.b_annexb          = 1;
    param_.i_keyint_max      = fps * 2; // keyframe every 2s

    x264_param_apply_profile(&param_, "high");
    x264_param_apply_preset(&param_, "ultrafast");
    x264_param_apply_tune(&param_, "zerolatency");

    enc_ = x264_encoder_open(&param_);
    if (!enc_) return false;

    x264_picture_init(&picIn_);
    x264_picture_alloc(&picIn_, X264_CSP_I420, width, height);

    frameCount_ = 0;
    return true;
}

// BGRA → I420 (BT.601)
static void BGRAToI420(const uint8_t* bgra, int width, int height, int stride,
                        x264_picture_t* pic) {
    uint8_t* y  = pic->img.plane[0];
    uint8_t* u  = pic->img.plane[1];
    uint8_t* v  = pic->img.plane[2];
    int yStride  = pic->img.i_stride[0];
    int uvStride = pic->img.i_stride[1];

    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            int srcIdx = row * stride + col * 4;
            uint8_t r = bgra[srcIdx + 2];
            uint8_t g = bgra[srcIdx + 1];
            uint8_t b = bgra[srcIdx + 0];

            int yy = (( 66 * r + 129 * g +  25 * b + 128) >> 8) + 16;
            y[row * yStride + col] = (uint8_t)(yy < 0 ? 0 : (yy > 255 ? 255 : yy));

            if (row % 2 == 0 && col % 2 == 0) {
                int uu = ((-38 * r -  74 * g + 112 * b + 128) >> 8) + 128;
                int vv = ((112 * r -  94 * g -  18 * b + 128) >> 8) + 128;
                int uvRow = row / 2, uvCol = col / 2;
                u[uvRow * uvStride + uvCol] = (uint8_t)(uu < 0 ? 0 : (uu > 255 ? 255 : uu));
                v[uvRow * uvStride + uvCol] = (uint8_t)(vv < 0 ? 0 : (vv > 255 ? 255 : vv));
            }
        }
    }
}

EncodedPacket Encoder::Encode(const uint8_t* bgra, int stride, int64_t pts) {
    EncodedPacket result = {};
    if (!enc_) return result;

    BGRAToI420(bgra, width_, height_, stride, &picIn_);
    picIn_.i_pts  = pts;
    picIn_.i_type = X264_TYPE_AUTO;

    x264_nal_t* nals;
    int nalCount;
    int frameSize = x264_encoder_encode(enc_, &nals, &nalCount, &picIn_, &picOut_);

    if (frameSize > 0) {
        result.data = new uint8_t[frameSize];
        uint8_t* dst = result.data;
        for (int i = 0; i < nalCount; i++) {
            memcpy(dst, nals[i].p_payload, nals[i].i_payload);
            dst += nals[i].i_payload;
        }
        result.size       = frameSize;
        result.isKeyFrame = (picOut_.b_keyframe != 0);
        result.pts        = picOut_.i_pts;
    }

    frameCount_++;
    return result;
}

void Encoder::RequestKeyFrame() {
    if (enc_) picIn_.i_type = X264_TYPE_IDR;
}

void Encoder::SetBitrate(int bitrateKbps) {
    if (enc_) {
        param_.i_bitrate = bitrateKbps;
        x264_encoder_reconfig(enc_, &param_);
    }
}

} // namespace encode

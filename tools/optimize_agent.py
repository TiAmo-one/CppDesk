"""Optimize agent.cpp for low-latency streaming."""

p = r"C:\Users\17410\Desktop\remote control\apps\agent\src\agent.cpp"
with open(p, "r", encoding="utf-8", errors="replace") as f:
    c = f.read()

# 1. Remove 200us pacing sleep (every 10 frags = 4.4ms/frame)
old_pace = '''                    offset += chunkSize;
                    if (fragIdx % 10 == 0)
                        std::this_thread::sleep_for(std::chrono::microseconds(200));'''
new_pace = '''                    offset += chunkSize;'''
c = c.replace(old_pace, new_pace)
print("1. Removed pacing sleep")

# 2. Reduce retries: 50 x 100us -> 3 x yield
old_retry = '''                    for (int retry = 0; retry < 50 && !sent && running_; retry++) {
                        sent = channel_.SendFrame(proto::FrameType::Video, sendBuf_.data(),
                                                   (uint16_t)(sizeof(proto::VideoPayload) + chunkSize));
                        if (!sent) {
                            sendFails++;
                            std::this_thread::sleep_for(std::chrono::microseconds(100));
                        } else {
                            sendSuccess++;
                        }
                    }'''
new_retry = '''                    for (int retry = 0; retry < 3 && !sent && running_; retry++) {
                        sent = channel_.SendFrame(proto::FrameType::Video, sendBuf_.data(),
                                                   (uint16_t)(sizeof(proto::VideoPayload) + chunkSize));
                        if (!sent) {
                            sendFails++;
                            std::this_thread::yield();
                        } else {
                            sendSuccess++;
                        }
                    }'''
c = c.replace(old_retry, new_retry)
print("2. Reduced retries: 50x100us -> 3xyield")

# 3. Replace 14.7MB memcmp with sparse fingerprint
old_delta = '''        if (!cachedFrame_.empty() && running_) {
            if (cachedWidth_ == prevWidth_ && cachedHeight_ == prevHeight_ &&
                cachedFrame_.size() == prevFrame_.size() &&
                memcmp(cachedFrame_.data(), prevFrame_.data(), cachedFrame_.size()) == 0) {
                // unchanged
            } else {
                prevFrame_ = cachedFrame_;
                prevWidth_ = cachedWidth_;
                prevHeight_ = cachedHeight_;'''
new_delta = '''        if (!cachedFrame_.empty() && running_) {
            bool changed = true;
            if (cachedWidth_ == prevWidth_ && cachedHeight_ == prevHeight_ &&
                cachedFrame_.size() == prevFrame_.size() && !prevFrame_.empty()) {
                const uint8_t* cur = cachedFrame_.data();
                const uint8_t* prv = prevFrame_.data();
                size_t sz = cachedFrame_.size();
                changed = false;
                for (size_t i = 0; i < sz; i += 1024) {
                    if (*(const uint32_t*)(cur + i) != *(const uint32_t*)(prv + i)) {
                        changed = true;
                        break;
                    }
                }
            }
            if (changed) {
                if (prevFrame_.size() != (cachedFrame_.size() + 1023) / 1024 * 4)
                    prevFrame_.resize((cachedFrame_.size() + 1023) / 1024 * 4);
                uint32_t* fp = (uint32_t*)prevFrame_.data();
                const uint8_t* src = cachedFrame_.data();
                size_t sz = cachedFrame_.size();
                for (size_t i = 0; i < sz; i += 1024)
                    fp[i / 1024] = *(const uint32_t*)(src + i);
                prevWidth_ = cachedWidth_;
                prevHeight_ = cachedHeight_;'''
c = c.replace(old_delta, new_delta)
print("3. Replaced 14.7MB memcmp with sparse fingerprint (256x smaller)")

with open(p, "w", encoding="utf-8") as f:
    f.write(c)
print("Agent optimization complete.")
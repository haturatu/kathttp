#ifndef KATHTTP3_HANDSHAKE_STREAM_BUFFER_H
#define KATHTTP3_HANDSHAKE_STREAM_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace kathttp3 {

struct BufferedHandshakeStreamData {
    uint32_t flags = 0;
    int64_t stream_id = -1;
    uint64_t offset = 0;
    std::vector<uint8_t> data;
};

// Bounds stream callbacks received after a Happy Eyeballs candidate has
// installed 1-RTT keys but before its HTTP/3 codec can be constructed.
class HandshakeStreamBuffer {
   public:
    static constexpr size_t kMaxBytes = 1024 * 1024;
    static constexpr size_t kMaxEvents = 1024;

    bool append(uint32_t flags, int64_t stream_id, uint64_t offset, const uint8_t* data,
                size_t length) {
        if ((length != 0 && !data) || events_.size() >= kMaxEvents ||
            length > kMaxBytes - buffered_bytes_) {
            return false;
        }
        BufferedHandshakeStreamData event{flags, stream_id, offset, {}};
        if (length != 0) event.data.assign(data, data + length);
        buffered_bytes_ += length;
        events_.push_back(std::move(event));
        return true;
    }

    const std::vector<BufferedHandshakeStreamData>& events() const {
        return events_;
    }

    size_t buffered_bytes() const {
        return buffered_bytes_;
    }

    void clear() {
        events_.clear();
        buffered_bytes_ = 0;
    }

   private:
    std::vector<BufferedHandshakeStreamData> events_;
    size_t buffered_bytes_ = 0;
};

}  // namespace kathttp3

#endif  // KATHTTP3_HANDSHAKE_STREAM_BUFFER_H

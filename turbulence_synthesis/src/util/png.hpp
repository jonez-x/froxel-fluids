// Minimal 8-bit grayscale / RGB PNG writer (stored deflate blocks, no compression). No external dependency.
#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace ff::png {

namespace detail {
inline uint32_t crc32(const uint8_t* data, size_t n, uint32_t crc = 0xFFFFFFFFu) {
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    for (size_t i = 0; i < n; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}
inline void put32(std::vector<uint8_t>& v, uint32_t x) {
    for (int s = 24; s >= 0; s -= 8) v.push_back(uint8_t(x >> s));
}
inline void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    put32(out, uint32_t(data.size()));
    std::vector<uint8_t> td(type, type + 4);
    td.insert(td.end(), data.begin(), data.end());
    out.insert(out.end(), td.begin(), td.end());
    put32(out, crc32(td.data(), td.size()) ^ 0xFFFFFFFFu);
}
}  // namespace detail

// channels: 1 (gray) or 3 (RGB). pixels: width*height*channels bytes, row-major, top row first.
inline bool write(const std::string& path, int width, int height, int channels, const std::vector<uint8_t>& pixels) {
    const size_t rowBytes = size_t(width) * channels;
    std::vector<uint8_t> raw;  // filter byte 0 + row
    raw.reserve(size_t(height) * (rowBytes + 1));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), pixels.begin() + size_t(y) * rowBytes, pixels.begin() + size_t(y + 1) * rowBytes);
    }
    std::vector<uint8_t> z = {0x78, 0x01};  // zlib header
    uint32_t a = 1, b = 0;                  // Adler-32
    for (uint8_t c : raw) {
        a = (a + c) % 65521;
        b = (b + a) % 65521;
    }
    for (size_t pos = 0; pos < raw.size() || pos == 0;) {
        const size_t len = std::min<size_t>(65535, raw.size() - pos);
        const bool last = pos + len >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(uint8_t(len & 0xFF));
        z.push_back(uint8_t(len >> 8));
        z.push_back(uint8_t(~len & 0xFF));
        z.push_back(uint8_t((~len >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + len);
        pos += len;
        if (last) break;
    }
    detail::put32(z, (b << 16) | a);

    std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<uint8_t> ihdr;
    detail::put32(ihdr, uint32_t(width));
    detail::put32(ihdr, uint32_t(height));
    // 8-bit, colour type 0 (gray) or 2 (RGB), deflate, no filter, no interlace
    ihdr.insert(ihdr.end(), {8, uint8_t(channels == 3 ? 2 : 0), 0, 0, 0});
    detail::chunk(out, "IHDR", ihdr);
    detail::chunk(out, "IDAT", z);
    detail::chunk(out, "IEND", {});
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(out.data()), std::streamsize(out.size()));
    return bool(f);
}

// pixels: width*height bytes, row-major, top row first. Returns false on write error.
inline bool writeGray(const std::string& path, int width, int height, const std::vector<uint8_t>& pixels) {
    return write(path, width, height, 1, pixels);
}

// pixels: width*height*3 bytes (R, G, B), row-major, top row first. Returns false on write error.
inline bool writeRGB(const std::string& path, int width, int height, const std::vector<uint8_t>& pixels) {
    return write(path, width, height, 3, pixels);
}

}  // namespace ff::png

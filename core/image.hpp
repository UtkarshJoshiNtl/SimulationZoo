#pragma once
// Minimal image output.
//
// Exhibits resolve a Framebuffer to 8-bit RGB and write a binary PPM (P6). PPM
// needs no external library and is lossless, so `tools/make_gif.py` can read it
// with Pillow and assemble the final animation.

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace zoo {

inline bool write_ppm(const std::string& path, int width, int height,
                      const std::vector<uint8_t>& rgb) {
    if (rgb.size() != static_cast<size_t>(width) * height * 3) return false;
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << width << " " << height << "\n255\n";
    out.write(reinterpret_cast<const char*>(rgb.data()),
              static_cast<std::streamsize>(rgb.size()));
    return static_cast<bool>(out);
}

} // namespace zoo

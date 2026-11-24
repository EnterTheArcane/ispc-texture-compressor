#include "ispc_texcomp.h"

#include <array>
#include <cinttypes>
#include <cstdio>

namespace
{
    uint64_t checksum(const uint8_t* data, int size)
    {
        uint64_t hash = 14695981039346656037ULL;
        for (int i = 0; i < size; ++i)
        {
            hash ^= data[i];
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    bool check(const char* name, const uint8_t* data, int size, uint64_t expected)
    {
        const uint64_t actual = checksum(data, size);
        if (actual == expected)
        {
            return true;
        }
        std::fprintf(stderr, "%s checksum %016" PRIx64 " (expected %016" PRIx64 ")\n", name, actual, expected);
        return false;
    }
}

int main()
{
    // Include flat, gradient and high-contrast blocks with partial SIMD groups.
    constexpr int width = 68;
    constexpr int height = 20;
    constexpr int stride = width * 4 + 12;
    constexpr int size = width * height;
    std::array<uint8_t, stride * height> pixels{};
    uint32_t noise = 9;
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            uint8_t* pixel = &pixels[y * stride + x * 4];
            const int pattern = (x / 4 + y / 4) % 3;
            if (pattern == 0)
            {
                pixel[0] = 95;
                pixel[1] = 132;
                pixel[2] = 178;
                pixel[3] = 255;
            }
            else if (pattern == 1)
            {
                pixel[0] = static_cast<uint8_t>(x * 7 + y * 3);
                pixel[1] = static_cast<uint8_t>(x * 5 - y * 9);
                pixel[2] = static_cast<uint8_t>((x ^ y) * 13);
                pixel[3] = static_cast<uint8_t>(x * 11 + y * 23);
            }
            else
            {
                noise = noise * 1664525u + 1013904223u;
                for (int channel = 0; channel < 4; ++channel)
                {
                    pixel[channel] = static_cast<uint8_t>(noise >> (channel * 8));
                }
            }
        }
    }

    const rgba_surface surface{pixels.data(), width, height, stride};
    std::array<uint8_t, size> output{};
    CompressBlocksBC3(&surface, output.data());
    bool passed = check("BC3", output.data(), size, 0xfd0e4281649dd1b7ULL);

    bc7_enc_settings bc7;
    GetProfile_alpha_basic(&bc7);
    CompressBlocksBC7(&surface, output.data(), &bc7);
    passed = check("BC7", output.data(), size, 0x47334eaffea10934ULL) && passed;

    astc_enc_settings astc;
    GetProfile_astc_alpha_fast(&astc, 4, 4);
    CompressBlocksASTC(&surface, output.data(), &astc);
    passed = check("ASTC", output.data(), size, 0x99cefdc014c332c7ULL) && passed;

    if (passed)
    {
        return 0;
    }
    return 1;
}

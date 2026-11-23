///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016, Intel Corporation
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of 
// the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
// SOFTWARE.

#include "ispc_texcomp.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <limits>
#include <vector>

namespace
{
    constexpr int block_size = 6;

    uint32_t read_le(const uint8_t* bytes, int count)
    {
        uint32_t value = 0;
        for (int i = 0; i < count; ++i)
        {
            value |= static_cast<uint32_t>(bytes[i]) << (8 * i);
        }
        return value;
    }

    int padded_size(int size)
    {
        return (size + block_size - 1) / block_size * block_size;
    }

    bool load_bmp(const char* filename, rgba_surface& image, std::vector<uint8_t>& pixels)
    {
        std::ifstream input(filename, std::ios::binary);
        std::array<uint8_t, 54> header{};
        if (!input.read(reinterpret_cast<char*>(header.data()), header.size()))
        {
            return false;
        }

        const uint32_t offset = read_le(&header[10], 4);
        const uint32_t dib_size = read_le(&header[14], 4);
        const uint32_t width = read_le(&header[18], 4);
        const uint32_t height = read_le(&header[22], 4);
        const uint32_t bits_per_pixel = read_le(&header[28], 2);
        if (header[0] != 'B' || header[1] != 'M'
            || dib_size < 40 || offset < 14ull + dib_size
            || read_le(&header[26], 2) != 1 || read_le(&header[30], 4) != 0
            || (bits_per_pixel != 24 && bits_per_pixel != 32)
            || width == 0 || width > 0xffffff
            || height == 0 || height > 0xffffff)
        {
            return false;
        }

        const uint64_t padded_bytes = 4ull * padded_size(width) * padded_size(height);
        if (padded_bytes > std::numeric_limits<int32_t>::max())
        {
            return false;
        }

        const uint32_t bytes_per_pixel = bits_per_pixel / 8;
        const uint32_t bmp_stride = (width * bytes_per_pixel + 3) & ~3u;
        input.seekg(0, std::ios::end);
        const auto file_size = input.tellg();
        if (file_size < 0 || static_cast<uint64_t>(file_size) < offset + uint64_t(bmp_stride) * height)
        {
            return false;
        }
        input.seekg(offset);

        pixels.resize(4ull * width * height);
        image = {pixels.data(), static_cast<int32_t>(width), static_cast<int32_t>(height), static_cast<int32_t>(width * 4)};
        std::vector<uint8_t> row(bmp_stride);
        for (int y = 0; y < image.height; ++y)
        {
            if (!input.read(reinterpret_cast<char*>(row.data()), row.size()))
            {
                return false;
            }

            // Keep the original test converter's bottom-up output.
            uint8_t* output = image.ptr + y * image.stride;
            for (uint32_t x = 0; x < width; ++x)
            {
                const uint8_t* pixel = row.data() + x * bytes_per_pixel;
                output[0] = pixel[2];
                output[1] = pixel[1];
                output[2] = pixel[0];
                output[3] = 255;
                if (bytes_per_pixel == 4)
                {
                    output[3] = pixel[3];
                }
                output += 4;
            }
        }
        return true;
    }

    bool store_astc(const char* filename, const rgba_surface& image, const std::vector<uint8_t>& blocks)
    {
        std::array<uint8_t, 16> header{0x13, 0xAB, 0xA1, 0x5C, block_size, block_size, 1};
        for (int i = 0; i < 3; ++i)
        {
            header[7 + i] = static_cast<uint8_t>(image.width >> (8 * i));
            header[10 + i] = static_cast<uint8_t>(image.height >> (8 * i));
        }
        header[13] = 1;

        std::ofstream output(filename, std::ios::binary);
        output.write(reinterpret_cast<const char*>(header.data()), header.size());
        output.write(reinterpret_cast<const char*>(blocks.data()), blocks.size());
        output.close();
        return static_cast<bool>(output);
    }
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::fprintf(stderr, "Usage: test_astc input.bmp output.astc\n");
        return 1;
    }

    rgba_surface image{};
    std::vector<uint8_t> pixels;
    if (!load_bmp(argv[1], image, pixels))
    {
        std::fprintf(stderr, "Could not read uncompressed 24-bit or 32-bit BMP: %s\n", argv[1]);
        return 1;
    }

    const int width = padded_size(image.width);
    const int height = padded_size(image.height);
    std::vector<uint8_t> padded_pixels(static_cast<size_t>(width) * height * 4);
    rgba_surface padded{padded_pixels.data(), width, height, width * 4};
    ReplicateBorders(&padded, &image, 0, 0, 32);

    std::vector<uint8_t> blocks(static_cast<size_t>(width / block_size) * (height / block_size) * 16);
    astc_enc_settings settings{};
    GetProfile_astc_alpha_fast(&settings, block_size, block_size);
    CompressBlocksASTC(&padded, blocks.data(), &settings);
    if (!store_astc(argv[2], image, blocks))
    {
        std::fprintf(stderr, "Could not write ASTC file: %s\n", argv[2]);
        return 1;
    }
    return 0;
}


/*
 * This file is part of sc4-dbpf-loading, a DLL Plugin for SimCity 4 that
 * optimizes the DBPF loading.
 *
 * Copyright (C) 2026 Casper Van Gheluwe
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, under
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

// Native-shaped QFS benchmark for SimCity 4.
//
// The decode_ref function follows the Mac x86 symbolized binary's
// cRZFastCompression3::decoderef at 0x002AA57E.  It intentionally uses the
// same pointer-oriented, byte-at-a-time overlapping copies visible there.

#include <windows.h>
#include <cstring>
#ifdef QFS_SIMD
#include <immintrin.h>
#endif
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <string>
#include <vector>

struct Sample {
    uint32_t type;
    uint32_t compressed;
    uint32_t uncompressed;
    double microseconds;
};

#ifdef QFS_SIMD
static void copy_nonoverlap_avx2(uint8_t* destination, const uint8_t* source, size_t count) {
    while (count >= 32) {
        const __m256i value = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(source));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(destination), value);
        source += 32;
        destination += 32;
        count -= 32;
    }
    if (count >= 16) {
        const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i*>(source));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(destination), value);
        source += 16;
        destination += 16;
        count -= 16;
    }
    if (count) std::memcpy(destination, source, count);
}

static void copy_overlap_avx2(uint8_t* destination, const uint8_t* source,
                              size_t count, size_t distance) {
    if (count < 32 || distance < 32) {
        size_t seeded = 0;
        while (seeded < count && seeded < distance) {
            destination[seeded] = source[seeded];
            ++seeded;
        }
        size_t copied = seeded;
        while (copied < count) {
            const size_t chunk = (copied < count - copied) ? copied : count - copied;
            std::memcpy(destination + copied, destination, chunk);
            copied += chunk;
        }
        return;
    }
    copy_nonoverlap_avx2(destination, source, count);
}

// A modern x86/SIMD implementation of the same RefPack state machine.
static uint32_t decode_ref_simd(const uint8_t* input, int* consumed, uint8_t* output) {
    const uint8_t* source = input + 2;
    if (*input & 1) source = input + 5;
    const uint32_t output_size = (uint32_t(source[0]) << 16) |
                                 (uint32_t(source[1]) << 8) | uint32_t(source[2]);
    source += 3;
    uint8_t* destination = output;
    for (;;) {
        const uint8_t code = *source++;
        if (!(code & 0x80)) {
            const uint8_t b2 = *source++;
            const size_t literals = code & 3;
            if (literals) { std::memcpy(destination, source, literals); destination += literals; source += literals; }
            const size_t length = ((code & 0x1c) >> 2) + 3;
            const size_t distance = (size_t(b2) + (code & 0x60) * 8) + 1;
            copy_overlap_avx2(destination, destination - distance, length, distance);
            destination += length;
        } else if (!(code & 0x40)) {
            const uint8_t b2 = *source++, b3 = *source++;
            const size_t literals = b2 >> 6;
            if (literals) { std::memcpy(destination, source, literals); destination += literals; source += literals; }
            const size_t length = (code & 0x3f) + 4;
            const size_t distance = ((size_t(b2 & 0x3f) << 8) | b3) + 1;
            copy_overlap_avx2(destination, destination - distance, length, distance);
            destination += length;
        } else if (!(code & 0x20)) {
            const uint8_t b2 = *source++, b3 = *source++, b4 = *source++;
            const size_t literals = code & 3;
            if (literals) { std::memcpy(destination, source, literals); destination += literals; source += literals; }
            const size_t length = ((code & 0x0c) << 6) + b4 + 5;
            const size_t distance = (((code & 0x10) >> 4) << 16) | (size_t(b2) << 8) | b3;
            copy_overlap_avx2(destination, destination - distance - 1, length, distance + 1);
            destination += length;
        } else {
            const size_t literals = (code & 0x1f) * 4 + 4;
            if (literals > 0x70) {
                const size_t tail = code & 3;
                if (tail) { std::memcpy(destination, source, tail); destination += tail; source += tail; }
                break;
            }
            std::memcpy(destination, source, literals);
            destination += literals;
            source += literals;
        }
    }
    if (consumed) *consumed = int(source - input);
    return output_size;
}

static __forceinline void copy_literals_0_3(
    uint8_t*& destination, const uint8_t*& source, size_t count) {
    switch (count) {
        case 3: destination[2] = source[2]; [[fallthrough]];
        case 2: destination[1] = source[1]; [[fallthrough]];
        case 1: destination[0] = source[0]; [[fallthrough]];
        default: break;
    }
    destination += count;
    source += count;
}

static __forceinline void copy_short_nonoverlap(
    uint8_t* destination, const uint8_t* source, size_t count) {
    while (count >= 16) {
        const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i*>(source));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(destination), value);
        source += 16;
        destination += 16;
        count -= 16;
    }
    if (count >= 8) {
        const __m128i value = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(source));
        _mm_storel_epi64(reinterpret_cast<__m128i*>(destination), value);
        source += 8;
        destination += 8;
        count -= 8;
    }
    if (count >= 4) {
        uint32_t value;
        std::memcpy(&value, source, sizeof(value));
        std::memcpy(destination, &value, sizeof(value));
        source += 4;
        destination += 4;
        count -= 4;
    }
    switch (count) {
        case 3: destination[2] = source[2]; [[fallthrough]];
        case 2: destination[1] = source[1]; [[fallthrough]];
        case 1: destination[0] = source[0];
        default: break;
    }
}

static __forceinline void copy_match_hybrid(
    uint8_t* destination, const uint8_t* source, size_t count, size_t distance) {
    if (count <= 31) {
        if (distance >= count) {
            copy_short_nonoverlap(destination, source, count);
        } else {
            while (count--) *destination++ = *source++;
        }
        return;
    }
    copy_overlap_avx2(destination, source, count, distance);
}

// Candidate selected from corpus profiling: eliminate calls on the dominant
// 3-15 byte, non-overlapping path while retaining the proven bulk algorithm.
static uint32_t decode_ref_hybrid(const uint8_t* input, int* consumed, uint8_t* output) {
    const uint8_t* source = input + 2;
    if (*input & 1) source = input + 5;
    const uint32_t output_size = (uint32_t(source[0]) << 16) |
                                 (uint32_t(source[1]) << 8) | uint32_t(source[2]);
    source += 3;
    uint8_t* destination = output;
    for (;;) {
        const uint8_t code = *source++;
        if (!(code & 0x80)) {
            const uint8_t b2 = *source++;
            const size_t literals = code & 3;
            copy_literals_0_3(destination, source, literals);
            const size_t length = ((code & 0x1c) >> 2) + 3;
            const size_t distance = size_t(b2) + (code & 0x60) * 8 + 1;
            copy_match_hybrid(destination, destination - distance, length, distance);
            destination += length;
        } else if (!(code & 0x40)) {
            const uint8_t b2 = *source++, b3 = *source++;
            const size_t literals = b2 >> 6;
            copy_literals_0_3(destination, source, literals);
            const size_t length = (code & 0x3f) + 4;
            const size_t distance = ((size_t(b2 & 0x3f) << 8) | b3) + 1;
            copy_match_hybrid(destination, destination - distance, length, distance);
            destination += length;
        } else if (!(code & 0x20)) {
            const uint8_t b2 = *source++, b3 = *source++, b4 = *source++;
            const size_t literals = code & 3;
            copy_literals_0_3(destination, source, literals);
            const size_t length = ((code & 0x0c) << 6) + b4 + 5;
            const size_t distance = ((((size_t(code & 0x10) >> 4) << 16) |
                                     (size_t(b2) << 8) | b3) + 1);
            copy_match_hybrid(destination, destination - distance, length, distance);
            destination += length;
        } else {
            const size_t literals = (code & 0x1f) * 4 + 4;
            if (literals > 0x70) {
                const size_t tail = code & 3;
                copy_literals_0_3(destination, source, tail);
                break;
            }
            std::memcpy(destination, source, literals);
            destination += literals;
            source += literals;
        }
    }
    if (consumed) *consumed = int(source - input);
    return output_size;
}
#endif

static uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
           (uint32_t(p[3]) << 24);
}

static constexpr bool range_within(size_t offset, size_t length, size_t total) {
    return offset <= total && length <= total - offset;
}

static_assert(range_within(4, 3, 7));
static_assert(!range_within(size_t(-2), 4, size_t(-1)));

// Equivalent to cRZFastCompression3::decoderef(char*, long*, char*).
static uint32_t decode_ref(const uint8_t* input, int* consumed, uint8_t* output) {
    if (!input) {
        if (consumed) *consumed = 0;
        return 0;
    }
    const uint8_t* source = input + 2;
    if (*input & 1) source = input + 5;
    const uint32_t output_size = (uint32_t(source[0]) << 16) |
                                 (uint32_t(source[1]) << 8) | uint32_t(source[2]);
    source += 3;
    uint8_t* destination = output;

    for (;;) {
        const uint8_t code = *source;
        const uint8_t* next = source + 1;
        if (!(code & 0x80)) {
            const uint8_t b2 = source[1];
            uint32_t literals = code & 3;
            for (uint32_t i = 0; i != literals; ++i) *destination++ = source[i + 2];
            source += literals + 2;
            uint8_t* back = destination - 1 - (uint32_t(b2) + (code & 0x60) * 8);
            uint8_t* end = destination + ((code & 0x1c) >> 2) + 3;
            while (destination != end) *destination++ = *back++;
            continue;
        }
        if (!(code & 0x40)) {
            const uint8_t b2 = source[1], b3 = source[2];
            uint32_t literals = b2 >> 6;
            for (uint32_t i = 0; i != literals; ++i) *destination++ = source[i + 3];
            source += literals + 3;
            uint8_t* back = destination - 1 - ((uint32_t(b2 & 0x3f) * 0x100) + b3);
            uint8_t* end = destination + (code & 0x3f) + 4;
            while (destination != end) *destination++ = *back++;
            continue;
        }
        if (!(code & 0x20)) {
            const uint8_t b2 = source[1], b3 = source[2], b4 = source[3];
            uint32_t literals = code & 3;
            for (uint32_t i = 0; i != literals; ++i) *destination++ = source[i + 4];
            source += literals + 4;
            uint8_t* back = destination - 1 - (((uint32_t(code & 0x10) >> 4) * 0x10000) +
                                               (uint32_t(b2) * 0x100) + b3);
            uint8_t* end = destination + (code & 0x0c) * 0x40 + 5 + b4;
            while (destination != end) *destination++ = *back++;
            continue;
        }
        const uint32_t literal_count = (code & 0x1f) * 4 + 4;
        if (literal_count > 0x70) {
            for (uint32_t i = 0; i != (code & 3); ++i) {
                *destination++ = *next++;
            }
            source = next;
            break;
        }
        for (uint32_t i = 0; i != literal_count; ++i) *destination++ = next[i];
        source = next + literal_count;
    }
    if (consumed) *consumed = int(source - input);
    return output_size;
}

static bool is_qfs(const std::vector<uint8_t>& payload) {
    return payload.size() >= 9 && (payload[4] == 0x10 || payload[4] == 0x50) &&
           payload[5] == 0xfb;
}

static uint32_t decode_dispatch(const uint8_t* input, int* consumed, uint8_t* output) {
#if defined(QFS_HYBRID)
    return decode_ref_hybrid(input, consumed, output);
#elif defined(QFS_SIMD)
    return decode_ref_simd(input, consumed, output);
#else
    return decode_ref(input, consumed, output);
#endif
}

static std::vector<uint8_t> read_file(const std::wstring& path) {
    std::ifstream f(std::filesystem::path(path), std::ios::binary);
    if (!f) return {};
    f.seekg(0, std::ios::end);
    const auto size = f.tellg();
    if (size <= 0) return {};
    f.seekg(0);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (!f.read(reinterpret_cast<char*>(data.data()), size)) return {};
    return data;
}

static const char* type_name(uint32_t type) {
    switch (type) {
        case 0x6534284a: return "Exemplar/Cohort";
        case 0x7ab50e44: return "FSH";
        case 0x5ad0e817: return "S3D";
        default: return "other";
    }
}

#ifndef QFS_BENCH_NO_MAIN
int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::wcerr << L"usage: qfs_native_bench.exe file.dat [file2.dat ...] or directory\n";
        return 2;
    }
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    std::map<std::string, std::vector<Sample>> results;
    size_t files = 0, qfs_entries = 0, errors = 0;
    volatile uint8_t checksum = 0;

    std::vector<std::wstring> input_files;
    for (int arg = 1; arg < argc; ++arg) {
        const std::filesystem::path input(argv[arg]);
        std::error_code error;
        if (std::filesystem::is_regular_file(input, error)) {
            input_files.push_back(input.wstring());
        } else if (std::filesystem::is_directory(input, error)) {
            for (std::filesystem::recursive_directory_iterator it(
                     input, std::filesystem::directory_options::skip_permission_denied, error), end;
                 it != end; it.increment(error)) {
                if (error) { error.clear(); continue; }
                if (it->is_regular_file(error) &&
                    _wcsicmp(it->path().extension().c_str(), L".dat") == 0)
                    input_files.push_back(it->path().wstring());
            }
        }
    }
    std::sort(input_files.begin(), input_files.end());

    for (const auto& input_file : input_files) {
        const auto file = read_file(input_file);
        if (file.size() < 0x60 || std::string(reinterpret_cast<const char*>(file.data()), 4) != "DBPF")
            continue;
        ++files;
        const uint32_t count = le32(file.data() + 0x24);
        const uint32_t index_offset = le32(file.data() + 0x28);
        const uint32_t index_size = le32(file.data() + 0x2c);
        if (!count || count > index_size / 20u ||
            !range_within(index_offset, index_size, file.size())) continue;
        const uint32_t stride = index_size / count;
        for (uint32_t i = 0; i < count; ++i) {
            const size_t record_offset = size_t(index_offset) + size_t(i) * stride;
            const uint8_t* record = file.data() + record_offset;
            const uint32_t type = le32(record);
            const uint32_t offset = le32(record + 12);
            const uint32_t size = le32(record + 16);
            if (!range_within(offset, size, file.size())) continue;
            std::vector<uint8_t> payload(file.begin() + offset, file.begin() + offset + size);
            if (!is_qfs(payload)) continue;
            ++qfs_entries;
            if (payload.size() < 9) { ++errors; continue; }
            const uint32_t expected = (uint32_t(payload[6]) << 16) |
                                      (uint32_t(payload[7]) << 8) | uint32_t(payload[8]);
            std::vector<uint8_t> decoded(expected + 16);
            int consumed = 0;
            const uint32_t got = decode_dispatch(payload.data() + 4, &consumed, decoded.data());
            if (got != expected) { ++errors; continue; }
            const int repeats = 5;
            LARGE_INTEGER start{}, end{};
            QueryPerformanceCounter(&start);
            for (int n = 0; n < repeats; ++n) {
                decode_dispatch(payload.data() + 4, &consumed, decoded.data());
                checksum ^= decoded[n % (expected ? expected : 1)];
            }
            QueryPerformanceCounter(&end);
            const double us = (double(end.QuadPart - start.QuadPart) * 1e6 /
                               double(frequency.QuadPart)) / repeats;
            results[type_name(type)].push_back({type, size, expected, us});
        }
        std::wcerr << L"processed " << input_file
                   << L" (QFS entries cumulative: " << qfs_entries << L")\n";
    }

    std::cout << "files=" << files << " qfs_entries=" << qfs_entries << " errors=" << errors
              << " checksum=" << unsigned(checksum) << "\n";
    std::cout << std::fixed << std::setprecision(2);
    for (std::map<std::string, std::vector<Sample>>::const_iterator item = results.begin();
         item != results.end(); ++item) {
        const std::string& name = item->first;
        const std::vector<Sample>& samples = item->second;
        std::vector<double> times;
        std::vector<uint32_t> sizes;
        uint64_t total_compressed = 0;
        uint64_t total_bytes = 0;
        double total_decode_us = 0;
        for (std::vector<Sample>::const_iterator s = samples.begin(); s != samples.end(); ++s) {
            times.push_back(s->microseconds);
            sizes.push_back(s->uncompressed);
            total_compressed += s->compressed;
            total_bytes += s->uncompressed;
            total_decode_us += s->microseconds;
        }
        std::sort(times.begin(), times.end());
        std::sort(sizes.begin(), sizes.end());
        const auto percentile = [&](double p) { return times[std::min<size_t>(times.size() - 1, size_t(times.size() * p))]; };
        const double median = percentile(0.5);
        const uint32_t size_median = sizes[sizes.size() / 2];
        const double mean = total_decode_us / samples.size();
        const double ratio = double(total_bytes) / std::max<uint64_t>(1, total_compressed);
        const double us_per_kb = total_decode_us * 1024.0 / std::max<uint64_t>(1, total_bytes);
        const double mb_per_second = double(total_bytes) /
                                     (total_decode_us > 0.000001 ? total_decode_us : 0.000001);
        std::cout << name << " n=" << samples.size()
                  << " compressed=" << total_compressed << "B"
                  << " uncompressed=" << total_bytes << "B"
                  << " ratio=" << ratio
                  << " size_med=" << size_median << "B"
                  << " decode_us_mean=" << mean
                  << " p50=" << median
                  << " p90=" << percentile(0.9)
                  << " p99=" << percentile(0.99)
                  << " total_decode_ms=" << (total_decode_us / 1000.0)
                  << " decode_us_per_KB=" << us_per_kb
                  << " throughput_MB_s=" << mb_per_second << "\n";
    }
    return 0;
}
#endif

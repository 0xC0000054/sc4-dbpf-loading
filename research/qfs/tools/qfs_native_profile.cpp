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

// Dual-decoder RefPack profiler for the SC4 FSH corpus.
//
// Build as x86 with AVX2 enabled. This reuses the two validated decoder
// implementations from qfs_native_bench.cpp, then correlates their timings
// with command, literal, match-length, and match-distance distributions.

#define NOMINMAX
#define QFS_SIMD 1
#define QFS_BENCH_NO_MAIN 1
#include "qfs_native_bench.cpp"

#include <array>
#include <cmath>
#include <filesystem>
#include <limits>

namespace {

constexpr uint32_t FshType = 0x7ab50e44;

struct StreamProfile {
    uint64_t commands = 0;
    uint64_t literal_bytes = 0;
    uint64_t match_bytes = 0;
    uint64_t overlapping_match_bytes = 0;
    std::array<uint64_t, 4> command_classes{}; // short, medium, long, literal-only
    std::array<uint64_t, 8> literal_lengths{}; // 0,1,2,3,4-15,16-31,32-63,64+
    std::array<uint64_t, 7> match_lengths{};   // 3-7,8-15,16-31,32-63,64-127,128-255,256+
    std::array<uint64_t, 9> match_distances{}; // 1,2,3-4,5-8,9-15,16-31,32-63,64-255,256+
};

struct TimedSample {
    uint32_t argument_index;
    uint32_t offset;
    uint32_t compressed;
    uint32_t uncompressed;
    float windows_us;
    float simd_us;
    float hybrid_us;
};

struct Aggregate {
    uint64_t samples = 0;
    uint64_t compressed = 0;
    uint64_t uncompressed = 0;
    double windows_us = 0;
    double simd_us = 0;
    double hybrid_us = 0;
    StreamProfile profile;
};

size_t literal_bucket(size_t n) {
    if (n <= 3) return n;
    if (n <= 15) return 4;
    if (n <= 31) return 5;
    if (n <= 63) return 6;
    return 7;
}

size_t length_bucket(size_t n) {
    if (n <= 7) return 0;
    if (n <= 15) return 1;
    if (n <= 31) return 2;
    if (n <= 63) return 3;
    if (n <= 127) return 4;
    if (n <= 255) return 5;
    return 6;
}

size_t distance_bucket(size_t n) {
    if (n == 1) return 0;
    if (n == 2) return 1;
    if (n <= 4) return 2;
    if (n <= 8) return 3;
    if (n <= 15) return 4;
    if (n <= 31) return 5;
    if (n <= 63) return 6;
    if (n <= 255) return 7;
    return 8;
}

bool profile_stream(const uint8_t* input, const uint8_t* end, StreamProfile& result,
                    const char** error_reason = nullptr, size_t* error_offset = nullptr) {
    const uint8_t* const begin = input;
    const auto fail = [&](const char* reason) {
        if (error_reason) *error_reason = reason;
        if (error_offset) *error_offset = input ? size_t(std::max(ptrdiff_t(0), end - begin)) : 0;
        return false;
    };
    const auto fail_at = [&](const char* reason, const uint8_t* position) {
        if (error_reason) *error_reason = reason;
        if (error_offset) *error_offset = position && begin
            ? size_t(std::max(ptrdiff_t(0), position - begin)) : 0;
        return false;
    };
    if (!input || input > end || size_t(end - input) < 5) return fail("truncated QFS header");
    const uint8_t* source = input + ((*input & 1) ? 5 : 2);
    if (size_t(end - source) < 3) return fail_at("truncated output-size field", source);
    const size_t expected = (size_t(source[0]) << 16) | (size_t(source[1]) << 8) | source[2];
    source += 3;
    size_t produced = 0;

    for (;;) {
        if (source >= end) return fail_at("missing terminator/control byte", source);
        const uint8_t code = *source++;
        size_t literals = 0, length = 0, distance = 0;
        size_t command_class = 0;

        if (!(code & 0x80)) {
            if (source >= end) return fail_at("truncated short control", source);
            const uint8_t b2 = *source++;
            literals = code & 3;
            length = ((code & 0x1c) >> 2) + 3;
            distance = size_t(b2) + (code & 0x60) * 8 + 1;
            command_class = 0;
        } else if (!(code & 0x40)) {
            if (size_t(end - source) < 2) return fail_at("truncated medium control", source);
            const uint8_t b2 = *source++, b3 = *source++;
            literals = b2 >> 6;
            length = (code & 0x3f) + 4;
            distance = (size_t(b2 & 0x3f) << 8) + b3 + 1;
            command_class = 1;
        } else if (!(code & 0x20)) {
            if (size_t(end - source) < 3) return fail_at("truncated long control", source);
            const uint8_t b2 = *source++, b3 = *source++, b4 = *source++;
            literals = code & 3;
            length = ((code & 0x0c) << 6) + b4 + 5;
            distance = (size_t(code & 0x10) << 12) + (size_t(b2) << 8) + b3 + 1;
            command_class = 2;
        } else {
            literals = (code & 0x1f) * 4 + 4;
            command_class = 3;
            if (literals > 0x70) {
                literals = code & 3;
                if (literals > size_t(end - source))
                    return fail_at("truncated terminator literals", source);
                if (literals > expected - produced)
                    return fail_at("output exceeds QFS header", source);
                result.literal_bytes += literals;
                result.literal_lengths[literal_bucket(literals)]++;
                produced += literals;
                source += literals;
                break;
            }
        }

        if (literals > size_t(end - source)) return fail_at("truncated command literals", source);
        if (literals > expected - produced) return fail_at("output exceeds QFS header", source);
        result.commands++;
        result.command_classes[command_class]++;
        result.literal_bytes += literals;
        result.literal_lengths[literal_bucket(literals)]++;
        produced += literals;
        source += literals;

        if (length) {
            if (distance > produced) return fail_at("match distance precedes output", source);
            if (length > expected - produced) return fail_at("output exceeds QFS header", source);
            result.match_bytes += length;
            result.match_lengths[length_bucket(length)]++;
            result.match_distances[distance_bucket(distance)]++;
            if (distance < length) result.overlapping_match_bytes += length;
            produced += length;
        }
    }
    if (produced != expected) return fail_at("produced size differs from QFS header", source);
    return true;
}

void add_profile(StreamProfile& to, const StreamProfile& from) {
    to.commands += from.commands;
    to.literal_bytes += from.literal_bytes;
    to.match_bytes += from.match_bytes;
    to.overlapping_match_bytes += from.overlapping_match_bytes;
    for (size_t i = 0; i < to.command_classes.size(); ++i) to.command_classes[i] += from.command_classes[i];
    for (size_t i = 0; i < to.literal_lengths.size(); ++i) to.literal_lengths[i] += from.literal_lengths[i];
    for (size_t i = 0; i < to.match_lengths.size(); ++i) to.match_lengths[i] += from.match_lengths[i];
    for (size_t i = 0; i < to.match_distances.size(); ++i) to.match_distances[i] += from.match_distances[i];
}

void add_sample(Aggregate& to, const TimedSample& sample, const StreamProfile& profile) {
    ++to.samples;
    to.compressed += sample.compressed;
    to.uncompressed += sample.uncompressed;
    to.windows_us += sample.windows_us;
    to.simd_us += sample.simd_us;
    to.hybrid_us += sample.hybrid_us;
    add_profile(to.profile, profile);
}

template <size_t N>
void print_distribution(const char* title, const std::array<uint64_t, N>& values,
                        const std::array<const char*, N>& labels) {
    const uint64_t total = std::accumulate(values.begin(), values.end(), uint64_t(0));
    std::cout << "  " << title << ":";
    for (size_t i = 0; i < N; ++i) {
        const double percentage = total ? 100.0 * double(values[i]) / double(total) : 0.0;
        std::cout << " " << labels[i] << "=" << values[i] << "(" << percentage << "%)";
    }
    std::cout << "\n";
}

void print_aggregate(const char* name, const Aggregate& value) {
    const double speedup = value.simd_us ? value.windows_us / value.simd_us : 0.0;
    const double hybrid_speedup = value.hybrid_us ? value.windows_us / value.hybrid_us : 0.0;
    const double hybrid_vs_simd = value.hybrid_us ? value.simd_us / value.hybrid_us : 0.0;
    const double ratio = value.compressed ? double(value.uncompressed) / value.compressed : 0.0;
    const uint64_t output_bytes = value.profile.literal_bytes + value.profile.match_bytes;
    const double literal_share = output_bytes ? 100.0 * value.profile.literal_bytes / output_bytes : 0.0;
    const double overlap_share = value.profile.match_bytes
        ? 100.0 * value.profile.overlapping_match_bytes / value.profile.match_bytes : 0.0;

    std::cout << "\n[" << name << "] entries=" << value.samples
              << " compressed_MB=" << double(value.compressed) / 1e6
              << " uncompressed_MB=" << double(value.uncompressed) / 1e6
              << " ratio=" << ratio
              << " windows_ms=" << value.windows_us / 1000.0
              << " simd_ms=" << value.simd_us / 1000.0
              << " speedup=" << speedup
              << " hybrid_ms=" << value.hybrid_us / 1000.0
              << " hybrid_speedup=" << hybrid_speedup
              << " hybrid_vs_simd=" << hybrid_vs_simd
              << " literal_output=" << literal_share << "%"
              << " overlapping_match_output=" << overlap_share << "%\n";
    print_distribution("commands", value.profile.command_classes,
        std::array<const char*, 4>{"short", "medium", "long", "literal"});
    print_distribution("literal lengths", value.profile.literal_lengths,
        std::array<const char*, 8>{"0", "1", "2", "3", "4-15", "16-31", "32-63", "64+"});
    print_distribution("match lengths", value.profile.match_lengths,
        std::array<const char*, 7>{"3-7", "8-15", "16-31", "32-63", "64-127", "128-255", "256+"});
    print_distribution("match distances", value.profile.match_distances,
        std::array<const char*, 9>{"1", "2", "3-4", "5-8", "9-15", "16-31", "32-63", "64-255", "256+"});
}

double percentile(std::vector<double> values, double p) {
    std::sort(values.begin(), values.end());
    return values[std::min(values.size() - 1, size_t(values.size() * p))];
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::wcerr << L"usage: qfs_native_profile.exe [--scan-errors] [--repeats=N] "
                      L"file-or-directory [...]\n";
        return 2;
    }

    int repeats = 5;
    bool scan_errors = false;
    int first_file = 1;
    while (first_file < argc) {
        const std::wstring option(argv[first_file]);
        if (option.rfind(L"--repeats=", 0) == 0) {
            repeats = std::max(1, _wtoi(argv[first_file] + 10));
        } else if (option == L"--scan-errors") {
            scan_errors = true;
        } else {
            break;
        }
        ++first_file;
    }

    std::vector<std::wstring> input_files;
    for (int arg = first_file; arg < argc; ++arg) {
        const std::filesystem::path input(argv[arg]);
        std::error_code error;
        if (std::filesystem::is_regular_file(input, error)) {
            input_files.push_back(input.wstring());
            continue;
        }
        if (!std::filesystem::is_directory(input, error)) continue;
        for (std::filesystem::recursive_directory_iterator it(
                 input, std::filesystem::directory_options::skip_permission_denied, error), end;
             it != end; it.increment(error)) {
            if (error) { error.clear(); continue; }
            if (!it->is_regular_file(error)) continue;
            if (_wcsicmp(it->path().extension().c_str(), L".dat") == 0)
                input_files.push_back(it->path().wstring());
        }
    }
    std::sort(input_files.begin(), input_files.end());

    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    std::vector<TimedSample> samples;
    Aggregate all;
    uint64_t errors = 0;
    uint64_t fsh_seen = 0;
    volatile uint8_t checksum = 0;

    for (size_t file_index = 0; file_index < input_files.size(); ++file_index) {
        const auto file = read_file(input_files[file_index]);
        if (file.size() < 0x60 || std::memcmp(file.data(), "DBPF", 4) != 0) continue;
        const uint32_t count = le32(file.data() + 0x24);
        const uint32_t index_offset = le32(file.data() + 0x28);
        const uint32_t index_size = le32(file.data() + 0x2c);
        if (!count || count > index_size / 20u ||
            !range_within(index_offset, index_size, file.size())) continue;
        const uint32_t stride = index_size / count;

        for (uint32_t i = 0; i < count; ++i) {
            const size_t record_offset = size_t(index_offset) + size_t(i) * stride;
            const uint8_t* record = file.data() + record_offset;
            if (le32(record) != FshType) continue;
            const uint32_t group = le32(record + 4), instance = le32(record + 8);
            const uint32_t offset = le32(record + 12), size = le32(record + 16);
            if (!range_within(offset, size, file.size())) continue;
            const uint8_t* payload = file.data() + offset;
            if (size < 9 || (payload[4] != 0x10 && payload[4] != 0x50) || payload[5] != 0xfb) continue;
            ++fsh_seen;
            const uint32_t expected = (uint32_t(payload[6]) << 16) |
                                      (uint32_t(payload[7]) << 8) | payload[8];
            if (!expected) continue;

            StreamProfile profile;
            const char* profile_error = nullptr;
            size_t profile_error_offset = 0;
            if (!profile_stream(payload + 4, payload + size, profile,
                                &profile_error, &profile_error_offset)) {
                ++errors;
                std::wcerr << L"WARNING: invalid FSH QFS stream file=\""
                           << input_files[file_index] << L"\" entry=" << i
                           << L" TGI=0x" << std::hex << std::setw(8) << std::setfill(L'0') << FshType
                           << L"-0x" << std::setw(8) << group
                           << L"-0x" << std::setw(8) << instance << std::dec << std::setfill(L' ')
                           << L" DBPF_offset=" << offset << L" compressed=" << size
                           << L" expected=" << expected
                           << L" QFS_offset=" << profile_error_offset
                           << L" reason=" << profile_error << L"\n";
                continue;
            }
            std::vector<uint8_t> windows_output(expected + 16), simd_output(expected + 16),
                                 hybrid_output(expected + 16);
            int windows_consumed = 0, simd_consumed = 0, hybrid_consumed = 0;
            decode_ref(payload + 4, &windows_consumed, windows_output.data());
            decode_ref_simd(payload + 4, &simd_consumed, simd_output.data());
            decode_ref_hybrid(payload + 4, &hybrid_consumed, hybrid_output.data());
            if (windows_consumed != simd_consumed || windows_consumed != hybrid_consumed ||
                std::memcmp(windows_output.data(), simd_output.data(), expected) != 0 ||
                std::memcmp(windows_output.data(), hybrid_output.data(), expected) != 0) {
                ++errors;
                size_t mismatch = 0;
                while (mismatch < expected &&
                       windows_output[mismatch] == simd_output[mismatch] &&
                       windows_output[mismatch] == hybrid_output[mismatch]) ++mismatch;
                std::wcerr << L"WARNING: decoder mismatch file=\"" << input_files[file_index]
                           << L"\" entry=" << i
                           << L" TGI=0x" << std::hex << std::setw(8) << std::setfill(L'0') << FshType
                           << L"-0x" << std::setw(8) << group
                           << L"-0x" << std::setw(8) << instance << std::dec << std::setfill(L' ')
                           << L" DBPF_offset=" << offset << L" compressed=" << size
                           << L" expected=" << expected
                           << L" consumed=" << windows_consumed << L"/" << simd_consumed
                           << L"/" << hybrid_consumed
                           << L" first_output_mismatch="
                           << (mismatch < expected ? std::to_wstring(mismatch) : L"none") << L"\n";
                continue;
            }

            if (scan_errors) continue;

            auto time_decoder = [&](auto decoder, uint8_t* output, int& consumed) {
                LARGE_INTEGER start{}, end{};
                QueryPerformanceCounter(&start);
                for (int n = 0; n < repeats; ++n) {
                    decoder(payload + 4, &consumed, output);
                    checksum ^= output[size_t(n) % expected];
                }
                QueryPerformanceCounter(&end);
                return float(double(end.QuadPart - start.QuadPart) * 1e6 /
                             double(frequency.QuadPart) / repeats);
            };

            float windows_us, simd_us, hybrid_us;
            if ((samples.size() % 3) == 0) {
                windows_us = time_decoder(decode_ref, windows_output.data(), windows_consumed);
                simd_us = time_decoder(decode_ref_simd, simd_output.data(), simd_consumed);
                hybrid_us = time_decoder(decode_ref_hybrid, hybrid_output.data(), hybrid_consumed);
            } else if ((samples.size() % 3) == 1) {
                simd_us = time_decoder(decode_ref_simd, simd_output.data(), simd_consumed);
                hybrid_us = time_decoder(decode_ref_hybrid, hybrid_output.data(), hybrid_consumed);
                windows_us = time_decoder(decode_ref, windows_output.data(), windows_consumed);
            } else {
                hybrid_us = time_decoder(decode_ref_hybrid, hybrid_output.data(), hybrid_consumed);
                windows_us = time_decoder(decode_ref, windows_output.data(), windows_consumed);
                simd_us = time_decoder(decode_ref_simd, simd_output.data(), simd_consumed);
            }

            TimedSample sample{
                uint32_t(file_index), offset, size, expected, windows_us, simd_us, hybrid_us};
            samples.push_back(sample);
            add_sample(all, sample, profile);
        }
        std::wcerr << L"processed " << input_files[file_index]
                   << L" (FSH cumulative: " << (scan_errors ? fsh_seen : samples.size()) << L")\n";
    }

    if (scan_errors) {
        std::cout << "FSH streams scanned=" << fsh_seen << " errors=" << errors << "\n";
        return 0;
    }

    if (samples.empty()) {
        std::cerr << "No compressed FSH entries found.\n";
        return 1;
    }

    std::vector<double> latencies, efficiencies;
    latencies.reserve(samples.size());
    efficiencies.reserve(samples.size());
    for (const auto& sample : samples) {
        latencies.push_back(sample.simd_us);
        efficiencies.push_back(double(sample.simd_us) / sample.uncompressed);
    }
    const double latency_p90 = percentile(latencies, 0.90);
    const double latency_p99 = percentile(latencies, 0.99);
    const double efficiency_p99 = percentile(efficiencies, 0.99);

    Aggregate latency_90_99, latency_99, inefficient_99;
    for (size_t file_index = 0; file_index < input_files.size(); ++file_index) {
        bool needed = false;
        for (const auto& sample : samples) if (sample.argument_index == uint32_t(file_index) &&
            (sample.simd_us >= latency_p90 || double(sample.simd_us) / sample.uncompressed >= efficiency_p99)) {
            needed = true;
            break;
        }
        if (!needed) continue;
        const auto file = read_file(input_files[file_index]);
        for (const auto& sample : samples) {
            if (sample.argument_index != uint32_t(file_index)) continue;
            const bool in_90_99 = sample.simd_us >= latency_p90 && sample.simd_us < latency_p99;
            const bool in_99 = sample.simd_us >= latency_p99;
            const bool inefficient = double(sample.simd_us) / sample.uncompressed >= efficiency_p99;
            if (!in_90_99 && !in_99 && !inefficient) continue;
            if (!range_within(sample.offset, sample.compressed, file.size())) continue;
            StreamProfile profile;
            const uint8_t* payload = file.data() + sample.offset;
            if (!profile_stream(payload + 4, payload + sample.compressed, profile)) continue;
            if (in_90_99) add_sample(latency_90_99, sample, profile);
            if (in_99) add_sample(latency_99, sample, profile);
            if (inefficient) add_sample(inefficient_99, sample, profile);
        }
    }

    std::cout << std::fixed << std::setprecision(2)
              << "FSH entries=" << samples.size() << " errors=" << errors
              << " checksum=" << unsigned(checksum)
              << " latency_p90_us=" << latency_p90
              << " latency_p99_us=" << latency_p99
              << " efficiency_p99_ns_per_byte=" << efficiency_p99 * 1000.0 << "\n";
    print_aggregate("all FSH", all);
    print_aggregate("SIMD latency p90-p99", latency_90_99);
    print_aggregate("SIMD latency p99", latency_99);
    print_aggregate("SIMD ns/byte p99", inefficient_99);
    return 0;
}

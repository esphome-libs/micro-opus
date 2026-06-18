// Copyright 2026 Kevin Ahrendt
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Unit tests for src/opus_header.cpp (RFC 7845 OpusHead/OpusTags parsing). Covers the field decode,
// channel-mapping-family rules, and every OpusHeaderResult error path. Builds headers with the
// shared ogg_mux helpers, then mutates bytes for the negative cases.

#include "ogg_mux.h"
#include "opus_header.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

using micro_opus::OpusHead;
using micro_opus::parse_opus_head;
using micro_opus_test::make_opus_head_family0;
using micro_opus_test::make_opus_head_family1;

int g_failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::printf("  FAIL: %s\n", message);
        ++g_failures;
    }
}

// Parse and assert the result code matches expectation.
void expect_result(const char* name, const std::vector<uint8_t>& head,
                   micro_opus::OpusHeaderResult expected) {
    OpusHead parsed{};
    const auto result = parse_opus_head(head.data(), head.size(), parsed);
    if (result != expected) {
        std::printf("  FAIL: %s expected result %d, got %d\n", name, expected, result);
        ++g_failures;
    }
}

}  // namespace

int main() {
    std::printf("opus_header parsing unit test\n");

    // --- is_opus_head / is_opus_tags ---
    {
        const auto head = make_opus_head_family0(2);
        check(micro_opus::is_opus_head(head.data(), head.size()), "is_opus_head true for OpusHead");
        check(!micro_opus::is_opus_tags(head.data(), head.size()),
              "is_opus_tags false for OpusHead");

        const auto tags = micro_opus_test::make_opus_tags();
        check(micro_opus::is_opus_tags(tags.data(), tags.size()), "is_opus_tags true for OpusTags");
        check(!micro_opus::is_opus_head(tags.data(), tags.size()),
              "is_opus_head false for OpusTags");

        const uint8_t tiny[4] = {'O', 'p', 'u', 's'};
        check(!micro_opus::is_opus_head(tiny, sizeof(tiny)), "is_opus_head false when too short");
    }

    // --- Family 0 mono: field decode and derived stream/coupled counts ---
    {
        auto head = make_opus_head_family0(1, /*pre_skip=*/312, /*input_sample_rate=*/44100);
        OpusHead parsed{};
        check(parse_opus_head(head.data(), head.size(), parsed) == micro_opus::OPUS_HEADER_OK,
              "family0 mono parses OK");
        check(parsed.version == 1, "version == 1");
        check(parsed.channel_count == 1, "channel_count == 1");
        check(parsed.pre_skip == 312, "pre_skip decoded");
        check(parsed.input_sample_rate == 44100, "input_sample_rate decoded");
        check(parsed.channel_mapping == 0, "mapping family 0");
        check(parsed.stream_count == 1, "family0 stream_count derived to 1");
        check(parsed.coupled_count == 0, "family0 mono coupled_count 0");
    }

    // --- Family 0 stereo: coupled_count derives to 1 ---
    {
        auto head = make_opus_head_family0(2);
        OpusHead parsed{};
        check(parse_opus_head(head.data(), head.size(), parsed) == micro_opus::OPUS_HEADER_OK,
              "family0 stereo parses OK");
        check(parsed.coupled_count == 1, "family0 stereo coupled_count 1");
    }

    // --- Negative output gain (Q7.8, signed) round-trips through int16 ---
    {
        auto head = make_opus_head_family0(2);
        head[16] = 0x00;  // Output gain LSB (offset = magic(8) + 8)
        head[17] = 0xFF;  // Output gain MSB -> 0xFF00 = -256
        OpusHead parsed{};
        check(parse_opus_head(head.data(), head.size(), parsed) == micro_opus::OPUS_HEADER_OK,
              "header with gain parses OK");
        check(parsed.output_gain == -256, "negative output_gain decoded as signed");
    }

    // --- Error paths ---
    {
        auto bad_magic = make_opus_head_family0(2);
        bad_magic[0] = 'X';
        expect_result("bad magic", bad_magic, micro_opus::OPUS_HEADER_INVALID_MAGIC);

        auto bad_version = make_opus_head_family0(2);
        bad_version[8] = 2;  // version field (offset = magic(8) + 0)
        expect_result("bad version", bad_version, micro_opus::OPUS_HEADER_INVALID_VERSION);

        auto too_short = make_opus_head_family0(2);
        too_short.resize(18);  // One byte under the 19-byte family-0 minimum
        expect_result("too short", too_short, micro_opus::OPUS_HEADER_TOO_SHORT);

        auto zero_channels = make_opus_head_family0(2);
        zero_channels[9] = 0;  // channel_count field (offset = magic(8) + 1)
        expect_result("zero channels", zero_channels, micro_opus::OPUS_HEADER_INVALID_CHANNELS);

        auto family0_three = make_opus_head_family0(3);  // family 0 allows only 1-2 channels
        expect_result("family0 > 2 channels", family0_three,
                      micro_opus::OPUS_HEADER_INVALID_CHANNELS);
    }

    // --- Family 1 happy paths ---
    {
        auto surround =
            make_opus_head_family1(3, /*stream_count=*/2, /*coupled_count=*/1, {0, 1, 2});
        OpusHead parsed{};
        check(
            parse_opus_head(surround.data(), surround.size(), parsed) == micro_opus::OPUS_HEADER_OK,
            "family1 3ch parses OK");
        check(parsed.stream_count == 2, "family1 stream_count parsed");
        check(parsed.coupled_count == 1, "family1 coupled_count parsed");

        auto silent = make_opus_head_family1(3, 1, 1, {0, 1, 255});  // 255 = silent channel
        expect_result("family1 silent channel (255)", silent, micro_opus::OPUS_HEADER_OK);
    }

    // --- Family 1 error paths ---
    {
        auto short_table = make_opus_head_family1(3, 2, 1, {0, 1, 2});
        short_table.resize(short_table.size() - 1);  // Drop a mapping-table byte
        expect_result("family1 truncated mapping table", short_table,
                      micro_opus::OPUS_HEADER_TOO_SHORT);

        auto bad_index = make_opus_head_family1(3, 1, 1, {0, 1, 5});  // 5 >= stream+coupled (2)
        expect_result("family1 mapping index out of range", bad_index,
                      micro_opus::OPUS_HEADER_INVALID_MAPPING);

        auto zero_streams = make_opus_head_family1(2, 0, 0, {255, 255});  // passes table, 0 streams
        expect_result("family1 stream_count 0", zero_streams,
                      micro_opus::OPUS_HEADER_INVALID_MAPPING);

        auto coupled_gt_stream = make_opus_head_family1(2, 1, 2, {0, 1});  // coupled > stream
        expect_result("family1 coupled_count > stream_count", coupled_gt_stream,
                      micro_opus::OPUS_HEADER_INVALID_MAPPING);

        auto too_many = make_opus_head_family1(9, 9, 0, {0, 1, 2, 3, 4, 5, 6, 7, 8});  // > 8 ch
        expect_result("family1 > 8 channels", too_many, micro_opus::OPUS_HEADER_INVALID_CHANNELS);
    }

    if (g_failures == 0) {
        std::printf("PASS: all checks passed\n");
        return 0;
    }
    std::printf("FAILED: %d check(s)\n", g_failures);
    return 1;
}

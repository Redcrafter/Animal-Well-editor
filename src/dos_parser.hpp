#pragma once

#include <cstdint>
#include <span>

struct SegmentData {
    std::span<uint8_t> data;
    uint64_t rdata_pointer_offset;
    std::span<uint8_t> rdata;
};

SegmentData getSegmentOffsets(std::span<char> data);

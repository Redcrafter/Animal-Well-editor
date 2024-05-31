#pragma once

#include <cstdint>
#include <span>

struct SegmentData {
    uint32_t data_virt_addr;
    std::span<uint8_t> data;

    uint32_t rdata_virt_addr;
    std::span<uint8_t> rdata;

    uint64_t image_base;

    std::span<uint8_t> get_data_ptr(uint64_t ptr, uint64_t length = -1) {
         // assert(sections.rdata.size() >= asset.ptr - sections.rdata_pointer_offset + asset.length);
         return data.subspan(ptr - image_base - data_virt_addr, length);
    }
    std::span<uint8_t> get_rdata_ptr(uint64_t ptr, uint64_t length = -1) {
        // assert(sections.rdata.size() >= asset.ptr - sections.rdata_pointer_offset + asset.length);
        return rdata.subspan(ptr - image_base - rdata_virt_addr, length);
    }
};

SegmentData getSegmentOffsets(std::span<char> data);

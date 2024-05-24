#pragma once

#include <cstdint>
#include <span>
#include <vector>

std::vector<uint8_t> encrypt(std::span<const uint8_t> data, const std::array<uint8_t, 16>& key);

bool decrypt(std::span<const uint8_t> data, const std::array<uint8_t, 16>& key, std::vector<uint8_t>& out);

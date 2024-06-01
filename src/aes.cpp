#include "aes.hpp"

#include <array>
#include <random>

#include <immintrin.h>

// see https://www.intel.com/content/dam/doc/white-paper/advanced-encryption-standard-new-instructions-set-paper.pdf

__m128i AES_128_ASSIST(__m128i temp1, __m128i temp2) {
    __m128i temp3;
    temp2 = _mm_shuffle_epi32(temp2, 0xff);
    temp3 = _mm_slli_si128(temp1, 0x4);
    temp1 = _mm_xor_si128(temp1, temp3);
    temp3 = _mm_slli_si128(temp3, 0x4);
    temp1 = _mm_xor_si128(temp1, temp3);
    temp3 = _mm_slli_si128(temp3, 0x4);
    temp1 = _mm_xor_si128(temp1, temp3);
    temp1 = _mm_xor_si128(temp1, temp2);
    return temp1;
}

auto expandKey(const std::array<uint8_t, 16>& key) {
    std::array<__m128i, 11> Key_Schedule;

    // normal key expansion
    auto temp1 = _mm_loadu_si128((__m128i*)key.data());
    Key_Schedule[0] = temp1;
    temp1 = AES_128_ASSIST(temp1, _mm_aeskeygenassist_si128(temp1, 0x1));
    Key_Schedule[1] = temp1;
    temp1 = AES_128_ASSIST(temp1, _mm_aeskeygenassist_si128(temp1, 0x2));
    Key_Schedule[2] = temp1;
    temp1 = AES_128_ASSIST(temp1, _mm_aeskeygenassist_si128(temp1, 0x4));
    Key_Schedule[3] = temp1;
    temp1 = AES_128_ASSIST(temp1, _mm_aeskeygenassist_si128(temp1, 0x8));
    Key_Schedule[4] = temp1;
    temp1 = AES_128_ASSIST(temp1, _mm_aeskeygenassist_si128(temp1, 0x10));
    Key_Schedule[5] = temp1;
    temp1 = AES_128_ASSIST(temp1, _mm_aeskeygenassist_si128(temp1, 0x20));
    Key_Schedule[6] = temp1;
    temp1 = AES_128_ASSIST(temp1, _mm_aeskeygenassist_si128(temp1, 0x40));
    Key_Schedule[7] = temp1;
    temp1 = AES_128_ASSIST(temp1, _mm_aeskeygenassist_si128(temp1, 0x80));
    Key_Schedule[8] = temp1;
    temp1 = AES_128_ASSIST(temp1, _mm_aeskeygenassist_si128(temp1, 0x1b));
    Key_Schedule[9] = temp1;
    temp1 = AES_128_ASSIST(temp1, _mm_aeskeygenassist_si128(temp1, 0x36));
    Key_Schedule[10] = temp1;

    // the game uses a shuffled key
    __m128i mask = _mm_set_epi8(15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0);
    for(int i = 0; i < 11; ++i) {
        Key_Schedule[i] = _mm_shuffle_epi8(Key_Schedule[i], mask);
    }

    return Key_Schedule;
}

#ifdef _MSC_VER
inline __m128i operator^(__m128i a, __m128i b) {
    return _mm_xor_si128(a, b);
}
#endif

inline bool eq(__m128i a, __m128i b) {
    auto v = a ^ b;
    return _mm_test_all_zeros(v, v);
}

std::vector<uint8_t> encrypt(std::span<const uint8_t> data, const std::array<uint8_t, 16>& key) {
    auto _key = expandKey(key);

    auto length = data.size();
    auto data_ = (__m128i*)data.data();

    std::vector<uint8_t> out((length & (~0xF)) + ((length & 0xF) != 0) * 16 + 0x20);
    auto out_ptr = (__m128i*)out.data();

    std::random_device rd;
    std::default_random_engine re(rd());
    std::uniform_int_distribution<uint32_t> dist;

    std::array<uint8_t, 16> iv;
    for(size_t i = 0; i < 16; i++) {
        iv[i] = dist(re);
    }

    *out_ptr = *(__m128i*)iv.data();
    out_ptr++;

    auto step = [&](__m128i val) {
        val = val ^ _key[0];
        for(int i = 1; i < 10; i++) {
            val = _mm_aesenc_si128(val, _key[i]);
        }
        *out_ptr = _mm_aesenclast_si128(val, _key[10]);
        out_ptr++;
    };

    step(_key[0] ^ *(out_ptr - 1));

    for(size_t i = 0; i < (length >> 4); i++) {
        step(data_[i] ^ *(out_ptr - 1));
    }
    if((length & 0xF) != 0) {
        std::array<uint8_t, 16> buf {0};
        for(size_t i = 0; i < (length & 0xF); ++i) {
            buf[i] = data[(length & (~0xF)) + i];
        }
        step(*(__m128i*)buf.data() ^ *(out_ptr - 1));
    }

    return out;
}

bool decrypt(std::span<const uint8_t> data, const std::array<uint8_t, 16>& key, std::vector<uint8_t>& out) {
    auto _key = expandKey(key);

    __m128i inv_key[9];
    for(int i = 0; i < 9; ++i) {
        inv_key[i] = _mm_aesimc_si128(_key[i + 1]);
    }

    auto step = [&](__m128i val) {
        val = val ^ _key[10];
        for(int i = 9; i > 0; i--) {
            val = _mm_aesdec_si128(val, inv_key[i - 1]);
        }
        return _mm_aesdeclast_si128(val, _key[0]);
    };

    auto data_ = (__m128i*)data.data();
    // first 16 bytes of decrypted data should be key again
    if(!eq((step(data_[1]) ^ data_[0]), _key[0]))
        return false;

    out.resize(data.size() - 0x20);
    auto out_ptr = (__m128i*)out.data();

    auto len = ((data.size() & 0xf) != 0) + ((data.size() - 0x10) >> 4);
    for(size_t i = 1; i < len; i += 1) {
        out_ptr[i - 1] = step(data_[i + 1]) ^ data_[i];
    }

    return true;
}

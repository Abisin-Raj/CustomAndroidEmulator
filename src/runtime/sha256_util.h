#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <sstream>
#include <iomanip>

namespace emulator::runtime {

namespace detail {

struct Sha256Ctx {
    uint32_t state[8];
    uint64_t bitCount;
    uint8_t buf[64];
    size_t bufLen;
};

static const uint32_t kSha256K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static inline uint32_t Sha256Rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

static inline void Sha256Block(Sha256Ctx& ctx, const uint8_t* block) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (uint32_t(block[i*4]) << 24) | (uint32_t(block[i*4+1]) << 16) |
               (uint32_t(block[i*4+2]) << 8) | uint32_t(block[i*4+3]);
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = Sha256Rotr(w[i-15], 7) ^ Sha256Rotr(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = Sha256Rotr(w[i-2], 17) ^ Sha256Rotr(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a = ctx.state[0], b = ctx.state[1], c = ctx.state[2], d = ctx.state[3];
    uint32_t e = ctx.state[4], f = ctx.state[5], g = ctx.state[6], h = ctx.state[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = Sha256Rotr(e, 6) ^ Sha256Rotr(e, 11) ^ Sha256Rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + kSha256K[i] + w[i];
        uint32_t S0 = Sha256Rotr(a, 2) ^ Sha256Rotr(a, 13) ^ Sha256Rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    ctx.state[0] += a; ctx.state[1] += b; ctx.state[2] += c; ctx.state[3] += d;
    ctx.state[4] += e; ctx.state[5] += f; ctx.state[6] += g; ctx.state[7] += h;
}

} // namespace detail

inline std::string ComputeSha256(const uint8_t* data, size_t len) {
    detail::Sha256Ctx ctx{};
    ctx.state[0] = 0x6a09e667; ctx.state[1] = 0xbb67ae85;
    ctx.state[2] = 0x3c6ef372; ctx.state[3] = 0xa54ff53a;
    ctx.state[4] = 0x510e527f; ctx.state[5] = 0x9b05688c;
    ctx.state[6] = 0x1f83d9ab; ctx.state[7] = 0x5be0cd19;

    for (size_t i = 0; i < len; ++i) {
        ctx.buf[ctx.bufLen++] = data[i];
        ctx.bitCount += 8;
        if (ctx.bufLen == 64) {
            detail::Sha256Block(ctx, ctx.buf);
            ctx.bufLen = 0;
        }
    }
    ctx.buf[ctx.bufLen++] = 0x80;
    if (ctx.bufLen > 56) {
        while (ctx.bufLen < 64) ctx.buf[ctx.bufLen++] = 0;
        detail::Sha256Block(ctx, ctx.buf);
        ctx.bufLen = 0;
    }
    while (ctx.bufLen < 56) ctx.buf[ctx.bufLen++] = 0;
    for (int i = 7; i >= 0; --i) {
        ctx.buf[ctx.bufLen++] = uint8_t(ctx.bitCount >> (i * 8));
    }
    detail::Sha256Block(ctx, ctx.buf);

    std::ostringstream oss;
    for (int i = 0; i < 8; ++i) {
        oss << std::hex << std::setfill('0') << std::setw(8) << ctx.state[i];
    }
    return oss.str();
}

} // namespace emulator::runtime


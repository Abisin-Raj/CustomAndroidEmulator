#include "image_converter/dat_image_builder.h"
#include "image_converter/transfer_list_parser.h"
#include <array>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

// ---------------------------------------------------------------------------
// Minimal SHA-256 implementation (no external dependency)
// ---------------------------------------------------------------------------
namespace {

struct Sha256Ctx {
    uint32_t state[8];
    uint64_t bitCount;
    uint8_t  buf[64];
    uint32_t bufLen;
};

static const uint32_t kK[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

static void sha256Block(Sha256Ctx& ctx, const uint8_t* block) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (uint32_t(block[i*4])<<24)|(uint32_t(block[i*4+1])<<16)|
               (uint32_t(block[i*4+2])<<8)|uint32_t(block[i*4+3]);
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1 = rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10);
        w[i] = w[i-16]+s0+w[i-7]+s1;
    }
    uint32_t a=ctx.state[0],b=ctx.state[1],c=ctx.state[2],d=ctx.state[3];
    uint32_t e=ctx.state[4],f=ctx.state[5],g=ctx.state[6],h=ctx.state[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1=rotr(e,6)^rotr(e,11)^rotr(e,25);
        uint32_t ch=(e&f)^(~e&g);
        uint32_t t1=h+S1+ch+kK[i]+w[i];
        uint32_t S0=rotr(a,2)^rotr(a,13)^rotr(a,22);
        uint32_t maj=(a&b)^(a&c)^(b&c);
        uint32_t t2=S0+maj;
        h=g; g=f; f=e; e=d+t1;
        d=c; c=b; b=a; a=t1+t2;
    }
    ctx.state[0]+=a; ctx.state[1]+=b; ctx.state[2]+=c; ctx.state[3]+=d;
    ctx.state[4]+=e; ctx.state[5]+=f; ctx.state[6]+=g; ctx.state[7]+=h;
}

static Sha256Ctx sha256Init() {
    Sha256Ctx ctx{};
    ctx.state[0]=0x6a09e667; ctx.state[1]=0xbb67ae85;
    ctx.state[2]=0x3c6ef372; ctx.state[3]=0xa54ff53a;
    ctx.state[4]=0x510e527f; ctx.state[5]=0x9b05688c;
    ctx.state[6]=0x1f83d9ab; ctx.state[7]=0x5be0cd19;
    return ctx;
}

static void sha256Update(Sha256Ctx& ctx, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        ctx.buf[ctx.bufLen++] = data[i];
        ctx.bitCount += 8;
        if (ctx.bufLen == 64) { sha256Block(ctx, ctx.buf); ctx.bufLen = 0; }
    }
}

static std::string sha256Final(Sha256Ctx& ctx) {
    ctx.buf[ctx.bufLen++] = 0x80;
    if (ctx.bufLen > 56) {
        while (ctx.bufLen < 64) ctx.buf[ctx.bufLen++] = 0;
        sha256Block(ctx, ctx.buf); ctx.bufLen = 0;
    }
    while (ctx.bufLen < 56) ctx.buf[ctx.bufLen++] = 0;
    for (int i = 7; i >= 0; --i) ctx.buf[ctx.bufLen++] = uint8_t(ctx.bitCount >> (i*8));
    sha256Block(ctx, ctx.buf);
    std::ostringstream oss;
    for (int i = 0; i < 8; ++i)
        oss << std::hex << std::setfill('0') << std::setw(8) << ctx.state[i];
    return oss.str();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// DatImageBuilder::build
// ---------------------------------------------------------------------------
bool DatImageBuilder::build(const std::filesystem::path& datFile,
                            const std::filesystem::path& transferListFile,
                            const std::filesystem::path& outputImage,
                            std::function<void(float)>   progressCb,
                            std::string*                 errorOut)
{
    TransferList tl;
    try {
        tl = TransferListParser::parse(transferListFile);
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = std::string("Transfer list parse error: ") + ex.what();
        return false;
    }

    // Pre-allocate output file (sparse write)
    const uint64_t imageBytes = tl.totalBlocks * kBlockSize;
    std::ofstream out(outputImage, std::ios::binary);
    if (!out) {
        if (errorOut) *errorOut = "Cannot create output image: " + outputImage.string();
        return false;
    }
    // Seek to end to establish file size without writing zeros
    out.seekp(static_cast<std::streamoff>(imageBytes - 1));
    out.put('\0');
    out.seekp(0);

    std::ifstream dat(datFile, std::ios::binary);
    if (!dat) {
        if (errorOut) *errorOut = "Cannot open .new.dat: " + datFile.string();
        return false;
    }

    std::vector<uint8_t> block(kBlockSize);
    uint64_t totalNewBlocks = 0;

    // Count total "new" blocks for progress
    for (const auto& cmd : tl.commands) {
        if (cmd.op == TransferCommand::Op::New) {
            for (const auto& [s, e] : cmd.ranges) totalNewBlocks += (e - s);
        }
    }

    uint64_t writtenBlocks = 0;

    for (const auto& cmd : tl.commands) {
        if (cmd.op == TransferCommand::Op::New) {
            for (const auto& [start, end] : cmd.ranges) {
                for (uint64_t blk = start; blk < end; ++blk) {
                    dat.read(reinterpret_cast<char*>(block.data()), kBlockSize);
                    if (dat.gcount() != kBlockSize) {
                        if (errorOut) *errorOut = "Unexpected end of .new.dat at block " + std::to_string(blk);
                        return false;
                    }
                    out.seekp(static_cast<std::streamoff>(blk * kBlockSize));
                    out.write(reinterpret_cast<const char*>(block.data()), kBlockSize);
                    ++writtenBlocks;
                    if (progressCb && totalNewBlocks > 0) {
                        progressCb(static_cast<float>(writtenBlocks) / static_cast<float>(totalNewBlocks));
                    }
                }
            }
        } else if (cmd.op == TransferCommand::Op::Zero || cmd.op == TransferCommand::Op::Erase) {
            std::fill(block.begin(), block.end(), 0);
            for (const auto& [start, end] : cmd.ranges) {
                for (uint64_t blk = start; blk < end; ++blk) {
                    out.seekp(static_cast<std::streamoff>(blk * kBlockSize));
                    out.write(reinterpret_cast<const char*>(block.data()), kBlockSize);
                }
            }
        }
        // Stash/Free commands: not needed for simple reconstruction (no bsdiff/imgdiff)
    }

    if (progressCb) progressCb(1.0f);
    return true;
}

// ---------------------------------------------------------------------------
// DatImageBuilder::verify
// ---------------------------------------------------------------------------
ImageVerificationResult DatImageBuilder::verify(const std::filesystem::path& imagePath)
{
    ImageVerificationResult result;

    std::ifstream f(imagePath, std::ios::binary | std::ios::ate);
    if (!f) return result;

    result.sizeBytes = static_cast<uint64_t>(f.tellg());
    f.seekg(0);

    // ext4 superblock magic is 0x53EF at byte offset 1080 (0x438)
    if (result.sizeBytes >= 1082) {
        f.seekg(1080);
        uint8_t magic[2] = {};
        f.read(reinterpret_cast<char*>(magic), 2);
        result.isValidExt4 = (magic[0] == 0x53 && magic[1] == 0xEF);
        f.seekg(0);
    }

    // Stream SHA-256 in 64 KiB chunks
    auto ctx = sha256Init();
    std::vector<uint8_t> buf(65536);
    while (f) {
        f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        std::streamsize got = f.gcount();
        if (got > 0) sha256Update(ctx, buf.data(), static_cast<size_t>(got));
    }
    result.sha256 = sha256Final(ctx);

    return result;
}

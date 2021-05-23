#include <iostream>
#include <vector>
#include <fstream>

char __fastcall decompress_rpak(__int64* a1, unsigned __int64 a2, unsigned __int64 a3);

constexpr bool TF2 = true; // or define?

constexpr size_t HEADER_SIZE = TF2 ? 88 : 0x80; // what? might be related to major version shit

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

#define WORDSWAP( X ) ((X>>8) | ((X&0xFF)<<8))

#pragma push(pack, 1)
struct rpack_hdr {
    u32 magic; // should be 'kaPR' aka "RPak"
    struct {
        u16 major; // 7 for TF|2, 8 for Pepex
        u16 minor; // BE, 1?
    } version;

    u64 type; // ?
    u64 pad0; // ?

    u64 size_disk;
    u64 pad1; // ?
    
    u64 size_decomp;

    char padd[40];
};

static_assert(sizeof(rpack_hdr) == HEADER_SIZE, "Header size missmatch!");
#pragma pop(pack)

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::printf("Usage:\n%s PAK\n", argv[0]);
        return -1;
    }

    std::printf("Working on: %s\n", argv[1]);

    std::string file_name = argv[1];

    std::vector<uint8_t> input;

    // resize and read
    {
        std::ifstream ifile(file_name, std::fstream::binary);
        ifile.seekg(0, std::fstream::end);
        input.resize(ifile.tellg());
        ifile.seekg(0, std::fstream::beg);
        ifile.read((char*)input.data(), input.size());
    }

    auto pak_hdr = (rpack_hdr*)input.data();
    if (pak_hdr->magic != 'kaPR') {
        std::printf("[error] RPak %s has invalid magic!", argv[1]);
        return -1;
    }
    if (pak_hdr->size_disk != input.size()) {
        std::printf("[assert-y] RPak %s has invalid on disk size!", argv[1]);
        return -1;
    }
    // debug print header
    {
        std::printf("Magic: %08X\n", pak_hdr->magic);
        std::printf("Version: %u.%u\n", int(pak_hdr->version.major), WORDSWAP(pak_hdr->version.minor));
        // ---
        std::printf("\nFile size on disk: %lld\nFile size decompressed: %lld\n", pak_hdr->size_disk, pak_hdr->size_decomp);
        std::printf("Compress ratio: %.02f\n", (pak_hdr->size_disk*100.f)/pak_hdr->size_decomp);
    }
}

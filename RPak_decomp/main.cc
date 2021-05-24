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

#pragma push(pack, 1)

// confirmed destruction from sub_18000A030 aka read_package_file_start_qm
struct rpack_hdr {
    u32 magic; // should be 'kaPR' aka "RPak"
    u16 version; // // 7 for TF|2, 8 for Pepex | if ( *(_WORD *)&v43[4] != 7 )
    union {
        struct {
            union {
                u8 unk6;
                struct {
                    u8 should_lla : 2; // ??? if ( (v43[6] & 3) != 0 ) aka 0b11
                    u8 unk6_3 : 6;
                };
            };
            u8 is_compressed; // ??? if ( (*(_WORD *)&v43[6] & 0x100) != 0 )
        } flag;
        u16 flags;
    };

    u64 type; // ? +0x8
    u64 unk10; // ?

    u64 size_disk; // +0x18
    u64 unk20; // ?
    
    u64 size_decomp; // +0x28

    u64 unk30; // pad

    u16 unk38; // v24 = *(unsigned __int16 *)&v43[0x38] + ...
    u16 unk3a; // v23 = *(unsigned __int16 *)&v43[0x3A];
    u16 unk3c; // v22 = *(unsigned __int16 *)&v43[0x3C];
    u16 unk3e; // v20 = *(unsigned __int16 *)&v43[0x3E]; | + ((_WORD)v20 != 0 ? 8 : 0) | * (v20 + ...

    u32 unk40;
    u32 unk44; // v18 = 8i64 * *(unsigned int *)&v43[0x44]; *((_DWORD *)v2 + 3) = *(_DWORD *)&v43[0x44];
    u32 unk48;
    u32 unk4c;

    u32 unk50;
    u32 unk54; // + *(unsigned int *)&v43[0x54]
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
        std::printf("Version: %u | flags word %u\n", int(pak_hdr->version), int(pak_hdr->flags));
        // --
        std::printf("Flags set: ");
        if ((pak_hdr->flag.is_compressed & 1) == 1) { // yes, that specific... (*(_WORD *)&v43[6] & 0x100) != 0
            std::printf("COMPRESSED? ");
        }
        if (pak_hdr->flag.should_lla) { // (v43[6] & 0b11) != 0
            std::printf("LLA?\n");
        }
        else {
            std::putc('\n', stdout);
        }
        // ---
        std::printf("\nFile size on disk: %lld\nFile size decompressed: %lld\n", pak_hdr->size_disk, pak_hdr->size_decomp);
        std::printf("Compress ratio: %.02f\n", (pak_hdr->size_disk*100.f)/pak_hdr->size_decomp);
    }
}

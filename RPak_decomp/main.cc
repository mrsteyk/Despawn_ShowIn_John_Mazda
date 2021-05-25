#include <iostream>
#include <vector>
#include <fstream>

#include <cinttypes>

char __fastcall decompress_rpak(__int64* a1, unsigned __int64 a2, unsigned __int64 a3);
__int64 __fastcall get_decompressed_size(__int64 params, uint8_t* file_buf, __int64 some_magic_shit, __int64 file_size, __int64 off_without_header_qm, __int64 header_size);

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

    u16 skip_shit; // how much to skip for actual asset entries | v24 = *(unsigned __int16 *)&v43[0x38] + ...
    u16 internal_shit_size; // internal_shit_size, has base?, align_byte, size_unaligned | v23 = *(unsigned __int16 *)&v43[0x3A];
    u16 unk3c; // 12 byte struct | v22 = *(unsigned __int16 *)&v43[0x3C];
    u16 skip_16; // skip 16 bytes of that, v20 = *(unsigned __int16 *)&v43[0x3E]; | + ((_WORD)v20 != 0 ? 8 : 0) | * (v20 + ...

    u32 unk40;
    u32 num_file_entries; // seems to match what legion tells or some other name but premise is the same... | v18 = 8i64 * *(unsigned int *)&v43[0x44]; *((_DWORD *)v2 + 3) = *(_DWORD *)&v43[0x44];
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
        std::printf("Usage:\n%s PAK writeDecompressed[0/1]\n", argv[0]);
        return -1;
    }

    std::printf("Working on: %s\n", argv[1]);

    const std::string file_name = argv[1];

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
        std::printf("Compress ratio (disk/mem): %.02f\n", (pak_hdr->size_disk*100.f)/pak_hdr->size_decomp);
        // ---
        std::printf("Type? %016" PRIx64 "\n", pak_hdr->type);
        // ---
        std::printf("Number of files inside?: %u\n", pak_hdr->num_file_entries);
        std::printf("Internal shit size (struct of 16 bytes, max 4): %u\n", pak_hdr->internal_shit_size);
        if (pak_hdr->internal_shit_size > 4) {
            std::printf("[assert-y] Such file shouldn't exist????\n\n");
            return -2;
        }
    }

    std::puts("\n---\n");

    std::vector<uint8_t> rpak_data;
    if ((pak_hdr->flag.is_compressed & 1) != 1) {
        std::printf("!!! RPak is uncompressed !!!\n");
        rpak_data = input;
    } else
    // actual decompression based on decomp from IDA
    {
        // __int64 v9[18]; from sub_180004B00 aka lemme not do LTO properly?..
        u64 parameters[18];
        auto dsize = get_decompressed_size(u64(parameters), input.data(), -1i64, input.size(), 0, HEADER_SIZE);
        if (dsize != pak_hdr->size_decomp) {
            std::printf("DSize was %llu expected %llu\n", dsize, pak_hdr->size_decomp);
        }
        else {
            std::printf("DSize is %llu\n", dsize);
        }

        std::vector<uint8_t> decompress_buffer(pak_hdr->size_decomp, 0); //(0x400000, 0);

        parameters[1] = u64(decompress_buffer.data());
        parameters[3] = -1i64;
        auto dret = decompress_rpak((long long*)parameters, input.size(), decompress_buffer.size());
        std::printf("If you see this decompression was gucci? %d %llu\n", +dret, parameters[5]);

        if (dret != 1) {
            std::printf("DRet was %d!\n", +dret);
            return -1;
        }

        if (argc > 2 && argv[2][0] == '1') {
            const auto out_fname = file_name + ".raw";
            std::ofstream out(out_fname, std::fstream::binary);
            out.write((char*)decompress_buffer.data(), parameters[5]);
            std::printf("Wrote unpacked file to %s\n", out_fname.c_str());
        }

        rpak_data = std::move(decompress_buffer); // avoid copy?

        std::puts("\n---\n");
    }

    // actual parse
    {
        std::printf("skip_16: %u\n", int(pak_hdr->skip_16));
        if (pak_hdr->skip_16 != 0) {
            std::printf("[assert-y] skip_16 wasn't 0!\n");
            return -2;
        }

        // we speak offsets here...
        auto starpak_name = HEADER_SIZE + (16ui64 * pak_hdr->skip_16); // TODO: figure out why game has such weird logic and wtf is this...
        std::printf("StarPak is %s\n", rpak_data.data() + starpak_name);

        auto second_starpak_qm = starpak_name + (2ui64 * pak_hdr->skip_16); // WHAT ON FUCKING EARTH IS THIS
        std::printf("2nd? StarPak is %s\n", rpak_data.data() + second_starpak_qm);

        auto internal_start = second_starpak_qm + pak_hdr->skip_shit;

        // Internal parser
        {
            class InternalBufferShit
            {
            public:
                uint32_t base_qm; //0x0000
                uint32_t align_byte; //0x0004
                uint64_t size_unaligned; //0x0008
            }; //Size: 0x0010

            auto internals = (InternalBufferShit*)(rpak_data.data() + internal_start);
            for (int i = 0; i < pak_hdr->internal_shit_size; i++) {
                std::printf("Internal shit [%d] : base_qm %08X | align_byte %08X | size_unaligned %p\n", i, internals[i].base_qm, internals[i].align_byte, internals[i].size_unaligned);
            }
        }

        auto internal_shit_skipped = internal_start + (16ui64 * pak_hdr->internal_shit_size);

        // TODO: parse unk3c's struct
        // TODO: 12 bytes struct

        auto unk3c_skipped = internal_shit_skipped + (12ui64 * pak_hdr->unk3c);

        // TODO: parse unk40's struct
        // TODO: 8 bytes struct

        auto unk40_skipped = unk3c_skipped + (8ui64 * pak_hdr->unk40);

        // parse file entries
        {
            class FileEntryShit
            {
            public:
                uint64_t hash; //0x0000
                char pad_0008[8]; //0x0008
                uint32_t array_idx; //0x0010
                uint32_t array_entry_offset; //0x0014
                uint32_t N00000470; //0x0018
                uint32_t N00000AD1; //0x001C
                char pad_0020[8]; //0x0020
                uint16_t N00000472; //0x0028
                uint16_t N000005EC; //0x002A
                char pad_002C[4]; //0x002C
                uint32_t start_idx; //0x0030
                char pad_0034[4]; //0x0034
                uint32_t count; //0x0038
                char pad_003C[8]; //0x003C
                char short_name[4]; //0x0044 // erm...
            }; //Size: 0x0048

            auto files = (FileEntryShit*)(rpak_data.data() + unk40_skipped);
            for (int i = 0; i < pak_hdr->num_file_entries; i++) {
                const auto& file = files[i];
                
                auto hash = file.hash;
                u32 short_name[2]{ *(u32*)file.short_name, 0 };
                std::printf("File[%03d]: hash %p | type %4s\n", i, hash, short_name);
            }
        }

        auto file_entries_skipped = unk40_skipped + (72ui64 * pak_hdr->num_file_entries);

        std::printf("Left @ %p\n", file_entries_skipped);
    }
}

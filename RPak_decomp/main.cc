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
            //return -2;
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
        auto starpak_other = HEADER_SIZE;
        std::printf("StarPak is      \"%s\"\n", rpak_data.data() + starpak_other);

        auto starpak_name = HEADER_SIZE + (16ui64 * pak_hdr->skip_16); // TODO: figure out why game has such weird logic and wtf is this...
        std::printf("StarPak? is     \"%s\"\n", rpak_data.data() + starpak_name);

        auto second_starpak_qm = starpak_name + (2ui64 * pak_hdr->skip_16); // WHAT ON FUCKING EARTH IS THIS
        std::printf("2nd? StarPak is \"%s\"\n", rpak_data.data() + second_starpak_qm);

        auto internal_start = second_starpak_qm + pak_hdr->skip_shit;

        // Internal parser
        class InternalBufferShit
        {
        public:
            uint32_t base_qm; //0x0000
            uint32_t align_byte; //0x0004
            uint64_t size_unaligned; //0x0008
        }; //Size: 0x0010
        auto internals = (InternalBufferShit*)(rpak_data.data() + internal_start);
        {
            for (int i = 0; i < pak_hdr->internal_shit_size; i++) {
                auto idkwhat = internals[i].base_qm & 3;
                std::printf("Internal shit [%d] : base_qm %08X(%u) | align_byte %08X | size_unaligned %p\n", i, internals[i].base_qm, idkwhat, internals[i].align_byte, internals[i].size_unaligned);
                /*if (idkwhat)
                {
                    auto v21 = internals[i].align_byte; //*(unsigned int*)(v19 + 4);
                    auto v22 = idkwhat;
                    auto v23 = v58[idkwhat];
                    auto v24 = ~(v21 - 1) & (v21 - 1 + v59[v22]);
                    v60[i] = v24;
                    auto v25 = internals[i].size_unaligned + v24;
                    if (v23 < (unsigned int)v21)
                        v23 = v21;
                    v59[v22] = v25;
                    v58[v22] = v23;
                }*/
            }

            //for (size_t i = 0; i < 4; i++) {
            //    std::printf("\t %p | %08X | %08X\n", v60[i], v58[i], v59[i]);
            //}
        }

        auto internal_shit_skipped = internal_start + (16ui64 * pak_hdr->internal_shit_size);

        // TODO: parse unk3c's struct
        // TODO: 12 bytes struct
        //std::printf("TODO: parse unk3c's 12b [%llu] @ %p\n", pak_hdr->unk3c, internal_shit_skipped);
        struct unk3c_s {
            u32 idk0;
            u32 idk4;
            u32 idk8; // used in some *(_QWORD *)(v1 + 0x550) = v20[2];
        };
        auto unk3c_elems = (unk3c_s*)(rpak_data.data() + internal_shit_skipped);
        {
            for (int i = 0; i < pak_hdr->unk3c; i++) {
                const auto& e = unk3c_elems[i];
                std::printf("Unk3c[%03u]: %03u(%u) %05u %08X\n", i, e.idk0, internals[e.idk0].base_qm&3, e.idk4, e.idk8);
            }
        }

        auto unk3c_skipped = internal_shit_skipped + (12ui64 * pak_hdr->unk3c);

        // TODO: parse unk40's struct
        // TODO: 8 bytes struct
        std::printf("TODO: parse unk40's 8b [%llu] @ %p\n", pak_hdr->unk40, unk3c_skipped);

        auto unk40_skipped = unk3c_skipped + (8ui64 * pak_hdr->unk40);

        // parse file entries
        class FileEntryShit
        {
        public:
            uint64_t hash; //0x0000
            char pad_0008[8]; //0x0008
            uint32_t array_idx; //0x0010
            uint32_t array_entry_offset; //0x0014
            uint32_t unk18; //0x0018
            uint32_t unk1c; //0x001C
            //char pad_0020[8]; //0x0020
            u64 starpak_offset;
            uint16_t unk28; //0x0028
            uint16_t unk2a; //0x002A
            uint32_t unk2c; //0x002C
            uint32_t start_idx; //0x0030
            uint32_t unk34; //0x0034
            uint32_t count; //0x0038
            char pad_003C[8]; //0x003C
            char short_name[4]; //0x0044 // erm...
        }; //Size: 0x0048
        FileEntryShit* g_files;
        {
            g_files = (FileEntryShit*)(rpak_data.data() + unk40_skipped);
            for (int i = 0; i < pak_hdr->num_file_entries; i++) {
                const auto& file = g_files[i];
                
                auto hash = file.hash;
                u32 short_name[2]{ *(u32*)file.short_name, 0 };
                // 0x2a looks like how much shit's referenced...
                std::printf("File[%03d]: hash %p | type %4s | 0x2a %u | \n", i, hash, short_name, file.unk2a);
            }
        }

        auto file_entries_skipped = unk40_skipped + (72ui64 * pak_hdr->num_file_entries);

        // TODO: parse unk48's struct
        // TODO: 8 bytes struct
        //std::printf("TODO: parse unk48's 8b [%llu] @ %p\n", pak_hdr->unk48, file_entries_skipped);
        {
            struct idk {
                u32 idk1;
                u32 idk2;
            };

            auto idks = (idk*)(rpak_data.data() + file_entries_skipped);

            for (int i = 0; i < pak_hdr->unk48; i++) {
                auto& e = idks[i];
                std::printf("unk48[%03d]: %05u | %08X\n", i, e.idk1, e.idk2);
            }
        }

        auto unk48_skipped = file_entries_skipped + (8ui64 * pak_hdr->unk48);

        // TODO: parse unk4c's struct
        // TODO: 4 bytes struct
        //std::printf("TODO: parse unk4c's 4b [%llu] @ %p\n", pak_hdr->unk4c, unk48_skipped);
        {
            auto items = (u32*)(rpak_data.data() + unk48_skipped);
            /*for (int i = 0; i < pak_hdr->unk4c; i++) {
                std::printf("Unk4c Relation[%03d]: hash %p\n", i, g_files[items[i]].hash);
            }*/
            std::puts("--- Relations BEG ---");
            for (int i = 0; i < pak_hdr->num_file_entries; i++) {
                const auto& file = g_files[i];
                for (int j = 0; j < file.unk34; j++) {
                    auto file_idx = items[j + file.unk2c];

                    auto& rel_file = g_files[file_idx];
                    u32 short_name[2]{ *(u32*)rel_file.short_name, 0 };
                    std::printf("File[%03d] relates (%u/%u) to File[%03d] with hash %p type %4s\n", i, j, file.unk34, file_idx, rel_file.hash, short_name);
                }
            }
            std::puts("--- Relations END ---");
        }

        auto unk4c_skipped = unk48_skipped + (4ui64 * pak_hdr->unk4c);

        // TODO: parse unk50's struct
        // TODO: 4 bytes struct
        std::printf("TODO: parse unk50's 4b [%llu] @ %p\n", pak_hdr->unk50, unk4c_skipped);

        auto unk50_skipped = unk4c_skipped + (4ui64 * pak_hdr->unk50);

        // TODO: parse unk54's struct
        // TODO: 1 bytes struct
        std::printf("TODO: parse unk54's 1b [%llu] @ %p\n", pak_hdr->unk54, unk50_skipped);

        auto unk54_skipped = unk50_skipped + pak_hdr->unk54;

        // TODO: skip_16 not zero logic here...
        /*
        if ( pak_hdr->skip_16 ) {
            unk54_skipped = v25 + *v5;
            a2[11] = unk50_skipped;
        }
        */

        std::printf("Left @ %p | %p\n", unk54_skipped, (rpak_data.size() - unk54_skipped));

        std::printf("\nFiles start and shit...\n");
        {
            std::vector<u32> file_seeks(pak_hdr->unk3c);

            auto tmp_skip = unk54_skipped;
            for (int i = 0; i < pak_hdr->unk3c; i++) {
                const auto& e = unk3c_elems[i];
                file_seeks[i] = tmp_skip;
                std::printf("Data[%03d]@%p: type %u? | size %p | align? %02X\n", i, tmp_skip, e.idk0, e.idk8, e.idk4);

                tmp_skip += e.idk8;
            }

            std::printf("Debug info: g_files[%u] unk3c_elems[%u]\n", pak_hdr->num_file_entries, pak_hdr->unk3c);

            {
                char k0k[260];
                for (int i = 0; i < pak_hdr->num_file_entries; i++) {
                    const auto& file = g_files[i];
                    u32 short_name[2]{ *(u32*)file.short_name, 0 };

                    std::printf("File[%03u](%03i) %4s ", i, file.unk18, short_name);
                    if (file.unk18 == 0xFFFFFFFF) {
                        std::putchar('\n');
                        continue;
                    }

                    auto offset = file_seeks[file.unk18] + file.unk1c;
                    auto size = unk3c_elems[file.unk18].idk8;

                    std::printf("Writing/dry %03d off %p size %p...\n", i, offset, size);
                    sprintf_s(k0k, "%s.%03d.0x%p.%s", argv[1], i, file.hash, short_name);
                    
                    if (argc > 2 && argv[2][0] == '1') {
                        std::ofstream outf; // (k0k, std::fstream::binary);
                        // texture
                        if (short_name[0] == 0x72747874) {
                            class TextureLoadA1
                            {
                            public:
                                uint64_t hash; //0x0000
                                char* texture_name; //0x0008 // name IDX in reality...
                                uint16_t width; //0x0010
                                uint16_t height; //0x0012
                                char pad_0014[2]; //0x0014
                                uint16_t texture_type; //0x0016
                                char pad_0018[9]; //0x0018
                                uint8_t mipmaps; //0x0021
                                uint8_t unk22; //0x0022
                            }; //Size: 0x012C
                            uint8_t dds_header[]{
                                0x44, 0x44, 0x53, 0x20, 0x7C, 0x00, 0x00, 0x00, 0x07, 0x10, 0x0A, 0x00, 0x38, 0x04, 0x00, 0x00, 0x80, 0x07, 0x00, 0x00, 0x00, 0xD2, 0x0F, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x44, 0x58, 0x54, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                            };

                            auto texture_info = (TextureLoadA1*)(rpak_data.data() + file_seeks[file.array_idx] + file.array_entry_offset);

                            // indicates streaming bullshit
                            if (texture_info->unk22) {
                                std::printf("\tStarPak offset: %p\n", file.starpak_offset);
                            }

                            //hacky...
                            bool canDump = texture_info->texture_type == 1 && texture_info->mipmaps == 1;
                            bool dump2 = texture_info->texture_type == 1 && texture_info->mipmaps == 9 && texture_info->unk22 == 3 && (texture_info->width == 2048 && texture_info->height == 2048);
                            if (canDump || dump2) {
                                short_name[0] = 'sdd';//"dds"
                                sprintf_s(k0k, "%s.%03d.0x%p.%s", argv[1], i, file.hash, short_name);
                            }

                            outf = std::ofstream(k0k, std::fstream::binary);

                            if (canDump) {
                                auto lobyte_ = 8ui64;
                                auto hibyte_ = 4ui64;

                                auto pitchOrLinearSize = lobyte_ * ((texture_info->width + hibyte_ - 1) >> (hibyte_ >> 1)) * ((texture_info->height + hibyte_ - 1) >> (hibyte_ >> 1));

                                // DDS...
                                *(u32*)(dds_header + 0xC) = texture_info->height;
                                *(u32*)(dds_header + 0x10) = texture_info->width;

                                *(u32*)(dds_header + 0x14) = pitchOrLinearSize;

                                std::printf("\t Type: %u | %u %u | %ux%u | %08X\n", texture_info->texture_type, texture_info->mipmaps, texture_info->unk22, texture_info->width, texture_info->height, pitchOrLinearSize);

                                outf.write((const char*)dds_header, sizeof(dds_header));
                            }
                            else if (dump2) {
                                std::printf("\t %p Type: %u | %u %u | %ux%u\n", file.hash, texture_info->texture_type, texture_info->mipmaps, texture_info->unk22, texture_info->width, texture_info->height);

                                // even more hacky...
                                // TODO: make less resolution dependant
                                constexpr auto add_offset = 0xA0000; // to biggest mipmap being full size
                                constexpr auto file_size = 0x200000; // 2048x2048

                                *(u32*)(dds_header + 0xC) = texture_info->height;
                                *(u32*)(dds_header + 0x10) = texture_info->width;

                                *(u32*)(dds_header + 0x14) = file_size;
                                outf.write((const char*)dds_header, sizeof(dds_header));

                                std::vector<uint8_t> tmpdat(file_size);
                                auto tmpname = (const char*)rpak_data.data() + starpak_other;
                                auto tmpfind = strrchr(tmpname, '\\') + 1;
                                std::ifstream starpak(tmpfind, std::fstream::binary);
                                starpak.seekg(file.starpak_offset + add_offset, std::fstream::beg);
                                starpak.read((char*)tmpdat.data(), file_size);

                                outf.write((const char*)tmpdat.data(), file_size);
                                continue; // we want to skip the normal write from the rpak...
                            }
                            else {
                                std::printf("\t %p Type: %u | %u %u | %ux%u\n", file.hash, texture_info->texture_type, texture_info->mipmaps, texture_info->unk22, texture_info->width, texture_info->height);
                                std::printf("\t\tDON'T KNOW HOW TO DUMP!\n");
                            }
                        }
                        // Can't prettify this format, write as is...
                        else {
                            outf = std::ofstream(k0k, std::fstream::binary);
                        }
                        outf.write((const char*)rpak_data.data() + offset, size);
                    }
                }
            }
        }
    }
}

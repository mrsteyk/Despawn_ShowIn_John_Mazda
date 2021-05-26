// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include <iostream>
#include <unordered_map>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <cinttypes>
#include <intrin.h>

#pragma intrinsic(_ReturnAddress)

#include <MinHook.h>
#pragma comment(lib, "libMinhook.x64.lib")

uintptr_t orig[34];
uintptr_t g_base;

class TypeShit
{
public:
    uint32_t short_name; //0x0000
    uint32_t N00000183; //0x0004
    char* name; //0x0008
    void* LoadDispatch; //0x0010 // functions like CreateTexture
    void* func1; //0x0018
    void* func2; //0x0020 // for texture this is responsible for adding it to rpak list
    void* chunk_ptr_size_of_elems_times_count; //0x0028
    char pad_0030[4]; //0x0030
    uint32_t elem_size; //0x0034
    char pad_0038[4]; //0x0038
    uint32_t size_16_and_count; //0x003C
    uint32_t N0000009A; //0x0040
    uint32_t count_minus_1; //0x0044
    uint32_t elem_size_2; //0x0048
    uint32_t count; //0x004C
    void* chunk_ptr; //0x0050
    void* allocd_shit; //0x0058
}; //Size: 0x0060
static_assert(sizeof(TypeShit) == 96, "TypeShit bruh moment");

enum class RPakStatus : int32_t
{
    None = 0,
    End = 7, // appears to be on all rpaks which are not being proccessed...
    Error = 11, // errors like bad read etc etc
    Start = 1, // says that it needs to parse rpak and whatnot
    Unk10 = 10,
    Unk2 = 2, // allocates some buffers and whatnot
    Unk3 = 3,
    Unk4 = 4,
    Unk5 = 5,
    Unk6 = 6,
    Unk8 = 8,
    Unk9 = 9
};

// inlined array rtech_game.dll+0x3C38A0
class Chunk_ptr_struct
{
public:
    char pad_0000[4]; //0x0000
    RPakStatus some_status_shit; //0x0004 // used for switch case of sub_18000B8B0
    uint32_t Version; //0x0008 // 7 in this game always
    uint32_t Compressed_qm; //0x000C // C++ doesn't allow ? in variable names
    char* rpak_name; //0x0010
    char pad_0018[24]; //0x0018
    void* someshit_0; //0x0030
    char* (*someshit_for_patch_its_pak_names)[512]; //0x0038
    void* someshit_2; //0x0040
    void* someshit_3; //0x0048
    char pad_0050[24]; //0x0050
    int32_t file_handle; //0x0068
    char pad_006C[60]; //0x006C
}; //Size: 0x00A8
static_assert(sizeof(Chunk_ptr_struct) == 0x15*8, "ChunkPtr shit bruh moment");

using pair_rpak_t = std::pair<bool, std::string>;
std::unordered_map<__int64, pair_rpak_t> g_isRPak;

__int64 __fastcall find_asset(char* a1, __int64 a2) {
    auto ret = reinterpret_cast<decltype(&find_asset)>(orig[13])(a1, a2);

    std::printf("find_asset(%64s, %p) = %p\n", a1, a2, ret);

    return ret;
}

__int64 __fastcall open_file(const CHAR* a1, LARGE_INTEGER* a2) {
    auto ret = reinterpret_cast<decltype(&open_file)>(orig[24])(a1, a2);

    auto bruh = a2 ? a2->QuadPart : 0i64;

    std::printf("open_file(%s, %p) = %p | %p\n", a1, a2, ret, bruh);
    g_isRPak[ret] = pair_rpak_t{ strstr(a1, ".rpak") || strstr(a1, ".starpak"), a1 };

    return ret;
}

__int64 __fastcall increment_ref_count(__int64 a1) {
    auto ret = reinterpret_cast<decltype(&increment_ref_count)>(orig[26])(a1);

    std::printf("increment_ref_count(%" PRIx64 ") = %p\n", a1, ret);

    return ret;
}

__int64 __fastcall dec_close_file_bs(__int64 a1) {
    auto ret = reinterpret_cast<decltype(&dec_close_file_bs)>(orig[25])(a1);

    std::printf("dec_close_file_bs(%" PRIx64 ") = %p\n", a1, ret);

    return ret;
}

__int64 __fastcall register_type_job_qm(__int64 type_shit_ptr, unsigned int job_priority, unsigned int affinity) {
    auto ret = reinterpret_cast<decltype(&register_type_job_qm)>(orig[1])(type_shit_ptr, job_priority, affinity);

    uint32_t str[]{ *(uint32_t*)type_shit_ptr, 0 };
    auto shit_idx = ((0xFF0B020B * str[0]) >> 24) & 0xF;
    auto oof = (TypeShit*)(g_base + 0x43270 + 96 * shit_idx);
    std::printf("register_type_job_qm(%p, %u, %u) = %p | %s %s %p %p %p\n", type_shit_ptr, job_priority, affinity, ret, str, oof->name, oof->LoadDispatch, oof->func1, oof->func2);

    return ret;
}

__int64 __fastcall unk_3(void* Src, __int64 a2, int a3) {
    auto ret = reinterpret_cast<decltype(&unk_3)>(orig[3])(Src, a2, a3);

    std::printf("unk_3(%32s, %p, %08X) = %p\n", Src, a2, a3);

    return ret;
}

__int64 __fastcall read_from_file_bs(unsigned int a1, __int64 a2, unsigned __int64 a3, __int64 a4, int a5) {
    auto ret = reinterpret_cast<decltype(&read_from_file_bs)>(orig[27])(a1, a2, a3, a4, a5);

    if(g_isRPak[a1].first && g_isRPak[a1].second.find(".starpak") != std::string::npos)
        std::printf("read_from_file_bs(%s, %p, %p, %p, %u) = %p | %p\n", g_isRPak[a1].second.c_str(), a2, a3, a4, a5, ret, _ReturnAddress());

    return ret;
}

PVOID decompress_rpak;
__int64 __fastcall decompress_rpak_hk(__int64* a1, unsigned __int64 a2, unsigned __int64 a3) {
    auto ret = reinterpret_cast<decltype(&decompress_rpak_hk)>(decompress_rpak)(a1, a2, a3);

    std::printf("decompress_rpak to %p\n", a1[1]);

    return ret;
}

__int64 __fastcall some_file_io_internal(unsigned int a1, __int64 a2, unsigned __int64 a3, __int64 a4, __int64 a5, __int64 a6, int a7) {
    auto ret = reinterpret_cast<decltype(&some_file_io_internal)>(orig[28])(a1, a2, a3, a4, a5, a6, a7);

    if (g_isRPak.find(a1) != g_isRPak.end() && g_isRPak[a1].first && g_isRPak[a1].second.find(".starpak") != std::string::npos)
        std::printf("some_file_io_internal(%s, %p, %p, %p, o, o, %u) = %p | %p\n", g_isRPak[a1].second.c_str(), a2, a3, a4, a7, ret, _ReturnAddress());

    return ret;
}

void __fastcall sub_180007D50(int a1, int short_name, __int64 a3) {

    std::printf("sub_180007D50(%u, %08X, %p) | %p\n", a1, short_name, a3, _ReturnAddress());

    reinterpret_cast<decltype(&sub_180007D50)>(orig[16])(a1, short_name, a3);
}

extern "C" //__declspec(dllexport)
void __fastcall Pak_SetLoadFuncs(uintptr_t * a1, uintptr_t b, uintptr_t c) {
    std::printf("Bruh\n");

    auto handle = LoadLibraryA("rtech_game_.dll");
    g_base = uintptr_t(handle);

    auto proc = reinterpret_cast<decltype(&Pak_SetLoadFuncs)>(GetProcAddress(handle, "Pak_SetLoadFuncs"));
    proc(a1, b, c);

    /**a1 = init_iface;
    a1[1] = register_type_job_qm;                 // called with various shit like "Ptch", "ui" "uimg" etc etc
    a1[3] = sub_18000B0F0;
    a1[4] = sub_18000B170;
    a1[5] = sub_18000B1B0;
    a1[6] = sub_18000B280;
    a1[8] = sub_18000B4F0;
    a1[9] = sub_18000BB70;
    a1[10] = sub_18000BC10;
    a1[11] = sub_180009A80;
    a1[12] = find_asset_by_hash;
    a1[13] = find_asset;                          // calls find asset by hash
    a1[15] = sub_180009960;
    a1[16] = sub_180007D50;
    a1[17] = sub_18000AFD0;
    a1[18] = sub_180007CF0;
    a1[24] = open_rpak_file;                      // mount rpak? calls CreateFileA so it's an open func
    a1[26] = increment_ref_count;                 // used for mstr and other shit usually, increments refcount of address&0x3ff ???
    a1[25] = dec_close_file_bs;                   // literally closes the handle... decrements refcount of address&0x3ff?
    a1[27] = read_from_file_bs;
    a1[28] = sub_180001F30;                       // kind of like read_from_file_bs but with more control over the parameters
    a1[29] = format_error_helper;
    a1[30] = sub_180001C20;                       // some "kind-of fence" with error getter?
    a1[31] = sub_180001A90;                       // set's some critical shit and calls the above aka [30]
    a1[32] = sub_180001A40;                       // same as the above [31] but without the last call
    result = (__int64(__fastcall*)())get_thread_handle;
    a1[33] = get_thread_handle;                   // get some thread handle from first call inside init_iface*/

    memcpy(orig, a1, sizeof(orig));

    //a1[1] = uintptr_t(&register_type_job_qm);
    MH_CreateHook(PVOID(a1[1]), &register_type_job_qm, (PVOID*)&orig[1]);

    //a1[3] = uintptr_t(&unk_3);
    MH_CreateHook(PVOID(a1[3]), &unk_3, (PVOID*)&orig[3]);

    //a1[12] = uintptr_t(&find_asset_by_hash);
    //a1[13] = uintptr_t(&find_asset);

    //a1[24] = uintptr_t(&open_file);
    MH_CreateHook(PVOID(a1[24]), &open_file, (PVOID*)&orig[24]);
    //a1[26] = uintptr_t(&increment_ref_count);
    //a1[25] = uintptr_t(&dec_close_file_bs);
    
    //a1[27] = uintptr_t(&read_from_file_bs);
    MH_CreateHook(PVOID(a1[27]), &read_from_file_bs, (PVOID*)&orig[27]);

    //MH_CreateHook(PVOID(g_base + 0x4EA0), &decompress_rpak_hk, &decompress_rpak);

    //MH_CreateHook(PVOID(g_base + 0x1F40), &some_file_io_internal, (PVOID*)&orig[28]);

    MH_CreateHook(PVOID(a1[16]), &sub_180007D50, (PVOID*)&orig[16]);

    MH_EnableHook(MH_ALL_HOOKS);

    //while (!IsDebuggerPresent())
    //    Sleep(100);

    // */
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        AllocConsole();
        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);

        SetConsoleTitleA("TitanFall 2 RTech BS");

        MH_Initialize();
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}


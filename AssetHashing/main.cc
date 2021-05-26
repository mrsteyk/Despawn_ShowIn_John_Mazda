#include <iostream>
#include <string>
#include <cinttypes>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using _DWORD = uint32_t;

unsigned __int64 __fastcall hash_string(_DWORD* a1)
{
    _DWORD* v1; // r8
    unsigned __int64 v2; // r10
    int v3; // er11
    unsigned int v4; // er9
    unsigned int i; // edx
    __int64 v6; // rcx
    int v7; // er9
    int v8; // edx
    int v9; // eax
    unsigned int v10; // er8
    int v12; // ecx

    v1 = a1;
    v2 = 0i64;
    v3 = 0;
    v4 = (*a1 - 45 * ((~(*a1 ^ 0x5C5C5C5Cu) >> 7) & (((*a1 ^ 0x5C5C5C5Cu) - 0x1010101) >> 7) & 0x1010101)) & 0xDFDFDFDF;
    for (i = ~*a1 & (*a1 - 0x1010101) & 0x80808080; !i; i = v8 & 0x80808080)
    {
        v6 = v4;
        v7 = v1[1];
        ++v1;
        v3 += 4;
        v2 = ((((unsigned __int64)(0xFB8C4D96501i64 * v6) >> 24) + 0x633D5F1 * v2) >> 61) ^ (((unsigned __int64)(0xFB8C4D96501i64 * v6) >> 24)
            + 0x633D5F1 * v2);
        v8 = ~v7 & (v7 - 0x1010101);
        v4 = (v7 - 45 * ((~(v7 ^ 0x5C5C5C5Cu) >> 7) & (((v7 ^ 0x5C5C5C5Cu) - 0x1010101) >> 7) & 0x1010101)) & 0xDFDFDFDF;
    }
    v9 = -1;
    v10 = (i & -(signed)i) - 1;
    if (_BitScanReverse((unsigned long*)&v12, v10))
        v9 = v12;
    return 0x633D5F1 * v2
        + ((0xFB8C4D96501i64 * (unsigned __int64)(v4 & v10)) >> 24)
        - 0xAE502812AA7333i64 * (unsigned int)(v3 + v9 / 8);
}

int main(int argc, char* argv[])
{
    std::puts("This will hash the asset string for internal use in what looks like some sort of unordered_map\nInput your string");
    std::string to_hash;
    if (argc > 1) {
        to_hash = argv[1];
    }
    else {
        std::getline(std::cin, to_hash);
    }

    std::cout << std::hex;

    auto actual = hash_string((_DWORD*)to_hash.c_str());

    for (auto& e : to_hash) {
        if (e == '/' || e == '\\')
            e = 0xf;
        else
            e = toupper(e);
    }
    auto expected = std::hash<std::string>{}(to_hash);

    std::printf("Expected hash: %" PRIx64 " [%llu]\nActual hash: %" PRIx64 " [%llu]\n", expected, expected & 0x1FFFF, actual, actual&0x1FFFF);
}

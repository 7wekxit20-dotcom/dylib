#include <cstdio>
#include <cstdint>
#include <substrate.h>
#include <mach-o/dyld.h>

// Dynamic Base Address calculation for iOS Mach-O ASLR
void* getRealOffset(uintptr_t offset) {
    uintptr_t base = 0;
    for (uint32_t i = 0; i < _dyld_image_count(); i++) {
        const char* imageName = _dyld_get_image_name(i);
        if (strstr(imageName, "UnityFramework") || strstr(imageName, "FreeFire") || strstr(imageName, "il2cpp")) {
            base = _dyld_get_image_vmaddr_slide(i) + 0x100000000;
            break;
        }
    }
    // Fallback if not found
    if (base == 0) {
        base = _dyld_get_image_vmaddr_slide(0) + 0x100000000;
    }
    return (void*)(base + offset);
}

// ===============================================================================
// Updated Offsets Extracted from Metadata & script.json
// ===============================================================================
#define BAN_ACC_PTR_OFFSET            0x4008DF0 // COW.UIModelAntiAddiction$$get_IsBan
#define BLACKLIST_PTR_OFFSET          0x374BA1C // proto.BlacklistRes$$ParseFrom
#define BLACKLIST_INFO_PTR_OFFSET     0x3872764 // proto.BlacklistInfoRes$$ParseFrom
#define IS_BANNED_PTR_OFFSET          0x5C00C14 // COW.GamePlay.LevelReviveTower$$get_IsBanned

// Additional Anti-Cheat / Telemetry / Emulator Offsets
#define IS_EMULATOR_PTR_OFFSET        0x55DEEF0 // COW.GameFacade$$IsEmulator
#define REPORT_GGP_PTR_OFFSET         0x374F8E4 // proto.ReportGGPInfo$$ParseFrom
#define DETECT_PACKAGES_PTR_OFFSET    0x38704E4 // proto.CSGetAndroidApplicationToDetectRes$$ParseFrom

// Biến toàn cục lưu pointer hàm gốc
bool (*originalBanAcc)(void* instance) = nullptr;
bool (*originalIsBanned)(void* instance) = nullptr;
void (*originalBlacklist)(void* instance, void* reader) = nullptr;
void (*originalBlacklistInfo)(void* instance, void* reader) = nullptr;
bool (*originalIsEmulator)(void* instance) = nullptr;
void (*originalReportGGP)(void* instance, void* reader) = nullptr;
void (*originalDetectPackages)(void* instance, void* reader) = nullptr;

bool isBlocked = false;

// Hàm giả thay thế get_IsBan
bool FakeBanAcc(void* instance) {
    if (isBlocked) {
        printf("[!] get_IsBan() accessed - Returning false (Not Banned)\n");
        return false;
    }
    if (originalBanAcc != nullptr) {
        return originalBanAcc(instance);
    }
    return false;
}

// Hàm giả thay thế get_IsBanned
bool FakeIsBanned(void* instance) {
    if (isBlocked) {
        printf("[!] get_IsBanned() accessed - Returning false (Not Banned)\n");
        return false;
    }
    if (originalIsBanned != nullptr) {
        return originalIsBanned(instance);
    }
    return false;
}

// Bypass Emulator Detection (Always return false so player is matched with mobile users)
bool FakeIsEmulator(void* instance) {
    if (isBlocked) {
        printf("[!] GameFacade::IsEmulator() accessed - Returning false (Bypassing Emulator Pool)\n");
        return false;
    }
    if (originalIsEmulator != nullptr) {
        return originalIsEmulator(instance);
    }
    return false;
}

// Hàm giả thay thế BlacklistRes.ParseFrom
void FakeBlacklist(void* instance, void* reader) {
    if (originalBlacklist != nullptr) {
        originalBlacklist(instance, reader);
    }
    
    if (isBlocked && instance != nullptr) {
        printf("[!] BlacklistRes::ParseFrom() accessed - Bypassing data\n");
        *(bool *)((uint64_t)instance + 0x30) = false; // proto.BlacklistRes.is_in_blacklist
    }
}

// Hàm giả thay thế BlacklistInfoRes.ParseFrom
void FakeBlacklistInfo(void* instance, void* reader) {
    if (originalBlacklistInfo != nullptr) {
        originalBlacklistInfo(instance, reader);
    }
    
    if (isBlocked && instance != nullptr) {
        printf("[!] BlacklistInfoRes::ParseFrom() accessed - Bypassing data\n");
        *(int *)((uint64_t)instance + 0x10) = 0; // BlacklistInfoRes.ban_reason (0 = NONE)
        *(uint32_t *)((uint64_t)instance + 0x14) = 0; // BlacklistInfoRes.expire_duration
    }
}

// Block Telemetry GGP Security Reports
void FakeReportGGP(void* instance, void* reader) {
    if (isBlocked) {
        printf("[!] proto::ReportGGPInfo::ParseFrom() blocked - Telemetry report dropped\n");
        return;
    }
    if (originalReportGGP != nullptr) {
        originalReportGGP(instance, reader);
    }
}

// Block Android Package Scan Requests
void FakeDetectPackages(void* instance, void* reader) {
    if (isBlocked) {
        printf("[!] proto::CSGetAndroidApplicationToDetectRes::ParseFrom() blocked - Package scan neutralized\n");
        return;
    }
    if (originalDetectPackages != nullptr) {
        originalDetectPackages(instance, reader);
    }
}

// Bật tất cả hooks
void EnableBlock() {
    if (isBlocked) return;

    MSHookFunction((void*)getRealOffset(BAN_ACC_PTR_OFFSET), (void*)FakeBanAcc, (void**)&originalBanAcc);
    MSHookFunction((void*)getRealOffset(IS_BANNED_PTR_OFFSET), (void*)FakeIsBanned, (void**)&originalIsBanned);
    MSHookFunction((void*)getRealOffset(BLACKLIST_PTR_OFFSET), (void*)FakeBlacklist, (void**)&originalBlacklist);
    MSHookFunction((void*)getRealOffset(BLACKLIST_INFO_PTR_OFFSET), (void*)FakeBlacklistInfo, (void**)&originalBlacklistInfo);
    
    // Extra Security Bypasses
    MSHookFunction((void*)getRealOffset(IS_EMULATOR_PTR_OFFSET), (void*)FakeIsEmulator, (void**)&originalIsEmulator);
    MSHookFunction((void*)getRealOffset(REPORT_GGP_PTR_OFFSET), (void*)FakeReportGGP, (void**)&originalReportGGP);
    MSHookFunction((void*)getRealOffset(DETECT_PACKAGES_PTR_OFFSET), (void*)FakeDetectPackages, (void**)&originalDetectPackages);

    isBlocked = true;
    printf("[+] All Ban, Blacklist, Emulator, and Anti-Cheat Telemetry hooks ENABLED!\n");
}

// iOS dylib constructor
__attribute__((constructor))
void lib_init() {
    EnableBlock();
}

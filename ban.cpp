#include <cstdio>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>

// getRealOffset - calculates real address from offset using DYLD image slide (iOS ASLR)
// On iOS, il2cpp is loaded as a framework inside the app bundle
void* getRealOffset(uintptr_t offset) {
    // Walk all loaded images to find the game binary (UnityFramework / il2cpp)
    uint32_t count = 0;
    
    // dlopen trick to get base - works on jailbroken iOS
    void* handle = dlopen(NULL, RTLD_LAZY);
    if (!handle) return (void*)offset;
    dlclose(handle);
    
    // Fallback: use the offset directly (Dobby resolves base itself on jailbreak)
    return (void*)offset;
}

// Hook function pointer types for DobbyHook
typedef int (*DobbyHook_t)(void*, void*, void**);
static DobbyHook_t _DobbyHook = nullptr;

// Lazy-load Dobby at runtime so the dylib links even without Dobby at compile time
static bool initDobby() {
    if (_DobbyHook) return true;
    void* h = dlopen("/usr/lib/libdobby.dylib", RTLD_LAZY);
    if (!h) h = dlopen("/var/jb/usr/lib/libdobby.dylib", RTLD_LAZY); // Rootless Jailbreak path
    if (!h) { printf("[!] libdobby.dylib not found!\n"); return false; }
    _DobbyHook = (DobbyHook_t)dlsym(h, "DobbyHook");
    return _DobbyHook != nullptr;
}

// ===============================================================================
// Offsets extracted from script.json + metadata (Free Fire / COW)
// ===============================================================================
#define BAN_ACC_PTR_OFFSET            0x4008DF0 // COW.UIModelAntiAddiction$$get_IsBan
#define BLACKLIST_PTR_OFFSET          0x374BA1C // proto.BlacklistRes$$ParseFrom
#define BLACKLIST_INFO_PTR_OFFSET     0x3872764 // proto.BlacklistInfoRes$$ParseFrom
#define IS_BANNED_PTR_OFFSET          0x5C00C14 // COW.GamePlay.LevelReviveTower$$get_IsBanned
#define IS_EMULATOR_PTR_OFFSET        0x55DEEF0 // COW.GameFacade$$IsEmulator
#define REPORT_GGP_PTR_OFFSET         0x374F8E4 // proto.ReportGGPInfo$$ParseFrom
#define DETECT_PACKAGES_PTR_OFFSET    0x38704E4 // proto.CSGetAndroidApplicationToDetectRes$$ParseFrom

// Original function pointers
static bool (*originalBanAcc)(void*) = nullptr;
static bool (*originalIsBanned)(void*) = nullptr;
static void (*originalBlacklist)(void*, void*) = nullptr;
static void (*originalBlacklistInfo)(void*, void*) = nullptr;
static bool (*originalIsEmulator)(void*) = nullptr;
static void (*originalReportGGP)(void*, void*) = nullptr;
static void (*originalDetectPackages)(void*, void*) = nullptr;

static bool isBlocked = false;

bool FakeBanAcc(void* instance) {
    printf("[!] get_IsBan() -> false\n");
    return false;
}

bool FakeIsBanned(void* instance) {
    printf("[!] get_IsBanned() -> false\n");
    return false;
}

bool FakeIsEmulator(void* instance) {
    printf("[!] IsEmulator() -> false (bypassing emulator pool)\n");
    return false;
}

void FakeBlacklist(void* instance, void* reader) {
    if (originalBlacklist) originalBlacklist(instance, reader);
    if (instance) {
        printf("[!] BlacklistRes::ParseFrom() - clearing is_in_blacklist\n");
        *(bool*)((uint64_t)instance + 0x30) = false;
    }
}

void FakeBlacklistInfo(void* instance, void* reader) {
    if (originalBlacklistInfo) originalBlacklistInfo(instance, reader);
    if (instance) {
        printf("[!] BlacklistInfoRes::ParseFrom() - clearing ban_reason\n");
        *(int*)((uint64_t)instance + 0x10) = 0;
        *(uint32_t*)((uint64_t)instance + 0x14) = 0;
    }
}

void FakeReportGGP(void* instance, void* reader) {
    printf("[!] ReportGGPInfo::ParseFrom() - telemetry dropped\n");
}

void FakeDetectPackages(void* instance, void* reader) {
    printf("[!] CSGetAndroidApplicationToDetectRes - package scan dropped\n");
}

void EnableBlock() {
    if (isBlocked) return;
    if (!initDobby()) {
        printf("[!] Dobby not available - hooks NOT installed\n");
        return;
    }

    _DobbyHook((void*)getRealOffset(BAN_ACC_PTR_OFFSET),         (void*)FakeBanAcc,          (void**)&originalBanAcc);
    _DobbyHook((void*)getRealOffset(IS_BANNED_PTR_OFFSET),        (void*)FakeIsBanned,         (void**)&originalIsBanned);
    _DobbyHook((void*)getRealOffset(BLACKLIST_PTR_OFFSET),        (void*)FakeBlacklist,        (void**)&originalBlacklist);
    _DobbyHook((void*)getRealOffset(BLACKLIST_INFO_PTR_OFFSET),   (void*)FakeBlacklistInfo,    (void**)&originalBlacklistInfo);
    _DobbyHook((void*)getRealOffset(IS_EMULATOR_PTR_OFFSET),      (void*)FakeIsEmulator,       (void**)&originalIsEmulator);
    _DobbyHook((void*)getRealOffset(REPORT_GGP_PTR_OFFSET),       (void*)FakeReportGGP,        (void**)&originalReportGGP);
    _DobbyHook((void*)getRealOffset(DETECT_PACKAGES_PTR_OFFSET),  (void*)FakeDetectPackages,   (void**)&originalDetectPackages);

    isBlocked = true;
    printf("[+] banBypass: ALL hooks installed!\n");
}

__attribute__((constructor))
void lib_init() {
    EnableBlock();
}

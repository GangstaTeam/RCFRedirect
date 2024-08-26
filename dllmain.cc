#define _CRT_SECURE_NO_WARNINGS
#define _HAS_EXCEPTIONS 0
#define WIN32_LEAN_AND_MEAN
#include <cstdint>
#include <unordered_set>
#include <Windows.h>

std::unordered_set<uint32_t> gIgnoreFileNames;

bool __fastcall CementLibrary_OpenFile(void* p_CementLibrary, void* edx, uint32_t p_kFileName, void** p_File)
{
    if (gIgnoreFileNames.find(p_kFileName) != gIgnoreFileNames.end()) {
        return false;
    }

    return reinterpret_cast<bool(__thiscall*)(void*, uint32_t, void**)>(0x6E6D90)(p_CementLibrary, p_kFileName, p_File);
}

bool ReplaceJmpRel32(uintptr_t p_Address, void* p_Target)
{
    uint32_t uJmpOffset = (reinterpret_cast<uint32_t>(p_Target) - p_Address - 5);
    uint32_t* pPatchAddress = reinterpret_cast<uint32_t*>(p_Address + 1);

    DWORD dwOldProtection;
    if (!VirtualProtect(reinterpret_cast<void*>(pPatchAddress), sizeof(uint32_t), PAGE_EXECUTE_READWRITE, &dwOldProtection)) {
        return false;
    }

    *pPatchAddress = uJmpOffset;

    VirtualProtect(reinterpret_cast<void*>(pPatchAddress), sizeof(uint32_t), dwOldProtection, &dwOldProtection);
    return true;
}

void RegisterFileOrFolder(const char* p_szPath)
{
    char dirPath[MAX_PATH] = { 0 };
    const char* pLastDelimer = strrchr(p_szPath, '/\\');
    if (pLastDelimer) {
        strncpy(dirPath, p_szPath, static_cast<size_t>(pLastDelimer - p_szPath) + 1);
    }

    WIN32_FIND_DATAA wFindData = { 0 };
    HANDLE hFindData = FindFirstFileA(p_szPath, &wFindData);
    if (hFindData == INVALID_HANDLE_VALUE) {
        return;
    }

    char fullPath[MAX_PATH];
    do
    {
        sprintf_s(fullPath, sizeof(fullPath), "%s%s", dirPath, wFindData.cFileName);

        if (wFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (wFindData.cFileName[0] == '.') {
                continue;
            }

            RegisterFileOrFolder(fullPath);
            continue;
        }

        uint32_t fileHash = reinterpret_cast<uint32_t(__cdecl*)(const char*)>(0x6DCC70)(fullPath);
        gIgnoreFileNames.insert(fileHash);
    } 
    while (FindNextFileA(hFindData, &wFindData));

    FindClose(hFindData);
}

bool LoadList()
{
    FILE* pFileList = fopen("plugins\\RCFRedirect.list", "r");
    if (!pFileList) {
        return false;
    }
  
    char filePath[MAX_PATH];
    while (fgets(filePath, sizeof(filePath), pFileList) != NULL)
    {
        filePath[strcspn(filePath, "\r\n")] = '\0';
        RegisterFileOrFolder(filePath);
    }

    fclose(pFileList);
    return true;
}

int __stdcall DllMain(HMODULE p_hModule, DWORD p_dwReason, void* p_pReserved)
{
    if (p_dwReason == DLL_PROCESS_ATTACH)
    {
        if (*reinterpret_cast<uint32_t*>(0x400128) != 0x455CAD51) 
        {
            MessageBoxA(0, "You're using wrong game version. v1.00.2 is required!", "RCFRedirect", MB_OK | MB_ICONERROR);
            return 0;
        }

        if (!LoadList()) {
            return 0;
        }

        // Replace function call to our to filter file names.
        ReplaceJmpRel32(0x6E6FA0, CementLibrary_OpenFile);
    }

    return 1;
}
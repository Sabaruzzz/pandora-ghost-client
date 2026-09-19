#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <conio.h>
#include "payload.h"

using f_LoadLibraryA = HINSTANCE(WINAPI*)(const char* lpLibFilename);
using f_GetProcAddress = FARPROC(WINAPI*)(HMODULE hModule, LPCSTR lpProcName);
using f_DLL_ENTRY_POINT = BOOL(WINAPI*)(void* hDll, DWORD dwReason, void* pReserved);

// Funciones para ofuscacion de la IAT (Import Address Table)
using f_OpenProcess = HANDLE(WINAPI*)(DWORD, BOOL, DWORD);
using f_VirtualAllocEx = LPVOID(WINAPI*)(HANDLE, LPVOID, SIZE_T, DWORD, DWORD);
using f_WriteProcessMemory = BOOL(WINAPI*)(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*);
using f_CreateRemoteThread = HANDLE(WINAPI*)(HANDLE, LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
using f_VirtualFreeEx = BOOL(WINAPI*)(HANDLE, LPVOID, SIZE_T, DWORD);

struct MANUAL_MAPPING_DATA {
    f_LoadLibraryA pLoadLibraryA;
    f_GetProcAddress pGetProcAddress;
    BYTE* pbase;
};

// --- INICIO DEL SHELLCODE QUE SE EJECUTARÁ DENTRO DE MINECRAFT ---
#pragma runtime_checks( "", off )
#pragma optimize( "", off )
void __stdcall Shellcode(MANUAL_MAPPING_DATA* pData) {
    if (!pData) return;

    BYTE* pBase = pData->pbase;
    auto* pOpt = &reinterpret_cast<IMAGE_NT_HEADERS*>(pBase + reinterpret_cast<IMAGE_DOS_HEADER*>((uintptr_t)pBase)->e_lfanew)->OptionalHeader;

    auto _LoadLibraryA = pData->pLoadLibraryA;
    auto _GetProcAddress = pData->pGetProcAddress;
    auto _DllMain = reinterpret_cast<f_DLL_ENTRY_POINT>(pBase + pOpt->AddressOfEntryPoint);

    // Arreglar Relocations
    BYTE* LocationDelta = pBase - pOpt->ImageBase;
    if (LocationDelta) {
        if (pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size) {
            auto* pRelocData = reinterpret_cast<IMAGE_BASE_RELOCATION*>(pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
            while (pRelocData->VirtualAddress) {
                UINT AmountOfEntries = (pRelocData->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                WORD* pRelativeInfo = reinterpret_cast<WORD*>(pRelocData + 1);

                for (UINT i = 0; i != AmountOfEntries; ++i, ++pRelativeInfo) {
                    if ((*pRelativeInfo >> 12) == IMAGE_REL_BASED_DIR64) {
                        UINT_PTR* pPatch = reinterpret_cast<UINT_PTR*>(pBase + pRelocData->VirtualAddress + ((*pRelativeInfo) & 0xFFF));
                        *pPatch += reinterpret_cast<UINT_PTR>(LocationDelta);
                    }
                }
                pRelocData = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<BYTE*>(pRelocData) + pRelocData->SizeOfBlock);
            }
        }
    }

    // Resolver Imports (Dependencias)
    if (pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size) {
        auto* pImportDescr = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        while (pImportDescr->Name) {
            char* szMod = reinterpret_cast<char*>(pBase + pImportDescr->Name);
            HINSTANCE hDll = _LoadLibraryA(szMod);

            ULONG_PTR* pThunkRef = reinterpret_cast<ULONG_PTR*>(pBase + pImportDescr->OriginalFirstThunk);
            ULONG_PTR* pFuncRef = reinterpret_cast<ULONG_PTR*>(pBase + pImportDescr->FirstThunk);

            if (!pThunkRef) pThunkRef = pFuncRef;

            for (; *pThunkRef; ++pThunkRef, ++pFuncRef) {
                if (IMAGE_SNAP_BY_ORDINAL(*pThunkRef)) {
                    *pFuncRef = (ULONG_PTR)_GetProcAddress(hDll, reinterpret_cast<char*>(*pThunkRef & 0xFFFF));
                } else {
                    auto* pImport = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(pBase + (*pThunkRef));
                    *pFuncRef = (ULONG_PTR)_GetProcAddress(hDll, pImport->Name);
                }
            }
            ++pImportDescr;
        }
    }

    // TLS Callbacks
    if (pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size) {
        auto* pTLS = reinterpret_cast<IMAGE_TLS_DIRECTORY*>(pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);
        auto* pCallback = reinterpret_cast<PIMAGE_TLS_CALLBACK*>(pTLS->AddressOfCallBacks);
        for (; pCallback && *pCallback; ++pCallback)
            (*pCallback)(pBase, DLL_PROCESS_ATTACH, nullptr);
    }

    // Ejecutar el DllMain del cheat!
    _DllMain(pBase, DLL_PROCESS_ATTACH, nullptr);
}
#pragma optimize( "", on )
#pragma runtime_checks( "", restore )
// --- FIN DEL SHELLCODE ---

struct ProcessWindowData {
    DWORD pid;
    HWND hwnd;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    ProcessWindowData* data = reinterpret_cast<ProcessWindowData*>(lParam);
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);
    
    if (processId == data->pid && IsWindowVisible(hwnd)) {
        int length = GetWindowTextLengthW(hwnd);
        if (length > 0) {
            data->hwnd = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

DWORD GetProcessIdByName(const wchar_t* name) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;
    
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);
    
    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (wcscmp(pe.szExeFile, name) == 0) {
                ProcessWindowData data = { pe.th32ProcessID, nullptr };
                EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));
                
                if (data.hwnd != nullptr) {
                    CloseHandle(hSnapshot);
                    return pe.th32ProcessID;
                }
            }
        } while (Process32NextW(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);
    return 0;
}

void XORStrW(wchar_t* str, size_t len, wchar_t key) {
    for (size_t i = 0; i < len; ++i) str[i] ^= key;
}
void XORStrA(char* str, size_t len, char key) {
    for (size_t i = 0; i < len; ++i) str[i] ^= key;
}

bool ManualMap(HANDLE hProc, BYTE* pSrcData, SIZE_T FileSize) {
    char cKernel32[] = { 'K'^0x15, 'E'^0x15, 'R'^0x15, 'N'^0x15, 'E'^0x15, 'L'^0x15, '3'^0x15, '2'^0x15, '.'^0x15, 'D'^0x15, 'L'^0x15, 'L'^0x15, 0 };
    XORStrA(cKernel32, 12, 0x15);
    HMODULE hKernel32 = GetModuleHandleA(cKernel32);

    char cVirtualAllocEx[] = { 'V'^0x21, 'i'^0x21, 'r'^0x21, 't'^0x21, 'u'^0x21, 'a'^0x21, 'l'^0x21, 'A'^0x21, 'l'^0x21, 'l'^0x21, 'o'^0x21, 'c'^0x21, 'E'^0x21, 'x'^0x21, 0 };
    char cWriteProcessMem[] = { 'W'^0x21, 'r'^0x21, 'i'^0x21, 't'^0x21, 'e'^0x21, 'P'^0x21, 'r'^0x21, 'o'^0x21, 'c'^0x21, 'e'^0x21, 's'^0x21, 's'^0x21, 'M'^0x21, 'e'^0x21, 'm'^0x21, 'o'^0x21, 'r'^0x21, 'y'^0x21, 0 };
    char cCreateRemoteThread[] = { 'C'^0x21, 'r'^0x21, 'e'^0x21, 'a'^0x21, 't'^0x21, 'e'^0x21, 'R'^0x21, 'e'^0x21, 'm'^0x21, 'o'^0x21, 't'^0x21, 'e'^0x21, 'T'^0x21, 'h'^0x21, 'r'^0x21, 'e'^0x21, 'a'^0x21, 'd'^0x21, 0 };
    char cVirtualFreeEx[] = { 'V'^0x21, 'i'^0x21, 'r'^0x21, 't'^0x21, 'u'^0x21, 'a'^0x21, 'F'^0x21, 'r'^0x21, 'e'^0x21, 'e'^0x21, 'E'^0x21, 'x'^0x21, 0 };

    XORStrA(cVirtualAllocEx, 14, 0x21);
    XORStrA(cWriteProcessMem, 18, 0x21);
    XORStrA(cCreateRemoteThread, 18, 0x21);
    XORStrA(cVirtualFreeEx, 13, 0x21);

    auto pVirtualAllocEx = (f_VirtualAllocEx)GetProcAddress(hKernel32, cVirtualAllocEx);
    auto pWriteProcessMemory = (f_WriteProcessMemory)GetProcAddress(hKernel32, cWriteProcessMem);
    auto pCreateRemoteThread = (f_CreateRemoteThread)GetProcAddress(hKernel32, cCreateRemoteThread);
    auto pVirtualFreeEx = (f_VirtualFreeEx)GetProcAddress(hKernel32, cVirtualFreeEx);

    IMAGE_NT_HEADERS* pOldNtHeader = reinterpret_cast<IMAGE_NT_HEADERS*>(pSrcData + reinterpret_cast<IMAGE_DOS_HEADER*>(pSrcData)->e_lfanew);
    IMAGE_OPTIONAL_HEADER* pOldOptHeader = &pOldNtHeader->OptionalHeader;
    IMAGE_FILE_HEADER* pOldFileHeader = &pOldNtHeader->FileHeader;

    BYTE* pTargetBase = reinterpret_cast<BYTE*>(pVirtualAllocEx(hProc, nullptr, pOldOptHeader->SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!pTargetBase) return false;

    pWriteProcessMemory(hProc, pTargetBase, pSrcData, pOldOptHeader->SizeOfHeaders, nullptr);

    IMAGE_SECTION_HEADER* pSectionHeader = IMAGE_FIRST_SECTION(pOldNtHeader);
    for (UINT i = 0; i != pOldFileHeader->NumberOfSections; ++i, ++pSectionHeader) {
        if (pSectionHeader->SizeOfRawData) {
            pWriteProcessMemory(hProc, pTargetBase + pSectionHeader->VirtualAddress, pSrcData + pSectionHeader->PointerToRawData, pSectionHeader->SizeOfRawData, nullptr);
        }
    }

    MANUAL_MAPPING_DATA data{ 0 };
    data.pLoadLibraryA = LoadLibraryA;
    data.pGetProcAddress = reinterpret_cast<f_GetProcAddress>(GetProcAddress);
    data.pbase = pTargetBase;

    BYTE* MappingDataAlloc = reinterpret_cast<BYTE*>(pVirtualAllocEx(hProc, nullptr, sizeof(MANUAL_MAPPING_DATA), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    pWriteProcessMemory(hProc, MappingDataAlloc, &data, sizeof(MANUAL_MAPPING_DATA), nullptr);

    void* pShellcode = pVirtualAllocEx(hProc, nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    pWriteProcessMemory(hProc, pShellcode, Shellcode, 0x1000, nullptr);

    HANDLE hThread = pCreateRemoteThread(hProc, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(pShellcode), MappingDataAlloc, 0, nullptr);
    if (!hThread) {
        pVirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
        pVirtualFreeEx(hProc, MappingDataAlloc, 0, MEM_RELEASE);
        pVirtualFreeEx(hProc, pShellcode, 0, MEM_RELEASE);
        return false;
    }

    CloseHandle(hThread);
    return true;
}

// --- PURPLE CLOUD CONSOLE UI ---

void MoveCursor(HANDLE hConsole, int x, int y) {
    COORD pos;
    pos.X = static_cast<SHORT>(x);
    pos.Y = static_cast<SHORT>(y);
    SetConsoleCursorPosition(hConsole, pos);
}

void SetPurpleColor(HANDLE hConsole) {
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
}

void SetLightPurpleColor(HANDLE hConsole) {
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
}

void ConfigurepandoraOrange(HANDLE hConsole) {
    CONSOLE_SCREEN_BUFFER_INFOEX info{};
    info.cbSize = sizeof(info);
    if (GetConsoleScreenBufferInfoEx(hConsole, &info)) {
        info.ColorTable[15] = RGB(255, 255, 255);
        SetConsoleScreenBufferInfoEx(hConsole, &info);
    }
}

void DrawpandoraASCII(HANDLE hConsole) {
    system("cls");
    SetPurpleColor(hConsole);

    const std::string asciiLines[] = {
        "                     _                 ",
        "                    | |                ",
        "_ __   __ _ _ __   __| | ___  _ __ __ _ ",
        "| '_ \\ / _` | '_ \\ / _` |/ _ \\| '__/ _` |",
        "| |_) | (_| | | | | (_| | (_) | | | (_| |",
        "| .__/ \\__,_|_| |_|\\__,_|\\___/|_|  \\__,_|",
        "| |                                      ",
        "|_|                                      "
    };

    int startY = 4;
    int startX = 18; // Centered for 80-width console (43 char wide logo)

    for (size_t i = 0; i < 8; ++i) {
        MoveCursor(hConsole, startX, startY + static_cast<int>(i));
        std::cout << asciiLines[i];
    }
}

void UpdateStatusText(HANDLE hConsole, const std::string& text, int yPos = 18) {
    MoveCursor(hConsole, 0, yPos);
    SetPurpleColor(hConsole);

    // Clear status line cleanly
    std::cout << std::string(80, ' ');

    // Center text
    int xPos = (80 - static_cast<int>(text.length())) / 2;
    if (xPos < 0) xPos = 0;

    MoveCursor(hConsole, xPos, yPos);
    std::cout << text;
}

void AnimateStatusDots(HANDLE hConsole, const std::string& baseText, int durationMs, int yPos = 18) {
    int elapsed = 0;
    int interval = 180;
    int dotStep = 0;
    bool dotIncreasing = true;

    while (elapsed < durationMs) {
        std::string dots = "";
        for (int i = 0; i < dotStep; i++) dots += ".";

        UpdateStatusText(hConsole, baseText + dots, yPos);
        Sleep(interval);
        elapsed += interval;

        if (dotIncreasing) {
            dotStep++;
            if (dotStep > 3) {
                dotStep = 2;
                dotIncreasing = false;
            }
        } else {
            dotStep--;
            if (dotStep < 0) {
                dotStep = 1;
                dotIncreasing = true;
            }
        }
    }
}

void DrawMappingProgressBar(HANDLE hConsole, int percentage, const std::string& statusText, int yPos = 16) {
    MoveCursor(hConsole, 0, yPos);
    SetPurpleColor(hConsole);
    std::cout << std::string(80, ' ');
    MoveCursor(hConsole, 0, yPos + 2);
    std::cout << std::string(80, ' ');

    int barWidth = 44;
    int filled = (percentage * barWidth) / 100;

    // Solid purple block bar WITHOUT brackets or percentage numbers
    std::string barStr = "";
    for (int i = 0; i < filled; i++) barStr += (char)219;
    for (int i = filled; i < barWidth; i++) barStr += " ";

    int xPos = (80 - barWidth) / 2;
    if (xPos < 0) xPos = 0;

    MoveCursor(hConsole, xPos, yPos);
    SetPurpleColor(hConsole);
    std::cout << barStr;

    if (!statusText.empty()) {
        int textX = (80 - static_cast<int>(statusText.length())) / 2;
        if (textX < 0) textX = 0;
        MoveCursor(hConsole, textX, yPos + 2);
        SetLightPurpleColor(hConsole);
        std::cout << statusText;
        SetPurpleColor(hConsole);
    }
}

std::string ReadMaskedPassword(HANDLE hConsole) {
    std::string pass = "";
    char ch;
    while ((ch = _getch()) != '\r') { // Enter key
        if (ch == '\b') { // Backspace
            if (!pass.empty()) {
                pass.pop_back();
                std::cout << "\b \b";
            }
        } else if (ch >= 32 && ch <= 126) {
            pass.push_back(ch);
            std::cout << "*";
        }
    }
    std::cout << std::endl;
    return pass;
}

int main() {
    // Window without title name as requested
    SetConsoleTitleA("");

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    ConfigurepandoraOrange(hConsole);

    // Hide console cursor
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);

    // Set normal window size
    SMALL_RECT windowSize = { 0, 0, 79, 24 };
    SetConsoleWindowInfo(hConsole, TRUE, &windowSize);
    COORD bufferSize = { 80, 25 };
    SetConsoleScreenBufferSize(hConsole, bufferSize);

    // Draw header
    DrawpandoraASCII(hConsole);

    // Step 0: Login screen (pandora / pandora)
    bool authenticated = false;
    while (!authenticated) {
        DrawpandoraASCII(hConsole);

        // Show input text prompts centered
        MoveCursor(hConsole, 25, 14);
        SetLightPurpleColor(hConsole);
        std::cout << "Username: ";
        SetPurpleColor(hConsole);
        cursorInfo.bVisible = TRUE;
        SetConsoleCursorInfo(hConsole, &cursorInfo);

        std::string user;
        std::cin >> user;

        MoveCursor(hConsole, 25, 16);
        SetLightPurpleColor(hConsole);
        std::cout << "Password: ";
        SetPurpleColor(hConsole);

        std::string pass = ReadMaskedPassword(hConsole);

        cursorInfo.bVisible = FALSE;
        SetConsoleCursorInfo(hConsole, &cursorInfo);

        if (user == "pandora" && pass == "pandora") {
            authenticated = true;
            UpdateStatusText(hConsole, "Access Granted. Welcome to pandora.", 19);
            Sleep(1000);
        } else {
            UpdateStatusText(hConsole, "Invalid credentials. Try again.", 19);
            Sleep(1500);
        }
    }

    DrawpandoraASCII(hConsole);

    // Step 1: Downloading dependencies (animated dots)
    AnimateStatusDots(hConsole, "Downloading dependencies", 1200);

    // Step 2: Searching for minecraft process (animated dots loop)
    wchar_t procName[] = { L'j'^0x11, L'a'^0x11, L'v'^0x11, L'a'^0x11, L'w'^0x11, L'.'^0x11, L'e'^0x11, L'x'^0x11, L'e'^0x11, 0 };
    XORStrW(procName, 9, 0x11);

    DWORD pid = 0;
    int dotStep = 0;
    bool dotIncreasing = true;
    while (pid == 0) {
        pid = GetProcessIdByName(procName);
        if (pid != 0) break;

        std::string dots = "";
        for (int i = 0; i < dotStep; i++) dots += ".";

        UpdateStatusText(hConsole, "Searching for minecraft process" + dots);
        Sleep(200);

        if (dotIncreasing) {
            dotStep++;
            if (dotStep > 3) {
                dotStep = 2;
                dotIncreasing = false;
            }
        } else {
            dotStep--;
            if (dotStep < 0) {
                dotStep = 1;
                dotIncreasing = true;
            }
        }
    }

    // Step 3: Starting pandora in 5 countdown
    for (int i = 5; i > 0; i--) {
        UpdateStatusText(hConsole, "Starting pandora in " + std::to_string(i) + "...");
        Sleep(1000);
    }

    // Step 4: Open process & decrypt payload
    char cKernel32[] = { 'K'^0x15, 'E'^0x15, 'R'^0x15, 'N'^0x15, 'E'^0x15, 'L'^0x15, '3'^0x15, '2'^0x15, '.'^0x15, 'D'^0x15, 'L'^0x15, 'L'^0x15, 0 };
    XORStrA(cKernel32, 12, 0x15);
    HMODULE hKernel32 = GetModuleHandleA(cKernel32);

    char cOpenProcess[] = { 'O'^0x21, 'p'^0x21, 'e'^0x21, 'n'^0x21, 'P'^0x21, 'r'^0x21, 'o'^0x21, 'c'^0x21, 'e'^0x21, 's'^0x21, 's'^0x21, 0 };
    XORStrA(cOpenProcess, 11, 0x21);
    auto pOpenProcess = (f_OpenProcess)GetProcAddress(hKernel32, cOpenProcess);

    HANDLE hProc = pOpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!hProc) {
        UpdateStatusText(hConsole, "Failed to attach process.");
        Sleep(2500);
        return 1;
    }

    // Step 5: Starting mapping... with progress bar & realistic delay
    DrawpandoraASCII(hConsole);
    for (int pct = 0; pct <= 100; pct += 2) {
        DrawMappingProgressBar(hConsole, pct, "Starting mapping...", 16);
        Sleep(80);
    }

    BYTE* decPayload = new BYTE[payload_size];
    for (unsigned int i = 0; i < payload_size; i++) {
        decPayload[i] = payload[i] ^ 0x93;
    }

    // Step 6: Injecting...
    DrawMappingProgressBar(hConsole, 100, "Injecting...", 16);
    Sleep(300);

    bool success = ManualMap(hProc, decPayload, payload_size);
    Sleep(200);

    SecureZeroMemory(decPayload, payload_size);
    delete[] decPayload;
    CloseHandle(hProc);

    // Step 7: Injected successfully. closing in...
    if (success) {
        for (int i = 3; i > 0; i--) {
            std::string msg = "Injected successfully. Closing in " + std::to_string(i) + "...";
            DrawMappingProgressBar(hConsole, 100, msg, 16);
            Sleep(1000);
        }
    } else {
        DrawMappingProgressBar(hConsole, 100, "Injection failed.", 16);
        Sleep(2500);
    }

    cursorInfo.bVisible = TRUE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);

    return 0;
}

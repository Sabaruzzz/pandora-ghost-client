#include "injector.h"

bool injector::is_already_injected(DWORD target_pid) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, target_pid);
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    MODULEENTRY32 module_entry{};
    module_entry.dwSize = sizeof(MODULEENTRY32);

    for (BOOL next = Module32First(snapshot, &module_entry); next; next = Module32Next(snapshot, &module_entry)) {
        // Check for our DLL by looking at the temp file naming pattern
        std::string mod_name = module_entry.szModule;
        // Our temp DLL is named pandora_<pid>_<tick>.dll
        if (mod_name.find("pandora_") == 0 && mod_name.find(".dll") != std::string::npos) {
            CloseHandle(snapshot);
            return true;
        }
    }
    CloseHandle(snapshot);
    return false;
}

bool injector::c_injector::inject() {
    DWORD target_pid = pid;

    if (target_pid == 0) {
        target_pid = get_process_id();
    }

    if (target_pid == NULL)
        return false;

    constexpr DWORD access = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ;
    HANDLE process = OpenProcess(access, FALSE, target_pid);

    if (!process || process == INVALID_HANDLE_VALUE)
        return false;

    if (!std::filesystem::exists(dll_path)) {
        CloseHandle(process);
        return false;
    }

    const SIZE_T path_size = dll_path.size() + 1;
    void* alloc_memory = VirtualAllocEx(process, nullptr, path_size,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!alloc_memory) {
        CloseHandle(process);
        return false;
    }

    SIZE_T bytes_written = 0;
    if (!WriteProcessMemory(process, alloc_memory, dll_path.c_str(), path_size,
        &bytes_written) || bytes_written != path_size) {
        VirtualFreeEx(process, alloc_memory, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    auto load_library = kernel32
        ? reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(kernel32, "LoadLibraryA"))
        : nullptr;
    if (!load_library) {
        VirtualFreeEx(process, alloc_memory, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    HANDLE thread = CreateRemoteThread(process, nullptr, 0, load_library,
        alloc_memory, 0, nullptr);
    if (!thread) {
        VirtualFreeEx(process, alloc_memory, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    // Wait for LoadLibraryA to complete — use 30 seconds instead of 15.
    // Heavy mod packs or slow disks can take longer to map the DLL.
    const DWORD wait_result = WaitForSingleObject(thread, 30000);

    // FIX: Don't rely on GetExitCodeThread for HMODULE — on x64 the 64-bit
    // HMODULE is truncated to a 32-bit DWORD, causing false negatives when
    // the lower 32 bits happen to be zero. Instead, verify that the module
    // is actually loaded in the target process by enumerating modules.
    bool completed = false;
    if (wait_result == WAIT_OBJECT_0) {
        // Give Windows a moment to finalize the mapping
        Sleep(50);
        // Verify the DLL is actually loaded by checking modules
        completed = is_already_injected(pid != 0 ? pid : target_pid);
    }

    CloseHandle(thread);
    VirtualFreeEx(process, alloc_memory, 0, MEM_RELEASE);
    CloseHandle(process);

    return completed;
}

bool injector::c_injector::inject_and_wait(DWORD timeout_ms) {
    DWORD target_pid = pid;
    if (target_pid == 0) {
        target_pid = get_process_id();
    }
    if (target_pid == 0) return false;

    // Prevent double injection — if our DLL is already loaded, bail out.
    if (is_already_injected(target_pid)) {
        return false;
    }

    // Create the named event BEFORE injecting so the DLL can find it
    // immediately when LogicThread starts. Manual-reset, initially non-signaled.
    const std::string event_name = make_event_name(target_pid);
    HANDLE ready_event = CreateEventA(nullptr, TRUE, FALSE, event_name.c_str());
    if (!ready_event) {
        return false;
    }

    // Perform the standard injection
    bool injected = inject();
    if (!injected) {
        CloseHandle(ready_event);
        return false;
    }

    // Wait for the DLL's LogicThread to signal that hooks and JNI are ready.
    // This is the key fix — without this, modules may not be initialized yet.
    const DWORD wait_result = WaitForSingleObject(ready_event, timeout_ms);
    CloseHandle(ready_event);

    return wait_result == WAIT_OBJECT_0;
}

DWORD injector::c_injector::get_process_id() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot == INVALID_HANDLE_VALUE)
        return NULL;

    PROCESSENTRY32 process_entry;
    process_entry.dwSize = sizeof(PROCESSENTRY32);

    DWORD found_pid = 0;
    for (
        bool next = Process32First(snapshot, &process_entry);
        next;
        next = Process32Next(snapshot, &process_entry))
    {
        if (process_name == (std::string)process_entry.szExeFile) {
            found_pid = process_entry.th32ProcessID;
            break;
        }
    }

    // FIX: Always close the snapshot handle — the old code returned early
    // inside the loop, leaking the kernel handle every single time.
    CloseHandle(snapshot);
    return found_pid;
}

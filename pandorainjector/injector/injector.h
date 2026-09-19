#pragma once

#include <iostream>
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <filesystem>
#include <memory>

namespace injector {

    // Named event shared between injector and DLL for init synchronization.
    // The DLL signals this event after LogicThread finishes initialization.
    // Format includes the target PID so multiple instances don't collide.
    inline std::string make_event_name(DWORD target_pid) {
        return "Global\\pandoraClientReady_" + std::to_string(target_pid);
    }

    // Check if pandora.dll is already loaded in the target process
    bool is_already_injected(DWORD target_pid);

    class c_injector
    {
    public:
        c_injector(std::string process_name) {
            this->process_name = process_name;
            this->pid = 0;
            this->dll_path.clear();
        }

        c_injector(DWORD pid) {
            this->pid = pid;
            this->process_name = "";
            this->dll_path.clear();
        }

        DWORD get_process_id();
        bool inject();

        // Inject and wait for the DLL to signal that initialization is complete.
        // Returns true only when the DLL has fully initialized (JNI, hooks, etc).
        // timeout_ms: max milliseconds to wait for the DLL init signal.
        bool inject_and_wait(DWORD timeout_ms = 30000);

        std::string dll_path;

    private:
        std::string process_name;
        DWORD pid;
    };

    inline std::unique_ptr<c_injector> instance;
}

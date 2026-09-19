#include "../injector/injector.h"
#include "payload.h"

#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {
    constexpr short kConsoleWidth = 120;
    constexpr short kConsoleHeight = 32;

    struct TargetProcess {
        DWORD pid{};
        std::string title;
    };

    void move_cursor(HANDLE console, short x, short y) {
        SetConsoleCursorPosition(console, COORD{ x, y });
    }

    void set_color(HANDLE console, WORD color) {
        SetConsoleTextAttribute(console, color);
    }

    void clear_line(HANDLE console, short y) {
        move_cursor(console, 0, y);
        std::cout << std::string(kConsoleWidth, ' ');
    }

    void center_text(HANDLE console, short y, const std::string& text, WORD color) {
        const short x = static_cast<short>((std::max)(0, (kConsoleWidth - static_cast<int>(text.length())) / 2));
        move_cursor(console, x, y);
        set_color(console, color);
        std::cout << text;
    }

    void setup_console(HANDLE console) {
        SetConsoleTitleW(L"");
        DWORD mode = 0;
        if (GetConsoleMode(console, &mode)) {
            SetConsoleMode(console, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
        SetConsoleOutputCP(CP_UTF8);
        CONSOLE_CURSOR_INFO cursor{};
        GetConsoleCursorInfo(console, &cursor);
        cursor.bVisible = FALSE;
        SetConsoleCursorInfo(console, &cursor);

        COORD buffer{ kConsoleWidth, kConsoleHeight };
        SetConsoleScreenBufferSize(console, buffer);
        SMALL_RECT rect{ 0, 0, static_cast<SHORT>(kConsoleWidth - 1), static_cast<SHORT>(kConsoleHeight - 1) };
        SetConsoleWindowInfo(console, TRUE, &rect);

        if (HWND window = GetConsoleWindow()) {
            SetWindowTextW(window, L"");
            RECT bounds{};
            GetWindowRect(window, &bounds);
            MoveWindow(window, bounds.left, bounds.top, 960, 540, TRUE);
            LONG style = GetWindowLongW(window, GWL_STYLE);
            style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
            SetWindowLongW(window, GWL_STYLE, style);
            SetWindowPos(window, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            ShowScrollBar(window, SB_BOTH, FALSE);
        }
    }

    void clear_screen() {
        std::cout << "\x1b[2J\x1b[H";
    }

    void draw_banner(double opacity) {
        static const std::vector<std::string> banner = {
            "                        _                 ",
            "                       | |                ",
            "  _ __   __ _ _ __   __| | ___  _ __ __ _ ",
            " | '_ \\ / *` | '* \\ / _` |/ _ \\| '__/ _` |",
            " | |*) | (*| | | | | (*| | (*) | | | (_| |",
            " | .__/ \\__,*|*| |*|\\_\\_,*|\\___/|*|  \\_\\_,*|",
            " | |                                      ",
            " |_|                                      "
        };
        for (size_t i = 0; i < banner.size(); ++i) {
            const double shade = 1.0 - static_cast<double>(i) / (banner.size() - 1) * 0.70;
            const int red = static_cast<int>(155.0 * shade * opacity);
            const int green = static_cast<int>(205.0 * shade * opacity);
            const int blue = static_cast<int>(255.0 * shade * opacity);
            const short x = static_cast<short>((kConsoleWidth - static_cast<int>(banner[i].size())) / 2);
            move_cursor(GetStdHandle(STD_OUTPUT_HANDLE), x, static_cast<short>(4 + i));
            std::cout << "\x1b[38;2;" << red << ';' << green << ';' << blue << "m" << banner[i] << "\x1b[0m";
        }

        struct Flake { short x; short y; char glyph; double strength; };
        static const Flake flakes[] = {
            { 23, 5, '*', 0.45 }, { 28, 11, '+', 0.62 }, { 35, 3, '.', 0.40 },
            { 85, 4, '*', 0.60 }, { 91, 8, '+', 0.48 }, { 82, 11, '.', 0.42 },
            { 26, 8, '.', 0.36 }, { 88, 10, '*', 0.52 }
        };
        for (const auto& flake : flakes) {
            const int red = static_cast<int>(185.0 * flake.strength * opacity);
            const int green = static_cast<int>(225.0 * flake.strength * opacity);
            const int blue = static_cast<int>(255.0 * flake.strength * opacity);
            move_cursor(GetStdHandle(STD_OUTPUT_HANDLE), flake.x, flake.y);
            std::cout << "\x1b[38;2;" << red << ';' << green << ';' << blue << "m" << flake.glyph << "\x1b[0m";
        }
    }

    void animate_banner() {
        clear_screen();
        for (int frame = 1; frame <= 13; ++frame) {
            draw_banner(static_cast<double>(frame) / 13.0);
            std::this_thread::sleep_for(std::chrono::milliseconds(18));
        }
    }

    void draw_shell(HANDLE console) {
        (void)console;
        animate_banner();
    }

    void render_status(HANDLE console, const std::string& message, short row, int shade = 208) {
        clear_line(console, row);
        const short x = static_cast<short>((kConsoleWidth - static_cast<int>(message.size())) / 2);
        move_cursor(console, x, row);
        std::cout << "\x1b[38;2;" << shade << ';' << shade << ';' << shade << "m" << message << "\x1b[0m";
    }

    void type_status(HANDLE console, const std::string& message, short row) {
        clear_line(console, row);
        const short x = static_cast<short>((kConsoleWidth - static_cast<int>(message.size())) / 2);
        move_cursor(console, x, row);
        for (char c : message) {
            std::cout << "\x1b[38;2;105;175;255m" << c << "\x1b[0m" << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(7));
            std::cout << "\b\x1b[38;2;200;200;200m" << c << "\x1b[0m" << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    }

    void transition_status(HANDLE console, const std::string& previous, const std::string& next, short row) {
        if (!previous.empty()) {
            for (int shade = 190; shade >= 45; shade -= 29) {
                render_status(console, previous, row, shade);
                std::this_thread::sleep_for(std::chrono::milliseconds(12));
            }
        }
        type_status(console, next, row);
    }

    bool payload_is_valid() {
        return payload_size > 2 &&
            static_cast<unsigned char>(payload[0] ^ 0x93) == 'M' &&
            static_cast<unsigned char>(payload[1] ^ 0x93) == 'Z';
    }

    bool write_payload(std::filesystem::path& destination) {
        if (!payload_is_valid()) return false;

        wchar_t temp[MAX_PATH]{};
        const DWORD length = GetTempPathW(MAX_PATH, temp);
        if (length == 0 || length >= MAX_PATH) return false;

        destination = std::filesystem::path(temp) /
            (L"pandora_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
             std::to_wstring(GetTickCount64()) + L".dll");

        HANDLE file = CreateFileW(destination.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;

        std::vector<unsigned char> decoded(payload_size);
        for (unsigned int i = 0; i < payload_size; ++i)
            decoded[i] = static_cast<unsigned char>(payload[i] ^ 0x93);

        DWORD written = 0;
        const bool ok = payload_size <= MAXDWORD &&
            WriteFile(file, decoded.data(), static_cast<DWORD>(payload_size), &written, nullptr) &&
            written == payload_size;
        // FIX: Flush file buffers to guarantee the DLL bytes are fully on disk
        // before LoadLibraryA in the remote process tries to read them.
        // FILE_ATTRIBUTE_TEMPORARY tells Windows to keep data in cache, which
        // can delay physical writes. Without this flush, LoadLibraryA may read
        // a partial/corrupt PE and crash.
        if (ok) FlushFileBuffers(file);
        SecureZeroMemory(decoded.data(), decoded.size());
        CloseHandle(file);

        if (!ok) {
            DeleteFileW(destination.c_str());
            destination.clear();
        }
        return ok;
    }

    // Keywords that identify a Minecraft client window title.
    // Covers vanilla, Forge, Lunar, Badlion, Feather, Labymod, etc.
    bool title_looks_like_minecraft(const char* title) {
        const char* keywords[] = {
            "Minecraft", "minecraft",
            "Lunar",     "lunar",
            "Badlion",   "badlion",
            "Feather",   "feather",
            "LabyMod",   "labymod",
            "Forge",     "forge",
            "PvP",       "pvp",
            "1.7",  "1.8",  "1.9",  "1.12", "1.16", "1.17", "1.18", "1.19", "1.20", "1.21",
            nullptr
        };
        for (int i = 0; keywords[i]; ++i) {
            if (strstr(title, keywords[i])) return true;
        }
        return false;
    }

    std::vector<TargetProcess> find_minecraft_processes() {
        std::vector<TargetProcess> targets;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return targets;

        PROCESSENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        for (BOOL next = Process32First(snapshot, &entry); next; next = Process32Next(snapshot, &entry)) {
            if (_stricmp(entry.szExeFile, "javaw.exe") != 0) continue;

            TargetProcess target{};
            target.pid = entry.th32ProcessID;
            target.title.clear();
            EnumWindows([](HWND hwnd, LPARAM value) -> BOOL {
                auto* target = reinterpret_cast<TargetProcess*>(value);
                DWORD windowPid{};
                GetWindowThreadProcessId(hwnd, &windowPid);
                if (windowPid != target->pid || !IsWindowVisible(hwnd)) return TRUE;
                char title[256]{};
                if (GetWindowTextA(hwnd, title, static_cast<int>(sizeof(title))) > 0) {
                    target->title = title;
                    return FALSE;
                }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&target));

            // FIX: Only accept processes that have a visible window with a
            // Minecraft-like title. This prevents injecting into random Java
            // apps, launchers, or headless JVM instances.
            if (!target.title.empty() && title_looks_like_minecraft(target.title.c_str())) {
                targets.push_back(std::move(target));
            }
        }
        CloseHandle(snapshot);
        return targets;
    }

    void print_progress(HANDLE console, float progress) {
        constexpr int barWidth = 90;
        const int filled = static_cast<int>((std::max)(0.0f, (std::min)(1.0f, progress)) * barWidth);

        clear_line(console, 24);
        const short x = static_cast<short>((kConsoleWidth - barWidth) / 2);
        move_cursor(console, x, 24);
        std::cout << "\x1b[48;2;30;30;30m" << std::string(barWidth, ' ') << "\x1b[0m";
        move_cursor(console, x, 24);
        std::cout << "\x1b[48;2;76;201;126m" << std::string(filled, ' ') << "\x1b[0m";
    }

    void animate_progress(HANDLE console, float from, float to) {
        constexpr int frames = 18;
        for (int frame = 0; frame <= frames; ++frame) {
            const float t = static_cast<float>(frame) / static_cast<float>(frames);
            const float eased = t * t * (3.0f - 2.0f * t);
            print_progress(console, from + (to - from) * eased);
            std::this_thread::sleep_for(std::chrono::milliseconds(18));
        }
    }
}

int main() {
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    setup_console(console);
    draw_shell(console);

    constexpr WORD dim = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    constexpr WORD bright = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;

    const auto targets = find_minecraft_processes();
    if (targets.empty()) {
        type_status(console, "Connecting...", 20);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        transition_status(console, "Connecting...", "No compatible Minecraft process found.", 20);
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return 1;
    }

    const TargetProcess& target = targets.front();

    // FIX: Guard against double injection — if our DLL is already loaded
    // in the target, don't inject again (avoids crash from duplicate hooks).
    if (injector::is_already_injected(target.pid)) {
        type_status(console, "Connecting...", 20);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        transition_status(console, "Connecting...", "Already injected. Close Minecraft first.", 20);
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return 1;
    }

    type_status(console, "Connecting...", 20);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    transition_status(console, "Connecting...", "Signing in.", 20);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    transition_status(console, "Signing in.", "Authenticating...", 20);
    animate_progress(console, 0.0f, 0.20f);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    transition_status(console, "Authenticating...", "Downloading client...", 20);
    animate_progress(console, 0.20f, 0.50f);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    transition_status(console, "Downloading client...", "Initializing...", 20);
    animate_progress(console, 0.50f, 0.70f);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    transition_status(console, "Initializing...", "Injecting...", 20);

    // FIX: Use inject_and_wait with retry. This waits for the DLL's LogicThread
    // to fully initialize (JNI, mapper, hooks) before reporting success.
    // Retry up to 2 times if the first attempt fails.
    std::filesystem::path temporaryDll;
    bool injected = false;
    constexpr int max_attempts = 2;
    for (int attempt = 0; attempt < max_attempts && !injected; ++attempt) {
        if (attempt > 0) {
            render_status(console, "Retrying...", 20, 180);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        if (write_payload(temporaryDll)) {
            injector::c_injector inj(target.pid);
            inj.dll_path = temporaryDll.string();
            injected = inj.inject_and_wait(30000);
            // Delete temp file — Windows won't actually delete it while the
            // DLL is mapped, but it marks it for deletion on close.
            DeleteFileW(temporaryDll.c_str());
        }
    }

    animate_progress(console, 0.70f, 1.0f);
    clear_line(console, 20);
    if (!injected) {
        transition_status(console, "Injecting...", "Injection failed.", 20);
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return 1;
    }

    transition_status(console, "Injecting...", "Injected successfully", 20);
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    clear_line(console, 24);
    for (int seconds = 3; seconds > 0; --seconds) {
        clear_line(console, 20);
        const std::string label = "Closing in ";
        const std::string number = std::to_string(seconds) + "...";
        const short x = static_cast<short>((kConsoleWidth - static_cast<int>(label.size() + number.size())) / 2);
        move_cursor(console, x, 20);
        std::cout << "\x1b[38;2;220;220;220m" << label
                  << "\x1b[38;2;105;175;255m" << number << "\x1b[0m";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}

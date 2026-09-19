#include <winsock2.h>
#include <windows.h>
#include <vector>
#include <mutex>
#include "../hooks.hpp"

#pragma comment(lib, "ws2_32.lib")

namespace features::misc::blink { extern bool is_blinking; }
namespace network_hooks {
    struct BufferedPacket {
        SOCKET sock = INVALID_SOCKET;
        std::vector<char> data;
        DWORD flags = 0;
        ULONGLONG timestamp = 0;
    };

    static std::vector<BufferedPacket> blink_vault;
    static std::mutex vault_mutex;
    static thread_local bool releasing_packets = false;

    typedef int(WSAAPI* WSASend_t)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
    WSASend_t original_WSASend = nullptr;

    int WSAAPI hooked_WSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
    {
        hooks::callback_guard callback;
        if (original_WSASend == nullptr) {
            WSASetLastError(WSAENETDOWN);
            return SOCKET_ERROR;
        }

        if (releasing_packets) {
            return original_WSASend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
        }

        // 1. Blink
        if (features::misc::blink::is_blinking) {
            if (lpOverlapped == nullptr && lpCompletionRoutine == nullptr) {
                std::lock_guard<std::mutex> lock(vault_mutex);
                for (DWORD i = 0; i < dwBufferCount; i++) {
                    BufferedPacket pkt;
                    pkt.sock = s;
                    pkt.flags = dwFlags;
                    pkt.timestamp = GetTickCount64();
                    pkt.data.assign(lpBuffers[i].buf, lpBuffers[i].buf + lpBuffers[i].len);
                    blink_vault.push_back(std::move(pkt));
                }
                if (lpNumberOfBytesSent != nullptr) {
                    DWORD total = 0;
                    for (DWORD i = 0; i < dwBufferCount; i++) total += lpBuffers[i].len;
                    *lpNumberOfBytesSent = total;
                }
                return 0;
            }
        }
        else {
            std::vector<BufferedPacket> packets;
            {
                std::lock_guard<std::mutex> lock(vault_mutex);
                packets.swap(blink_vault);
            }

            releasing_packets = true;
            for (auto& pkt : packets) {
                WSABUF buf;
                buf.buf = pkt.data.data();
                buf.len = (ULONG)pkt.data.size();
                DWORD bytes_sent = 0;
                original_WSASend(pkt.sock, &buf, 1, &bytes_sent, pkt.flags, nullptr, nullptr);
            }
            releasing_packets = false;
        }

        // 2. Comportamiento normal
        return original_WSASend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
    }
}

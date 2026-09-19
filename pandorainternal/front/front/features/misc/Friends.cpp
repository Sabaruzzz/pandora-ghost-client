#include "../features.hpp"
#include "sdk.hpp"
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#pragma comment(lib, "winhttp.lib")

extern bool gui_friends_enabled;
extern int gui_friends_add_bind;
extern int gui_friends_nearby_bind;
extern int gui_friends_clear_bind;
extern void TriggerNotification(const char*, const char*, const char*);

namespace features::friends {
    extern std::vector<std::string>* list;
    extern std::vector<std::string>* uuids;

    namespace {
        struct LookupResult {
            std::string name;
            std::string uuid;
            bool by_uuid = false;
            bool verified = false;
        };

        struct LookupRequest {
            std::string value;
            bool by_uuid = false;
        };

        std::mutex resolver_mutex;
        std::recursive_mutex list_mutex;
        std::condition_variable resolver_cv;
        std::deque<LookupRequest> pending_lookups;
        std::deque<LookupResult> completed_lookups;
        std::unordered_set<std::string> queued_names;
        std::unordered_map<std::string, std::string> uuid_cache;
        std::thread resolver_thread;
        bool resolver_stopping = false;

        bool add_was_down = false;
        bool nearby_was_down = false;
        bool clear_was_down = false;

        std::string lower_copy(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return value;
        }

        std::string normalize_uuid(std::string value) {
            value.erase(std::remove(value.begin(), value.end(), '-'), value.end());
            return lower_copy(std::move(value));
        }

        std::string resolve_uuid(const std::string& name) {
            if (name.empty()) return {};

            HINTERNET session = WinHttpOpen(
                L"pandoraClient/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!session) return {};

            WinHttpSetTimeouts(session, 2000, 2000, 2000, 2000);
            HINTERNET connection = WinHttpConnect(
                session, L"api.mojang.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
            if (!connection) {
                WinHttpCloseHandle(session);
                return {};
            }

            std::wstring wide_name(name.begin(), name.end());
            std::wstring path = L"/users/profiles/minecraft/" + wide_name;
            HINTERNET request = WinHttpOpenRequest(
                connection, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

            std::string response;
            if (request &&
                WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(request, nullptr)) {
                DWORD status = 0;
                DWORD status_size = sizeof(status);
                WinHttpQueryHeaders(request,
                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                    WINHTTP_NO_HEADER_INDEX);

                if (status == 200) {
                    DWORD available = 0;
                    while (WinHttpQueryDataAvailable(request, &available) && available != 0) {
                        std::string chunk(available, '\0');
                        DWORD received = 0;
                        if (!WinHttpReadData(request, chunk.data(), available, &received)) break;
                        chunk.resize(received);
                        response += chunk;
                    }
                }
            }

            if (request) WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);

            const size_t id_key = response.find("\"id\"");
            if (id_key == std::string::npos) return {};
            const size_t colon = response.find(':', id_key + 4);
            if (colon == std::string::npos) return {};
            const size_t first_quote = response.find('"', colon + 1);
            if (first_quote == std::string::npos) return {};
            const size_t second_quote = response.find('"', first_quote + 1);
            if (second_quote == std::string::npos) return {};

            const std::string uuid = normalize_uuid(
                response.substr(first_quote + 1, second_quote - first_quote - 1));
            return uuid.size() == 32 ? uuid : std::string{};
        }

        std::string resolve_name(const std::string& uuid) {
            const std::string normalized = normalize_uuid(uuid);
            if (normalized.size() != 32) return {};

            HINTERNET session = WinHttpOpen(
                L"pandoraClient/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!session) return {};

            WinHttpSetTimeouts(session, 2000, 2000, 2000, 2000);
            HINTERNET connection = WinHttpConnect(
                session, L"api.minecraftservices.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
            if (!connection) {
                WinHttpCloseHandle(session);
                return {};
            }

            std::wstring wide_uuid(normalized.begin(), normalized.end());
            std::wstring path = L"/minecraft/profile/lookup/" + wide_uuid;
            HINTERNET request = WinHttpOpenRequest(
                connection, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

            std::string response;
            if (request &&
                WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(request, nullptr)) {
                DWORD status = 0;
                DWORD status_size = sizeof(status);
                WinHttpQueryHeaders(request,
                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                    WINHTTP_NO_HEADER_INDEX);
                if (status == 200) {
                    DWORD available = 0;
                    while (WinHttpQueryDataAvailable(request, &available) && available != 0) {
                        std::string chunk(available, '\0');
                        DWORD received = 0;
                        if (!WinHttpReadData(request, chunk.data(), available, &received)) break;
                        chunk.resize(received);
                        response += chunk;
                    }
                }
            }

            if (request) WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);

            const size_t name_key = response.find("\"name\"");
            if (name_key == std::string::npos) return {};
            const size_t colon = response.find(':', name_key + 6);
            if (colon == std::string::npos) return {};
            const size_t first_quote = response.find('"', colon + 1);
            if (first_quote == std::string::npos) return {};
            const size_t second_quote = response.find('"', first_quote + 1);
            if (second_quote == std::string::npos) return {};
            return response.substr(first_quote + 1, second_quote - first_quote - 1);
        }

        void resolver_loop() {
            for (;;) {
                LookupRequest lookup;
                {
                    std::unique_lock<std::mutex> lock(resolver_mutex);
                    resolver_cv.wait(lock, [] {
                        return resolver_stopping || !pending_lookups.empty();
                    });
                    if (resolver_stopping && pending_lookups.empty()) return;
                    lookup = std::move(pending_lookups.front());
                    pending_lookups.pop_front();
                }

                LookupResult result;
                result.by_uuid = lookup.by_uuid;
                if (lookup.by_uuid) {
                    result.uuid = normalize_uuid(lookup.value);
                    result.name = resolve_name(result.uuid);
                    result.verified = !result.name.empty();
                } else {
                    result.name = lookup.value;
                    result.uuid = resolve_uuid(lookup.value);
                    result.verified = !result.uuid.empty();
                }
                {
                    std::lock_guard<std::mutex> lock(resolver_mutex);
                    const std::string key = (lookup.by_uuid ? "uuid:" : "name:") + lower_copy(lookup.value);
                    if (!lookup.by_uuid) uuid_cache[lower_copy(lookup.value)] = result.uuid;
                    queued_names.erase(key);
                    completed_lookups.push_back(std::move(result));
                }
            }
        }

        void ensure_resolver_started() {
            std::lock_guard<std::mutex> lock(resolver_mutex);
            if (!resolver_thread.joinable() && !resolver_stopping)
                resolver_thread = std::thread(resolver_loop);
        }

        void queue_uuid_lookup(const std::string& name, bool high_priority) {
            if (name.empty()) return;
            ensure_resolver_started();

            const std::string key = "name:" + lower_copy(name);
            {
                std::lock_guard<std::mutex> lock(resolver_mutex);
                if (uuid_cache.find(lower_copy(name)) != uuid_cache.end() || queued_names.count(key) != 0) return;
                queued_names.insert(key);
                LookupRequest request{ name, false };
                if (high_priority) pending_lookups.push_front(std::move(request));
                else pending_lookups.push_back(std::move(request));
            }
            resolver_cv.notify_one();
        }

        void queue_uuid_verification(const std::string& uuid) {
            const std::string normalized = normalize_uuid(uuid);
            if (normalized.size() != 32) return;
            ensure_resolver_started();

            const std::string key = "uuid:" + normalized;
            {
                std::lock_guard<std::mutex> lock(resolver_mutex);
                if (queued_names.count(key) != 0) return;
                queued_names.insert(key);
                pending_lookups.push_front({ normalized, true });
            }
            resolver_cv.notify_one();
        }

        void apply_completed_lookups() {
            std::deque<LookupResult> results;
            {
                std::lock_guard<std::mutex> lock(resolver_mutex);
                results.swap(completed_lookups);
            }

            std::lock_guard<std::recursive_mutex> list_lock(list_mutex);
            if (!list || !uuids) return;
            if (uuids->size() < list->size()) uuids->resize(list->size());
            for (const auto& result : results) {
                if (result.by_uuid) {
                    for (size_t i = 0; i < uuids->size() && i < list->size(); ++i) {
                        if (normalize_uuid((*uuids)[i]) != result.uuid) continue;
                        // La UUID del GameProfile de la entidad es la identidad que usa
                        // esta partida. Mojang no conoce UUID offline, nicked o generadas
                        // por el servidor; un fallo de la API no demuestra que sea un bot
                        // y nunca debe borrar un jugador ya capturado dentro del mundo.
                        break;
                    }
                    continue;
                }

                if (!result.verified || result.uuid.empty()) continue;
                for (size_t i = 0; i < list->size(); ++i) {
                    if (lower_copy((*list)[i]) == lower_copy(result.name) && (*uuids)[i].empty())
                        (*uuids)[i] = result.uuid;
                }
            }
        }

        void toggle_player(mapper::__player& player) {
            std::lock_guard<std::recursive_mutex> list_lock(list_mutex);
            if (!list || !uuids || !player.object) return;
            const std::string uuid = normalize_uuid(player.get_uuid());
            if (uuid.size() != 32) {
                TriggerNotification("Friends", "Could not read the player's GameProfile UUID.", "FRIENDS");
                return;
            }

            for (size_t i = 0; i < uuids->size() && i < list->size(); ++i) {
                if (normalize_uuid((*uuids)[i]) == uuid) {
                    const std::string old_name = (*list)[i];
                    remove_at(i);
                    const std::string body = old_name + " was removed from friends.";
                    TriggerNotification("Friends", body.c_str(), "FRIEND_REMOVE");
                    return;
                }
            }

            std::string display_name = player.get_name();
            if (display_name.empty()) display_name = "Player";
            list->push_back(display_name);
            uuids->push_back(uuid);
            queue_uuid_verification(uuid);
            const std::string body = display_name + " was added as a friend.";
            TriggerNotification("Friends", body.c_str(), "FRIEND_ADD");
        }
    }

    void request_uuid(const std::string& name) {
        queue_uuid_lookup(name, true);
    }

    void verify_uuid(const std::string& uuid) {
        queue_uuid_verification(uuid);
    }

    bool is_friend(const std::string& name) {
        std::lock_guard<std::recursive_mutex> list_lock(list_mutex);
        if (!gui_friends_enabled || !list || !uuids || name.empty()) return false;
        const std::string key = lower_copy(name);

        for (const std::string& entry : *list) {
            if (lower_copy(entry) == key) return true;
        }
        return false;
    }

    bool is_teammate(mapper::__player& player, mapper::__player& local_player) {
        (void)local_player;
        std::lock_guard<std::recursive_mutex> list_lock(list_mutex);
        if (!gui_friends_enabled || !list || !uuids) return false;

        const std::string player_uuid = normalize_uuid(player.get_uuid());
        if (player_uuid.empty()) return false;
        for (const std::string& friend_uuid : *uuids) {
            if (!friend_uuid.empty() && normalize_uuid(friend_uuid) == player_uuid) return true;
        }
        return false;
    }

    void clear() {
        std::lock_guard<std::recursive_mutex> list_lock(list_mutex);
        if (list) list->clear();
        if (uuids) uuids->clear();
    }

    void remove_at(size_t index) {
        std::lock_guard<std::recursive_mutex> list_lock(list_mutex);
        if (!list || index >= list->size()) return;
        list->erase(list->begin() + index);
        if (uuids && index < uuids->size()) uuids->erase(uuids->begin() + index);
    }

    void remove_last() {
        std::lock_guard<std::recursive_mutex> list_lock(list_mutex);
        if (list && !list->empty()) remove_at(list->size() - 1);
    }

    std::vector<std::string> snapshot_names() {
        std::lock_guard<std::recursive_mutex> list_lock(list_mutex);
        return list ? *list : std::vector<std::string>{};
    }

    std::vector<std::pair<std::string, std::string>> snapshot_entries() {
        std::lock_guard<std::recursive_mutex> list_lock(list_mutex);
        std::vector<std::pair<std::string, std::string>> result;
        if (!list) return result;
        result.reserve(list->size());
        for (size_t i = 0; i < list->size(); ++i)
            result.emplace_back((*list)[i], uuids && i < uuids->size() ? (*uuids)[i] : std::string{});
        return result;
    }

    void replace_all(const std::vector<std::string>& names, const std::vector<std::string>& ids) {
        std::lock_guard<std::recursive_mutex> list_lock(list_mutex);
        if (!list || !uuids) return;
        *list = names;
        *uuids = ids;
        if (uuids->size() < list->size()) uuids->resize(list->size());
        else if (uuids->size() > list->size()) uuids->resize(list->size());
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(resolver_mutex);
            resolver_stopping = true;
            pending_lookups.clear();
        }
        resolver_cv.notify_all();
        if (resolver_thread.joinable()) resolver_thread.join();
    }

    void run(mapper::__minecraft& minecraft) {
        apply_completed_lookups();
        if (!gui_friends_enabled || !list || !uuids) return;

        const bool add_down = gui_friends_add_bind != 0 &&
            (GetAsyncKeyState(gui_friends_add_bind) & 0x8000) != 0;
        if (add_down && !add_was_down) {
            auto pointed = minecraft.get_pointed_entity();
            if (pointed.object) toggle_player(pointed);
        }
        add_was_down = add_down;

        const bool nearby_down = gui_friends_nearby_bind != 0 &&
            (GetAsyncKeyState(gui_friends_nearby_bind) & 0x8000) != 0;
        if (nearby_down && !nearby_was_down) {
            auto local = minecraft.get_local_player();
            auto world = minecraft.get_world();
            if (local.object && world.object) {
                const mapper::__vec3 origin = local.get_position();
                int added = 0;
                std::lock_guard<std::recursive_mutex> list_lock(list_mutex);
                for (auto& player : world.get_players()) {
                    if (!player.object || sdk::jni->IsSameObject(player.object, local.object)) continue;
                    const mapper::__vec3 pos = player.get_position();
                    const double dx = pos.x - origin.x;
                    const double dy = pos.y - origin.y;
                    const double dz = pos.z - origin.z;
                    if (std::sqrt(dx * dx + dy * dy + dz * dz) > 15.0) continue;
                    const std::string uuid = normalize_uuid(player.get_uuid());
                    if (uuid.size() != 32) continue;
                    bool exists = false;
                    for (const std::string& stored : *uuids) {
                        if (normalize_uuid(stored) == uuid) { exists = true; break; }
                    }
                    if (!exists) {
                        std::string display_name = player.get_name();
                        if (display_name.empty()) display_name = "Player";
                        list->push_back(display_name);
                        uuids->push_back(uuid);
                        queue_uuid_verification(uuid);
                        ++added;
                    }
                }
                const std::string body = std::to_string(added) + " nearby friends added.";
                TriggerNotification("Friends", body.c_str(), "FRIEND_ADD");
            }
        }
        nearby_was_down = nearby_down;

        const bool clear_down = gui_friends_clear_bind != 0 &&
            (GetAsyncKeyState(gui_friends_clear_bind) & 0x8000) != 0;
        if (clear_down && !clear_was_down) {
            const size_t count = snapshot_names().size();
            clear();
            const std::string body = std::to_string(count) + " friends removed.";
            TriggerNotification("Friends", body.c_str(), "FRIEND_REMOVE");
        }
        clear_was_down = clear_down;
    }
}

#include "altmanager.h"

bool g_AltManagerMode = false;
std::atomic<int> g_AltManagerAction{ 0 };
std::atomic<bool> g_AltManagerRefocusInput{ false };
char g_AltManagerPendingName[17] = {};
std::string g_AltManagerPremiumName = "Unknown";
std::string g_AltManagerCurrentName = "Unknown";
std::string g_AltManagerStatus;
std::mutex g_AltManagerStateMutex;
jobject g_AltManagerPremiumSession = nullptr;
std::vector<std::pair<jfieldID, jobject>> g_AltManagerPremiumStringFields;
static std::atomic<bool> g_AltAuthBusy{ false };
static HANDLE g_AltAuthThread = nullptr;
static alt_auth::account g_AltManagerPendingAccount;

void ShutdownAltAuthWorker() {
    if (!g_AltAuthThread) return;
    WaitForSingleObject(g_AltAuthThread, 20000);
    CloseHandle(g_AltAuthThread);
    g_AltAuthThread = nullptr;
}

static std::string AltManagerReadJavaString(JNIEnv* env, jstring value) {
    if (!env || !value) return {};
    const char* utf = env->GetStringUTFChars(value, nullptr);
    if (!utf) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return {};
    }
    std::string result(utf);
    env->ReleaseStringUTFChars(value, utf);
    return result;
}

static std::string AltManagerGetClassName(JNIEnv* env, jobject object) {
    if (!env || !object) return {};
    jclass objectClass = env->GetObjectClass(object);
    jclass classClass = env->FindClass("java/lang/Class");
    if (!objectClass || !classClass) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (objectClass) env->DeleteLocalRef(objectClass);
        if (classClass) env->DeleteLocalRef(classClass);
        return {};
    }
    jmethodID getName = env->GetMethodID(classClass, "getName", "()Ljava/lang/String;");
    jstring name = getName ? (jstring)env->CallObjectMethod(objectClass, getName) : nullptr;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        name = nullptr;
    }
    std::string result = AltManagerReadJavaString(env, name);
    if (name) env->DeleteLocalRef(name);
    env->DeleteLocalRef(classClass);
    env->DeleteLocalRef(objectClass);
    return result;
}

static bool AltManagerIsSessionClass(JNIEnv* env, jclass klass) {
    if (!env || !klass || !mapper::jvmti) return false;
    jmethodID ctor = env->GetMethodID(klass, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if (!ctor) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        ctor = env->GetMethodID(klass, "<init>",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    }
    if (!ctor) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return false;
    }
    jint count = 0;
    jfieldID* fields = nullptr;
    if (mapper::jvmti->GetClassFields(klass, &count, &fields) != JVMTI_ERROR_NONE || !fields)
        return false;
    int stringFields = 0;
    for (jint i = 0; i < count; ++i) {
        char* name = nullptr;
        char* signature = nullptr;
        char* generic = nullptr;
        mapper::jvmti->GetFieldName(klass, fields[i], &name, &signature, &generic);
        if (signature && strcmp(signature, "Ljava/lang/String;") == 0)
            ++stringFields;
        if (name) mapper::jvmti->Deallocate((unsigned char*)name);
        if (signature) mapper::jvmti->Deallocate((unsigned char*)signature);
        if (generic) mapper::jvmti->Deallocate((unsigned char*)generic);
    }
    mapper::jvmti->Deallocate((unsigned char*)fields);
    return stringFields >= 3 && stringFields <= 5;
}

bool AltManagerFindSession(JNIEnv* env, jobject minecraft,
                                  jfieldID& outField, jobject& outSession) {
    outField = nullptr;
    outSession = nullptr;
    if (!env || !minecraft || !mapper::jvmti) return false;
    jclass mcClass = env->GetObjectClass(minecraft);
    if (!mcClass) return false;
    jint count = 0;
    jfieldID* fields = nullptr;
    if (mapper::jvmti->GetClassFields(mcClass, &count, &fields) != JVMTI_ERROR_NONE || !fields) {
        env->DeleteLocalRef(mcClass);
        return false;
    }
    for (jint i = 0; i < count && !outSession; ++i) {
        char* name = nullptr;
        char* signature = nullptr;
        char* generic = nullptr;
        mapper::jvmti->GetFieldName(mcClass, fields[i], &name, &signature, &generic);
        jint modifiers = 0;
        mapper::jvmti->GetFieldModifiers(mcClass, fields[i], &modifiers);
        const bool objectField = signature && signature[0] == 'L' && (modifiers & 0x0008) == 0;
        if (objectField) {
            jobject candidate = env->GetObjectField(minecraft, fields[i]);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                candidate = nullptr;
            }
            if (candidate) {
                jclass candidateClass = env->GetObjectClass(candidate);
                const bool knownName = name &&
                    (strcmp(name, "session") == 0 || strcmp(name, "field_71449_j") == 0);
                const std::string candidateClassName = AltManagerGetClassName(env, candidate);
                const bool namedSessionClass =
                    candidateClassName.find("Session") != std::string::npos ||
                    candidateClassName.find("session") != std::string::npos;
                // The old constructor-only heuristic also matched unrelated
                // Lunar objects and could display arbitrary strings as the IGN.
                if (knownName || (namedSessionClass && AltManagerIsSessionClass(env, candidateClass))) {
                    outField = fields[i];
                    outSession = candidate;
                } else {
                    env->DeleteLocalRef(candidate);
                }
                if (candidateClass) env->DeleteLocalRef(candidateClass);
            }
        }
        if (name) mapper::jvmti->Deallocate((unsigned char*)name);
        if (signature) mapper::jvmti->Deallocate((unsigned char*)signature);
        if (generic) mapper::jvmti->Deallocate((unsigned char*)generic);
    }
    mapper::jvmti->Deallocate((unsigned char*)fields);
    env->DeleteLocalRef(mcClass);
    return outField != nullptr && outSession != nullptr;
}

static std::string AltManagerGetSessionName(JNIEnv* env, jobject session) {
    if (!env || !session || !mapper::jvmti) return {};
    jclass sessionClass = env->GetObjectClass(session);
    if (!sessionClass) return {};
    const char* methodNames[] = { "getUsername", "func_111285_a", "a" };
    for (const char* methodName : methodNames) {
        jmethodID method = env->GetMethodID(sessionClass, methodName, "()Ljava/lang/String;");
        if (!method) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            continue;
        }
        jstring value = (jstring)env->CallObjectMethod(session, method);
        if (!env->ExceptionCheck() && value) {
            std::string result = AltManagerReadJavaString(env, value);
            env->DeleteLocalRef(value);
            env->DeleteLocalRef(sessionClass);
            if (!result.empty()) return result;
        } else if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }

    jint count = 0;
    jfieldID* fields = nullptr;
    if (mapper::jvmti->GetClassFields(sessionClass, &count, &fields) == JVMTI_ERROR_NONE && fields) {
        for (jint i = 0; i < count; ++i) {
            char* name = nullptr;
            char* signature = nullptr;
            char* generic = nullptr;
            mapper::jvmti->GetFieldName(sessionClass, fields[i], &name, &signature, &generic);
            const bool isString = signature && strcmp(signature, "Ljava/lang/String;") == 0;
            if (isString) {
                jstring value = (jstring)env->GetObjectField(session, fields[i]);
                if (!env->ExceptionCheck() && value) {
                    std::string result = AltManagerReadJavaString(env, value);
                    env->DeleteLocalRef(value);
                    if (name) mapper::jvmti->Deallocate((unsigned char*)name);
                    if (signature) mapper::jvmti->Deallocate((unsigned char*)signature);
                    if (generic) mapper::jvmti->Deallocate((unsigned char*)generic);
                    mapper::jvmti->Deallocate((unsigned char*)fields);
                    env->DeleteLocalRef(sessionClass);
                    return result;
                }
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
            if (name) mapper::jvmti->Deallocate((unsigned char*)name);
            if (signature) mapper::jvmti->Deallocate((unsigned char*)signature);
            if (generic) mapper::jvmti->Deallocate((unsigned char*)generic);
        }
        mapper::jvmti->Deallocate((unsigned char*)fields);
    }
    env->DeleteLocalRef(sessionClass);
    return {};
}

bool AltManagerSetSessionName(JNIEnv* env, jobject session, const char* username) {
    if (!env || !session || !username || !username[0] || !mapper::jvmti) return false;
    jclass sessionClass = env->GetObjectClass(session);
    if (!sessionClass) return false;

    const std::string oldName = AltManagerGetSessionName(env, session);
    jint count = 0;
    jfieldID* fields = nullptr;
    if (mapper::jvmti->GetClassFields(sessionClass, &count, &fields) != JVMTI_ERROR_NONE || !fields) {
        env->DeleteLocalRef(sessionClass);
        return false;
    }

    jfieldID nameField = nullptr;
    for (jint i = 0; i < count && !nameField; ++i) {
        char* name = nullptr;
        char* signature = nullptr;
        char* generic = nullptr;
        mapper::jvmti->GetFieldName(sessionClass, fields[i], &name, &signature, &generic);
        if (signature && strcmp(signature, "Ljava/lang/String;") == 0) {
            jstring value = (jstring)env->GetObjectField(session, fields[i]);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                value = nullptr;
            }
            const std::string text = AltManagerReadJavaString(env, value);
            if (value) env->DeleteLocalRef(value);
            const bool knownField = name &&
                (strcmp(name, "username") == 0 || strcmp(name, "field_74286_b") == 0);
            if (knownField || (!oldName.empty() && text == oldName))
                nameField = fields[i];
        }
        if (name) mapper::jvmti->Deallocate((unsigned char*)name);
        if (signature) mapper::jvmti->Deallocate((unsigned char*)signature);
        if (generic) mapper::jvmti->Deallocate((unsigned char*)generic);
    }
    mapper::jvmti->Deallocate((unsigned char*)fields);

    bool success = false;
    if (nameField) {
        jstring newName = env->NewStringUTF(username);
        env->SetObjectField(session, nameField, newName);
        success = !env->ExceptionCheck();
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (newName) env->DeleteLocalRef(newName);
    }
    env->DeleteLocalRef(sessionClass);
    return success;
}

static bool AltManagerApplyAuthenticatedSession(JNIEnv* env, jobject minecraft,
                                                jfieldID sessionField, jobject currentSession,
                                                const alt_auth::account& account) {
    if (!env || !minecraft || !sessionField || !currentSession ||
        account.username.empty() || account.uuid.empty() || account.access_token.empty()) return false;
    jclass sessionClass = env->GetObjectClass(currentSession);
    if (!sessionClass) return false;
    jmethodID ctor = env->GetMethodID(sessionClass, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if (!ctor && env->ExceptionCheck()) env->ExceptionClear();
    if (!ctor) { env->DeleteLocalRef(sessionClass); return false; }

    jstring username = env->NewStringUTF(account.username.c_str());
    jstring uuid = env->NewStringUTF(account.uuid.c_str());
    jstring token = env->NewStringUTF(account.access_token.c_str());
    jstring type = env->NewStringUTF("mojang");
    jobject replacement = env->NewObject(sessionClass, ctor, username, uuid, token, type);
    bool success = replacement && !env->ExceptionCheck();
    if (success) {
        env->SetObjectField(minecraft, sessionField, replacement);
        success = !env->ExceptionCheck();
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (replacement) env->DeleteLocalRef(replacement);
    if (username) env->DeleteLocalRef(username);
    if (uuid) env->DeleteLocalRef(uuid);
    if (token) env->DeleteLocalRef(token);
    if (type) env->DeleteLocalRef(type);
    env->DeleteLocalRef(sessionClass);
    return success;
}

static bool AltManagerCapturePremiumSession(JNIEnv* env, jobject session) {
    if (!env || !session || !mapper::jvmti || !g_AltManagerPremiumStringFields.empty()) return false;
    jclass sessionClass = env->GetObjectClass(session);
    if (!sessionClass) return false;

    jint count = 0;
    jfieldID* fields = nullptr;
    if (mapper::jvmti->GetClassFields(sessionClass, &count, &fields) != JVMTI_ERROR_NONE || !fields) {
        env->DeleteLocalRef(sessionClass);
        return false;
    }

    for (jint i = 0; i < count; ++i) {
        char* name = nullptr;
        char* signature = nullptr;
        char* generic = nullptr;
        mapper::jvmti->GetFieldName(sessionClass, fields[i], &name, &signature, &generic);
        if (signature && strcmp(signature, "Ljava/lang/String;") == 0) {
            jobject value = env->GetObjectField(session, fields[i]);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                value = nullptr;
            }
            jobject savedValue = value ? env->NewGlobalRef(value) : nullptr;
            g_AltManagerPremiumStringFields.emplace_back(fields[i], savedValue);
            if (value) env->DeleteLocalRef(value);
        }
        if (name) mapper::jvmti->Deallocate((unsigned char*)name);
        if (signature) mapper::jvmti->Deallocate((unsigned char*)signature);
        if (generic) mapper::jvmti->Deallocate((unsigned char*)generic);
    }
    mapper::jvmti->Deallocate((unsigned char*)fields);
    env->DeleteLocalRef(sessionClass);
    return !g_AltManagerPremiumStringFields.empty();
}

bool AltManagerRestorePremiumSession(JNIEnv* env, jobject session) {
    if (!env || !session || g_AltManagerPremiumStringFields.empty()) return false;
    bool success = true;
    for (const auto& savedField : g_AltManagerPremiumStringFields) {
        env->SetObjectField(session, savedField.first, savedField.second);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            success = false;
        }
    }
    return success;
}

void AltManagerUpdateOnGameThread(JNIEnv* env, jobject minecraft, jobject currentScreen) {
    static ULONGLONG nextScreenCheck = 0;
    static std::string hostScreenName;
    const ULONGLONG now = GetTickCount64();
    const std::string currentScreenName = AltManagerGetClassName(env, currentScreen);
    g_OnMinecraftMainMenu.store(
        currentScreenName.find("GuiMainMenu") != std::string::npos ||
        currentScreenName.find("TitleScreen") != std::string::npos ||
        currentScreenName.find("MainMenuScreen") != std::string::npos,
        std::memory_order_release);
    g_ChatOpen.store(currentScreenName.find("GuiChat") != std::string::npos ||
                     currentScreenName.find("ChatScreen") != std::string::npos,
                     std::memory_order_release);

    // Escape closes Alt Manager while still on GuiMultiplayer and deliberately
    // leaves the native cursor available. Once Minecraft enters a world there
    // is no screen anymore, so return cursor ownership to LWJGL even though Alt
    // Manager mode was already cleared earlier.
    if (!currentScreen && !g_MenuVisible &&
        s_DeferredCursorRelease.load(std::memory_order_acquire)) {
        ReleaseMenuCursorForGameplay();
    }
    if (g_AltManagerMode && g_MenuVisible) {
        if (hostScreenName.empty()) {
            hostScreenName = currentScreenName;
        } else if (!currentScreen || currentScreenName != hostScreenName) {
            g_MenuVisible = false;
            g_AltManagerMode = false;
            hostScreenName.clear();
            MenuClose(false);
        }
    } else if (!g_AltManagerMode) {
        hostScreenName.clear();
    }
    if (now >= nextScreenCheck) {
        nextScreenCheck = now + 250;
        g_OnMultiplayerScreen.store(currentScreenName.find("GuiMultiplayer") != std::string::npos ||
                                    currentScreenName.find("MultiplayerScreen") != std::string::npos ||
                                    currentScreenName.find("ServerSelection") != std::string::npos ||
                                    currentScreenName.find("SelectServer") != std::string::npos,
                                    std::memory_order_release);
    }

    if (g_AltManagerPremiumSession &&
        g_AltManagerAction.load(std::memory_order_acquire) == 0)
        return;

    static jfieldID cachedSessionField = nullptr;
    jfieldID sessionField = nullptr;
    jobject currentSession = nullptr;
    if (cachedSessionField) {
        sessionField = cachedSessionField;
        currentSession = env->GetObjectField(minecraft, sessionField);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            currentSession = nullptr;
            cachedSessionField = nullptr;
        }
    }
    if (!currentSession) {
        if (!AltManagerFindSession(env, minecraft, sessionField, currentSession)) return;
        cachedSessionField = sessionField;
    }
    if (!g_AltManagerPremiumSession) {
        g_AltManagerPremiumSession = env->NewGlobalRef(currentSession);
        AltManagerCapturePremiumSession(env, currentSession);
        const std::string premiumName = AltManagerGetSessionName(env, currentSession);
        if (!premiumName.empty()) {
            std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
            g_AltManagerPremiumName = premiumName;
            g_AltManagerCurrentName = premiumName;
        }
    }

    const int action = g_AltManagerAction.exchange(0, std::memory_order_acq_rel);
    if (action == 1) {
        // Keep Lunar's authenticated session object and token intact. Replacing
        // it with an empty legacy session makes Lunar start Microsoft sign-in.
        if (AltManagerSetSessionName(env, currentSession, g_AltManagerPendingName)) {
            std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
            g_AltManagerCurrentName = g_AltManagerPendingName;
            g_AltManagerStatus = "Added successfully.";
        } else {
            std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
            g_AltManagerStatus = "Could not apply the offline session.";
        }
    } else if (action == 2 && g_AltManagerPremiumSession) {
        // Restore every captured authenticated field, then ALWAYS force the
        // original premium username. The previous short-circuit (`fields ||
        // name`) skipped the name write whenever restoring the other fields
        // succeeded, which could leave a previously selected offline IGN.
        AltManagerRestorePremiumSession(env, currentSession);
        AltManagerSetSessionName(env, currentSession, g_AltManagerPremiumName.c_str());
        const std::string restoredName = AltManagerGetSessionName(env, currentSession);
        const bool restored = !g_AltManagerPremiumName.empty() &&
            g_AltManagerPremiumName != "Unknown" &&
            restoredName == g_AltManagerPremiumName;
        if (!restored) {
            std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
            g_AltManagerStatus = "Could not restore the premium session.";
        } else {
            std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
            g_AltManagerCurrentName = g_AltManagerPremiumName;
            g_AltManagerStatus = "Restored successfully.";
        }
        g_AltManagerRefocusInput.store(true, std::memory_order_release);
    } else if (action == 3) {
        alt_auth::account pending;
        {
            std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
            pending = g_AltManagerPendingAccount;
        }
        const bool applied = AltManagerApplyAuthenticatedSession(
            env, minecraft, sessionField, currentSession, pending);
        {
            std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
            if (applied) {
                g_AltManagerCurrentName = pending.username;
                g_AltManagerStatus = "Logged in successfully as " + pending.username + ".";
            } else {
                g_AltManagerStatus = "Authenticated, but the Minecraft session could not be replaced.";
            }
        }
    }
    env->DeleteLocalRef(currentSession);
}


static bool AltManagerIsValidOfflineName(const char* name) {
    if (!name) return false;
    const size_t length = strlen(name);
    if (length < 3 || length > 16) return false;
    for (size_t i = 0; i < length; ++i) {
        const unsigned char ch = (unsigned char)name[i];
        if (!std::isalnum(ch) && ch != '_') return false;
    }
    return true;
}

struct AltAuthRequest {
    alt_auth::method method;
    std::string input;
};

static void AltManagerSetStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
    g_AltManagerStatus = status;
}

static DWORD WINAPI AltManagerAuthThread(LPVOID parameter) {
    std::unique_ptr<AltAuthRequest> request(static_cast<AltAuthRequest*>(parameter));
    std::vector<alt_auth::result> results;
    const auto progress = [](const std::string& text) { AltManagerSetStatus(text); };
    if (request->method == alt_auth::method::cookie) {
        results.push_back(alt_auth::login_cookie_text(request->input, progress));
    } else {
        const auto credentials = alt_auth::split_credentials(request->input, request->method);
        for (size_t i = 0; i < credentials.size(); ++i) {
            AltManagerSetStatus("Processing account " + std::to_string(i + 1) + " of " +
                std::to_string(credentials.size()) + "...");
            results.push_back(request->method == alt_auth::method::access_token
                ? alt_auth::login_access_token(credentials[i], progress)
                : alt_auth::login_refresh_token(credentials[i], progress));
        }
    }
    alt_auth::secure_clear(request->input);

    int succeeded = 0;
    std::string last_error;
    alt_auth::account last_account;
    {
        std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
        for (auto& result : results) {
            if (!result.success) { last_error = result.error; continue; }
            ++succeeded; last_account = result.value;
        }
        if (succeeded) {
            g_AltManagerPendingAccount = last_account;
            g_AltManagerStatus = succeeded == 1 ? "Authentication completed." :
                std::to_string(succeeded) + " accounts imported. Applying the last one...";
        } else {
            g_AltManagerStatus = last_error.empty() ? "No valid credentials were found." : last_error;
        }
    }
    if (succeeded) {
        g_AltManagerAction.store(3, std::memory_order_release);
    }
    g_AltAuthBusy.store(false, std::memory_order_release);
    return 0;
}

static bool AltManagerStartAuthentication(alt_auth::method method,
                                          const std::string& input) {
    if (g_AltAuthBusy.exchange(true, std::memory_order_acq_rel)) return false;
    if (g_AltAuthThread && WaitForSingleObject(g_AltAuthThread, 0) == WAIT_OBJECT_0) {
        CloseHandle(g_AltAuthThread); g_AltAuthThread = nullptr;
    }
    auto* request = new AltAuthRequest{ method, input };
    g_AltAuthThread = CreateThread(nullptr, 0, AltManagerAuthThread, request, 0, nullptr);
    if (!g_AltAuthThread) {
        alt_auth::secure_clear(request->input); delete request;
        g_AltAuthBusy.store(false, std::memory_order_release);
        return false;
    }
    return true;
}

static std::wstring AltManagerChooseCookieFile() {
    wchar_t path[MAX_PATH]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_GameWindow;
    dialog.lpstrFilter = L"Cookie files (*.txt;*.json;*.cookies)\0*.txt;*.json;*.cookies\0All files\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&dialog) ? std::wstring(path) : std::wstring();
}

static void RenderpandoraAltManager() {
    ImGuiIO& io = ImGui::GetIO();
    g_ConfigInputActive = false;
    
    (void)io;
    
    static float alt_anim = 0.0f;
    const float step = ImClamp(io.DeltaTime * 9.0f, 0.0f, 1.0f);
    alt_anim += g_AltManagerMode ? step : -step;
    alt_anim = ImClamp(alt_anim, 0.0f, 1.0f);
    if (!g_AltManagerMode && alt_anim <= 0.0f) return;
    const float ease = 1.0f - powf(1.0f - alt_anim, 4.0f);

    constexpr float width = 600.0f;
    constexpr float height = 430.0f;
    if (g_MenuVisible) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(
            ImVec2(0.0f, 0.0f), io.DisplaySize,
            IM_COL32(0, 0, 0, 255));
    }
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - width) * 0.5f,
                                  (io.DisplaySize.y - height) * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 26.0f));
    constexpr float altRounding = 28.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, altRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));
    
    // Push the Poppins Bold font to fix the pixelated font!
    FONT_MANAGER.push_ui_font();

    ImGui::Begin("pandoraAltManager", nullptr, ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground |
        (!g_AltManagerMode ? ImGuiWindowFlags_NoInputs : 0));

    // Force arrow cursor so it doesn't disappear when typing
    ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);

    const ImVec2 wp = ImGui::GetWindowPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(wp, ImVec2(wp.x + width, wp.y + height),
                        IM_COL32(20, 20, 20, 255), altRounding);
    draw->AddRect(ImVec2(wp.x + 0.5f, wp.y + 0.5f),
                  ImVec2(wp.x + width - 0.5f, wp.y + height - 0.5f),
                  IM_COL32(35, 35, 35, 255), altRounding - 0.5f, 0, 1.0f);

    std::string premiumName;
    std::string currentName;
    std::string status;
    {
        std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
        premiumName = g_AltManagerPremiumName;
        currentName = g_AltManagerCurrentName;
        status = g_AltManagerStatus;
    }

    const ImVec2 contentMin = ImGui::GetCursorScreenPos();
    const float contentW = ImGui::GetContentRegionAvail().x;
    const ImVec2 tabMin = contentMin;
    const ImVec2 tabMax(contentMin.x + contentW, contentMin.y + 42.0f);
    draw->AddRectFilled(tabMin, tabMax, IM_COL32(15, 15, 15, 255), 12.0f);
    draw->AddRectFilled(ImVec2(tabMin.x + 3.0f, tabMin.y + 3.0f),
        ImVec2(tabMax.x - 3.0f, tabMax.y - 3.0f), IM_COL32(10, 10, 10, 255), 10.0f);
    const char* title = "Alt Manager";
    const ImVec2 titleSize = ImGui::CalcTextSize(title);
    draw->AddText(ImVec2(tabMin.x + (contentW - titleSize.x) * 0.5f,
        tabMin.y + (42.0f - titleSize.y) * 0.5f), IM_COL32(255, 255, 255, 255), title);

    const ImVec2 sessionMin(wp.x + 28.0f, wp.y + 90.0f);
    const ImVec2 sessionMax(sessionMin.x + contentW, sessionMin.y + 74.0f);
    draw->AddRectFilled(sessionMin, sessionMax, IM_COL32(15, 15, 15, 255), 12.0f);
    draw->AddRect(sessionMin, sessionMax, IM_COL32(28, 30, 35, 255), 12.0f);
    draw->AddText(ImVec2(sessionMin.x + 18.0f, sessionMin.y + 14.0f),
        IM_COL32(180, 180, 180, 255), "Current session");
    draw->AddText(ImVec2(sessionMin.x + 18.0f, sessionMin.y + 40.0f),
        IM_COL32(255, 255, 255, 255), currentName.c_str());
    const std::string premiumLabel = "Premium: " + premiumName;
    const ImVec2 premiumSize = ImGui::CalcTextSize(premiumLabel.c_str());
    draw->AddText(ImVec2(sessionMax.x - premiumSize.x - 18.0f, sessionMin.y + 40.0f),
        IM_COL32(255, 255, 255, 255), premiumLabel.c_str());

    static char unifiedInput[65536] = {};
    ImGui::SetCursorPos(ImVec2(28.0f, 188.0f));
    const bool busy = g_AltAuthBusy.load(std::memory_order_acquire);
    ImGui::TextUnformatted("Username / Token");
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 8.0f));
    const bool submitted = ImGui::InputTextMultiline("##alt_unified_input", unifiedInput,
        sizeof(unifiedInput), ImVec2(contentW, 42.0f),
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine);
    ImGui::PopStyleVar();
    if (ImGui::IsItemActive()) g_ConfigInputActive = true;

    ImGui::SetCursorPos(ImVec2(28.0f, 272.0f));
    ImGui::BeginDisabled(busy);
    if (ImGui::Button("Restore Premium", ImVec2(contentW, 40.0f))) {
        g_AltManagerAction.store(2, std::memory_order_release);
        AltManagerSetStatus("Restoring premium session...");
    }
    ImGui::SetCursorPos(ImVec2(28.0f, 322.0f));
    const bool cookieButtonClicked = ImGui::Button("Cookie", ImVec2(contentW, 40.0f));
    ImGui::EndDisabled();

    if (cookieButtonClicked && !busy) {
        const std::wstring cookiePath = AltManagerChooseCookieFile();
        if (!cookiePath.empty()) {
            std::ifstream cookieFile(cookiePath, std::ios::binary);
            if (!cookieFile) {
                AltManagerSetStatus("Could not open the selected cookie file.");
            } else {
                std::string cookieContent((std::istreambuf_iterator<char>(cookieFile)), {});
                if (cookieContent.empty()) AltManagerSetStatus("The selected cookie file is empty.");
                else AltManagerStartAuthentication(alt_auth::method::cookie, cookieContent);
                alt_auth::secure_clear(cookieContent);
            }
        }
    }

    if (submitted && !busy) {
        std::string value(unifiedInput);
        const size_t first = value.find_first_not_of(" \t\r\n");
        const size_t last = value.find_last_not_of(" \t\r\n");
        value = first == std::string::npos ? std::string() : value.substr(first, last - first + 1);

        const bool offlineName = AltManagerIsValidOfflineName(value.c_str());
        const bool refreshToken = value.rfind("M.C", 0) == 0 || value.find("\nM.C") != std::string::npos;
        const bool jwtToken = value.rfind("eyJ", 0) == 0 &&
            std::count(value.begin(), value.end(), '.') >= 2;
        const bool cookieText = value.find("# Netscape HTTP Cookie File") != std::string::npos ||
            value.find("#HttpOnly_") != std::string::npos ||
            (value.find("\"name\"") != std::string::npos && value.find("\"value\"") != std::string::npos) ||
            (value.find('\t') != std::string::npos && value.find("live.com") != std::string::npos);

        bool accepted = false;
        if (value.empty()) {
            AltManagerSetStatus("Enter a username, token, or cookie data.");
        } else if (offlineName) {
            strncpy_s(g_AltManagerPendingName, value.c_str(), _TRUNCATE);
            g_AltManagerAction.store(1, std::memory_order_release);
            AltManagerSetStatus("Applying offline session...");
            accepted = true;
        } else {
            alt_auth::method detectedMethod = alt_auth::method::access_token;
            if (cookieText) detectedMethod = alt_auth::method::cookie;
            else if (refreshToken) detectedMethod = alt_auth::method::refresh_token;
            else if (jwtToken) detectedMethod = alt_auth::method::access_token;
            else {
                AltManagerSetStatus("Input was not recognized as a username or token.");
            }
            if (cookieText || refreshToken || jwtToken)
                accepted = AltManagerStartAuthentication(detectedMethod, value);
        }
        alt_auth::secure_clear(value);
        if (accepted) SecureZeroMemory(unifiedInput, sizeof(unifiedInput));
    }

    if (!status.empty()) {
        ImGui::SetCursorPos(ImVec2(28.0f, height - 48.0f));
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", status.c_str());
    }

    ImGui::End();
	ImGui::PopFont();
    ImGui::PopStyleVar(5);
}


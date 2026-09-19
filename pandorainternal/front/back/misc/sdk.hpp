#pragma once
#include "jni.h"
#include "jvmti.h"
#include "include.hpp"
#include <windows.h>

namespace sdk
{
    extern JavaVM* jvm; // <-- ESTA LÍNEA ES LA QUE FALTA O ESTÁ MAL
    extern JNIEnv* jni;
    extern jvmtiEnv* jvmti;

    bool init();
}

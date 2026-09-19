#include "sdk.hpp"

namespace sdk
{
    // Declaración global de las variables (soluciona el error de extern)
    JavaVM* jvm = nullptr;
    JNIEnv* jni = nullptr;
    jvmtiEnv* jvmti = nullptr;

    // Definición de la función nativa de Java
    typedef jint(JNICALL* GetCreatedJavaVMs_t)(JavaVM**, jsize, jsize*);

    bool init()
    {
        // 1. Encontrar el módulo de Java dentro del proceso de Minecraft
        HMODULE jvm_dll = GetModuleHandleA("jvm.dll");
        if (!jvm_dll) return false;

        // 2. Obtener la dirección de la función que nos da las Máquinas Virtuales activas
        GetCreatedJavaVMs_t JNI_GetCreatedJavaVMs = (GetCreatedJavaVMs_t)GetProcAddress(jvm_dll, "JNI_GetCreatedJavaVMs");
        if (!JNI_GetCreatedJavaVMs) return false;

        JavaVM* vms[1];
        jsize vm_count;

        // 3. Extraer la JavaVM
        if (JNI_GetCreatedJavaVMs(vms, 1, &vm_count) != JNI_OK || vm_count == 0)
            return false;

        // Guardamos la máquina virtual en la variable GLOBAL 'jvm'
        jvm = vms[0];

        // 4. Conectar nuestro hilo (Thread) actual al entorno de Java para obtener el JNIEnv
        jint env_stat = jvm->GetEnv((void**)&jni, JNI_VERSION_1_8);
        if (env_stat == JNI_EDETACHED)
        {
            if (jvm->AttachCurrentThread((void**)&jni, nullptr) != JNI_OK)
                return false;
        }
        else if (env_stat != JNI_OK)
        {
            return false; // Error crítico
        }

        // 5. (Opcional) Obtener JVMTI para funciones avanzadas
        jvm->GetEnv((void**)&jvmti, JVMTI_VERSION_1_2);

        return jni != nullptr;
    }
}

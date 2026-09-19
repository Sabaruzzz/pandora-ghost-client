#include "../hooks.hpp"
#include "../../features/features.hpp"
#include "../../sdk.hpp"
#include <gl/GL.h>

namespace hooks
{
    auto clear(JNIEnv* jni, jclass klass, jint mask, jlong function) -> void
    {
        // Candado para evitar capturar la matriz de la mano en primera persona
        static bool s_captured_depth_only = false;

        const bool has_depth = (mask & 0x0100) != 0; // GL_DEPTH_BUFFER_BIT
        const bool has_color = (mask & 0x4000) != 0; // GL_COLOR_BUFFER_BIT

        // Al inicio de cada frame, Minecraft hace clear a ambos. Reiniciamos el candado.
        if (has_color && has_depth) s_captured_depth_only = false;

        if (jni != nullptr && klass != nullptr && function != 0)
        {
            static auto opengl_module = GetModuleHandleA("opengl32.dll");
            static auto gl_clear_ptr = GetProcAddress(opengl_module, "glClear");

            if ((void*)function == gl_clear_ptr && has_depth)
            {
                const bool is_depth_only = !has_color;

                // Captura SOLO la matriz de entidades, y se bloquea para ignorar la mano
                if (is_depth_only && !s_captured_depth_only)
                {
                    GLint matrix_mode = 0;
                    glGetIntegerv(GL_MATRIX_MODE, &matrix_mode);

                    if (matrix_mode == 0x1700) // GL_MODELVIEW
                    {
                        GLint vp[4] = {};
                        glGetIntegerv(GL_VIEWPORT, vp);

                        if (vp[2] > 0 && vp[3] > 0)
                        {
                            glGetIntegerv(GL_VIEWPORT, features::visual::view_port);
                            glGetDoublev(GL_MODELVIEW_MATRIX, features::visual::model_view_matrix);
                            glGetDoublev(GL_PROJECTION_MATRIX, features::visual::projection_matrix);
                            features::visual::matrix_capture_tick =
                                static_cast<unsigned long long>(GetTickCount64());

                            // Bloqueamos la captura hasta el siguiente frame
                            s_captured_depth_only = true;
                        }
                    }
                }
            }
        }

        // Llamamos al glClear original
        if (function != 0)
        {
            typedef void(__stdcall* PFN_GLCLEAR)(GLuint);
            ((PFN_GLCLEAR)function)(static_cast<GLuint>(mask));
        }
    }
}

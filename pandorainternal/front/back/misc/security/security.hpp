#pragma once
#include <string>

namespace security
{
    std::string get_hwid();
    void copy_to_clipboard(const std::string& text);
    void authenticate();
}

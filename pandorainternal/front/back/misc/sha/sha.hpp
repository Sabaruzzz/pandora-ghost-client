#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <windows.h>
#include <wincrypt.h>

namespace sha
{
    inline std::string hash256(const std::string& input)
    {
        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;
        std::string hash_hex = "";

        if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
                // ARREGLO DE LA ADVERTENCIA: Convertimos explícitamente a DWORD
                if (CryptHashData(hHash, (const BYTE*)input.c_str(), (DWORD)input.length(), 0)) {
                    DWORD cbHashSize = 0, dwCount = sizeof(DWORD);
                    if (CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&cbHashSize, &dwCount, 0)) {
                        std::vector<BYTE> buffer(cbHashSize);
                        if (CryptGetHashParam(hHash, HP_HASHVAL, buffer.data(), &cbHashSize, 0)) {
                            std::ostringstream oss;
                            for (size_t i = 0; i < buffer.size(); ++i) {
                                oss << std::hex << std::setw(2) << std::setfill('0') << (int)buffer[i];
                            }
                            hash_hex = oss.str();
                        }
                    }
                }
                CryptDestroyHash(hHash);
            }
            CryptReleaseContext(hProv, 0);
        }
        return hash_hex;
    }
}

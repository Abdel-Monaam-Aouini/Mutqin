// Minimal SHA-256 (FIPS 180-4), used to verify the Quran source files. No dependencies.
#ifndef MUTQIN_TEXT_SHA256_H
#define MUTQIN_TEXT_SHA256_H

#include <string>

namespace mutqin {

// Lower-case hex SHA-256 digest of `data`.
std::string Sha256Hex(const std::string& data);

}  // namespace mutqin

#endif  // MUTQIN_TEXT_SHA256_H

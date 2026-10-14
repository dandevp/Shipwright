#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// No torch types here; TorchExtract.cpp is the only TU that includes Companion.h.
namespace SohTorch {

// Archive root file holding the SOH_ASSET_VERSION (spec hash) the archive was extracted with.
inline constexpr const char* kAssetVersionFile = "assetVersion";

// Count of .yml files under a version directory. Torch's phase callback fires once per file,
// so this is the progress denominator.
size_t CountAssetFiles(const std::string& ymlDir);

// Extracts rom into destDir. rom must already be big-endian (.z64 order), since config.yml
// only lists hashes of big-endian dumps. Torch picks both the version directory under srcDir and the
// archive name (oot.o2r, oot-mq.o2r) from config.yml by hashing the ROM, so the name it chose
// is returned rather than assumed. Empty if extraction threw or produced no archive.
// Stamps the archive with SOH_ASSET_VERSION. Increments progress once per asset file.
std::string Extract(std::vector<uint8_t> rom, const std::string& srcDir, const std::string& destDir,
                    const std::string& portVersion, std::atomic<size_t>* progress);

} // namespace SohTorch

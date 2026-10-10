#include <filesystem>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <zip.h>

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/utils/StrHash64.h>
#include <ship/window/Window.h>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/PuppetModels.h"
#include "soh/OTRGlobals.h"
#include "soh/SohGui/SohModals.h"

/**
 * Gameplay mods (sent with the client state)
 *
 * Mods that change more than looks and sound (scenes, actors, text, ...) have to match between teammates, else the
 * rooms they share aren't the same. Texture packs, sound packs and Link models (see CoopModels.cpp) don't count.
 * Teammates with different gameplay mods aren't co-op peers, and whoever's mods differ from the room owner's is asked
 * whether to match them. That disables the gameplay mods the owner doesn't have and enables those they have that were
 * disabled this way before, by renaming the files, at the next start since they're in use until then.
 */

#define DISABLED_SUFFIX ".disabled"
#define PENDING_SEPARATOR '|'

namespace {

// Resource types that only change looks or sound (see Fast::ResourceType and SOH::ResourceType)
bool IsLookOrSoundResource(uint32_t type) {
    switch (type) {
        case 0x4F544558: // OTEX, texture
        case 0x4F424749: // OBGI, background image
        case 0x4F415544: // OAUD, audio
        case 0x4F534D50: // OSMP, audio sample
        case 0x4F534654: // OSFT, sound font
        case 0x4F534551: // OSEQ, sequence
            return true;
        default:
            return false;
    }
}

struct ScannedMod {
    std::string hash;
    bool isGameplay;
};

// Scans by path, kept while the file stays the same (scans run on a worker thread, see CoopModels.cpp)
struct CachedScan {
    uintmax_t size;
    std::filesystem::file_time_type modified;
    ScannedMod result;
};
std::mutex sScanCacheMutex;
std::map<std::string, CachedScan> sScanCache;

// Looks into a mod archive: a hash of its contents, and whether it changes gameplay. Only the start of each file is
// read, the rest comes from the zip's directory.
bool ScanModArchiveUncached(const std::string& path, ScannedMod& result) {
    int error = 0;
    zip_t* zip = zip_open(path.c_str(), ZIP_RDONLY, &error);
    if (zip == nullptr) {
        return false;
    }

    std::map<std::string, std::pair<uint32_t, uint64_t>> entries;
    std::vector<std::string> names;
    zip_int64_t count = zip_get_num_entries(zip, 0);
    for (zip_int64_t i = 0; i < count; i++) {
        zip_stat_t stat;
        if (zip_stat_index(zip, i, 0, &stat) == 0 && !std::string_view(stat.name).ends_with("/")) {
            entries[stat.name] = { stat.crc, stat.size };
            names.push_back(stat.name);
        }
    }

    // The Link model doesn't count, aliases included (only those are read whole, they're small)
    auto read = [zip](const std::string& name) {
        std::vector<char> data;
        zip_stat_t stat;
        if (zip_stat(zip, name.c_str(), 0, &stat) != 0 || stat.size > 64 * 1024) {
            return data;
        }
        zip_file_t* file = zip_fopen(zip, name.c_str(), 0);
        if (file != nullptr) {
            data.resize(stat.size);
            if (zip_fread(file, data.data(), stat.size) != (zip_int64_t)stat.size) {
                data.clear();
            }
            zip_fclose(file);
        }
        return data;
    };
    auto modelFiles = PuppetModels::SelectModelFiles(names, read, true);

    result.isGameplay = false;
    for (zip_int64_t i = 0; i < count && !result.isGameplay; i++) {
        zip_stat_t stat;
        if (zip_stat_index(zip, i, 0, &stat) != 0 || std::string_view(stat.name).ends_with("/")) {
            continue;
        }
        std::string name = stat.name;
        std::string modelPath = name.starts_with("alt/") ? name.substr(4) : name;
        if (modelFiles.contains(modelPath) || name == "manifest.json" || name == "version" || name.ends_with(".meta") ||
            name.ends_with(".png") || name.ends_with(".jpg")) {
            continue;
        }

        // Binary resources say what they are at bytes 4 to 7
        uint8_t header[8] = {};
        zip_file_t* file = zip_fopen_index(zip, i, 0);
        bool hasHeader = file != nullptr && zip_fread(file, header, sizeof(header)) == sizeof(header);
        if (file != nullptr) {
            zip_fclose(file);
        }
        uint32_t type = header[0] == 0 ? (header[4] | header[5] << 8 | header[6] << 16 | (uint32_t)header[7] << 24)
                                       : ((uint32_t)header[4] << 24 | header[5] << 16 | header[6] << 8 | header[7]);
        if (!hasHeader || !IsLookOrSoundResource(type)) {
            result.isGameplay = true;
        }
    }
    zip_discard(zip);

    uint64_t crc = INITIAL_CRC64;
    for (auto& [name, info] : entries) {
        crc = update_crc64(name.data(), (uint32_t)name.size(), crc);
        crc = update_crc64(&info, sizeof(info), crc);
    }
    result.hash = fmt::format("{:016x}", crc);
    return true;
}

bool ScanModArchive(const std::string& path, ScannedMod& result) {
    std::error_code error;
    uintmax_t size = std::filesystem::file_size(path, error);
    auto modified = std::filesystem::last_write_time(path, error);
    if (error) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(sScanCacheMutex);
        auto cached = sScanCache.find(path);
        if (cached != sScanCache.end() && cached->second.size == size && cached->second.modified == modified) {
            result = cached->second.result;
            return true;
        }
    }

    if (!ScanModArchiveUncached(path, result)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(sScanCacheMutex);
    sScanCache[path] = { size, modified, result };
    return true;
}

std::string GetModsDirectory() {
    return Ship::Context::LocateFileAcrossAppDirs("mods", appShortName);
}

std::string ModName(const std::filesystem::path& path) {
    std::string name = path.filename().string();
    if (name.ends_with(DISABLED_SUFFIX)) {
        name = name.substr(0, name.size() - std::string_view(DISABLED_SUFFIX).size());
    }
    return std::filesystem::path(name).stem().string();
}

} // namespace

// Renames queued by SyncCoopModsToOwner, applied at start before the mods are mounted (see UpdateModFiles)
void Anchor_ApplyPendingModChanges() {
    std::stringstream stream(CVarGetString(CVAR_REMOTE_ANCHOR("Mods.PendingRenames"), ""));
    std::string from, to;
    while (std::getline(stream, from, PENDING_SEPARATOR) && std::getline(stream, to, PENDING_SEPARATOR)) {
        std::error_code error;
        std::filesystem::rename(from, to, error);
        if (error) {
            SPDLOG_ERROR("[Anchor] Couldn't rename {} to {}: {}", from, to, error.message());
        }
    }
    CVarClear(CVAR_REMOTE_ANCHOR("Mods.PendingRenames"));
}

// Our gameplay mods among the given mod archives, and a hash of them all. Runs on a worker thread (see
// CoopModels.cpp), scans are kept so only new or changed mods are looked into again.
std::string Anchor::ScanCoopMods(const std::vector<std::string>& archivePaths, std::vector<CoopModInfo>& mods) {
    mods.clear();
    for (const std::string& path : archivePaths) {
        ScannedMod scanned;
        if (ScanModArchive(path, scanned) && scanned.isGameplay) {
            mods.push_back({ ModName(path), scanned.hash });
        }
    }

    uint64_t crc = INITIAL_CRC64;
    std::vector<std::string> hashes;
    for (auto& mod : mods) {
        hashes.push_back(mod.hash);
    }
    std::sort(hashes.begin(), hashes.end());
    for (auto& hash : hashes) {
        crc = update_crc64(hash.data(), (uint32_t)hash.size(), crc);
    }
    return hashes.empty() ? "" : fmt::format("{:016x}", crc);
}

nlohmann::json Anchor::PrepCoopMods() {
    nlohmann::json mods = nlohmann::json::array();
    for (auto& mod : ownGameplayMods) {
        mods.push_back({ { "name", mod.name }, { "hash", mod.hash } });
    }
    return mods;
}

// Called when a teammate's client state comes in. If it's the room owner's and their gameplay mods differ from ours,
// we're asked once whether to match them.
void Anchor::OnCoopClientMods(uint32_t clientId) {
    if (!clients.contains(clientId) || clientId != roomState.ownerClientId || clientId == ownClientId ||
        !roomState.coopSync) {
        return;
    }
    AnchorClient& owner = clients[clientId];
    if (owner.modsHash == ownModsHash || coopModsAsked.contains(owner.modsHash)) {
        return;
    }
    coopModsAsked.insert(owner.modsHash);

    // What it takes to match: our gameplay mods they don't have go, theirs we disabled before come back
    std::set<std::string> ownerHashes;
    for (auto& mod : owner.gameplayMods) {
        ownerHashes.insert(mod.hash);
    }
    std::vector<std::pair<std::string, std::string>> renames;
    std::string toDisable, toEnable, missing;
    for (auto& mod : ownGameplayMods) {
        if (!ownerHashes.contains(mod.hash)) {
            toDisable += "\n- " + mod.name;
        }
    }

    std::map<std::string, std::filesystem::path> disabledByHash;
    std::string modsDirectory = GetModsDirectory();
    std::error_code error;
    if (!modsDirectory.empty() && std::filesystem::is_directory(modsDirectory, error)) {
        for (auto& entry : std::filesystem::recursive_directory_iterator(modsDirectory, error)) {
            std::string fileName = entry.path().filename().string();
            ScannedMod scanned;
            if (entry.is_regular_file() && fileName.ends_with(".o2r" DISABLED_SUFFIX) &&
                ScanModArchive(entry.path().string(), scanned)) {
                disabledByHash[scanned.hash] = entry.path();
            }
        }
    }

    auto archives = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->GetArchives();
    for (auto& mod : ownGameplayMods) {
        if (ownerHashes.contains(mod.hash)) {
            continue;
        }
        for (auto& archive : *archives) {
            if (ModName(archive->GetPath()) == mod.name) {
                renames.push_back({ archive->GetPath(), archive->GetPath() + DISABLED_SUFFIX });
                break;
            }
        }
    }
    for (auto& mod : owner.gameplayMods) {
        bool isEnabled = std::any_of(ownGameplayMods.begin(), ownGameplayMods.end(),
                                     [&](const CoopModInfo& own) { return own.hash == mod.hash; });
        if (isEnabled) {
            continue;
        }
        auto disabled = disabledByHash.find(mod.hash);
        if (disabled != disabledByHash.end()) {
            std::string path = disabled->second.string();
            renames.push_back({ path, path.substr(0, path.size() - std::string_view(DISABLED_SUFFIX).size()) });
            toEnable += "\n- " + mod.name;
        } else {
            missing += "\n- " + mod.name;
        }
    }

    std::string message = fmt::format("Your gameplay mods differ from the room owner's ({}), so Co-op Sync is paused "
                                      "between you.",
                                      owner.name);
    if (!toDisable.empty()) {
        message += "\n\nTo disable:" + toDisable;
    }
    if (!toEnable.empty()) {
        message += "\n\nTo enable again:" + toEnable;
    }
    if (!missing.empty()) {
        message += "\n\nYou don't have these, get them from the room owner:" + missing;
    }

    auto modal = std::static_pointer_cast<SohModalWindow>(
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow("Modal Window"));
    if (renames.empty()) {
        modal->RegisterPopup("Gameplay Mods Differ", message, "OK");
        return;
    }
    message += "\n\nMatch them? SoH closes to apply it, start it again afterwards.";
    modal->RegisterPopup("Gameplay Mods Differ", message, "Match and Close", "Not now", [renames]() {
        std::string pending;
        for (auto& [from, to] : renames) {
            pending += from + PENDING_SEPARATOR + to + PENDING_SEPARATOR;
        }
        CVarSetString(CVAR_REMOTE_ANCHOR("Mods.PendingRenames"), pending.c_str());
        Ship::Context::GetRawInstance()->GetConsoleVariables()->Save();
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    });
}

// Gameplay mods disabled to match a room owner come back at the next start
size_t Anchor::RestoreCoopDisabledMods() {
    std::string pending;
    size_t count = 0;
    std::string modsDirectory = GetModsDirectory();
    std::error_code error;
    if (!modsDirectory.empty() && std::filesystem::is_directory(modsDirectory, error)) {
        for (auto& entry : std::filesystem::recursive_directory_iterator(modsDirectory, error)) {
            std::string path = entry.path().string();
            if (entry.is_regular_file() && path.ends_with(".o2r" DISABLED_SUFFIX)) {
                pending += path + PENDING_SEPARATOR +
                           path.substr(0, path.size() - std::string_view(DISABLED_SUFFIX).size()) + PENDING_SEPARATOR;
                count++;
            }
        }
    }
    CVarSetString(CVAR_REMOTE_ANCHOR("Mods.PendingRenames"), pending.c_str());
    Ship::Context::GetRawInstance()->GetConsoleVariables()->Save();
    return count;
}

#include <filesystem>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <zip.h>

#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/utils/StrHash64.h>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/PuppetModels.h"
#include "soh/Notification/Notification.h"
#include "soh/OTRGlobals.h"
#include "soh/SohGui/SohModals.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

/**
 * MODEL_REQUEST, MODEL_CHUNK
 *
 * Our Link model is the Link files of our enabled mods (see PuppetModels.h), or of those we picked in the Anchor menu.
 * They're bundled into one archive, identified by a hash of its contents, which comes with our client state.
 * Teammates who don't have that model yet ask us for it (MODEL_REQUEST) and we send it over in chunks (MODEL_CHUNK),
 * a little every frame so it doesn't hold up the game's own packets. Received models are kept, so each is only sent
 * once.
 */

#define MODEL_CHUNK_SIZE (32 * 1024)      // Bytes per chunk, about 43 KB once encoded
#define MODEL_MAX_SIZE (64 * 1024 * 1024) // Bigger models aren't sent nor accepted
#define MODEL_EXCLUDED_SEPARATOR '|'

namespace {

const std::string BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string EncodeBase64(const uint8_t* data, size_t size) {
    std::string out;
    out.reserve((size + 2) / 3 * 4);
    for (size_t i = 0; i < size; i += 3) {
        uint32_t chunk = data[i] << 16;
        if (i + 1 < size) {
            chunk |= data[i + 1] << 8;
        }
        if (i + 2 < size) {
            chunk |= data[i + 2];
        }
        out += BASE64_CHARS[(chunk >> 18) & 0x3F];
        out += BASE64_CHARS[(chunk >> 12) & 0x3F];
        out += i + 1 < size ? BASE64_CHARS[(chunk >> 6) & 0x3F] : '=';
        out += i + 2 < size ? BASE64_CHARS[chunk & 0x3F] : '=';
    }
    return out;
}

bool DecodeBase64(const std::string& text, std::vector<uint8_t>& out) {
    uint32_t buffer = 0;
    int bits = 0;
    for (char c : text) {
        if (c == '=') {
            break;
        }
        size_t value = BASE64_CHARS.find(c);
        if (value == std::string::npos) {
            return false;
        }
        buffer = (buffer << 6) | (uint32_t)value;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((uint8_t)(buffer >> bits));
        }
    }
    return true;
}

std::string GetModelCacheDirectory() {
    return Ship::Context::GetPathRelativeToAppDirectory("anchor-models", appShortName);
}

std::string GetModelCachePath(const std::string& modelId) {
    return GetModelCacheDirectory() + "/" + modelId + ".o2r";
}

// The id of a model: a hash of its files, so the same model has the same id however it was bundled
std::string HashModelFiles(const std::map<std::string, std::vector<char>>& files) {
    uint64_t crc = INITIAL_CRC64;
    for (auto& [path, data] : files) {
        crc = update_crc64(path.data(), (uint32_t)path.size(), crc);
        crc = update_crc64(data.data(), (uint32_t)data.size(), crc);
    }
    return fmt::format("{:016x}", crc);
}

bool WriteModelArchive(const std::string& path, const std::map<std::string, std::vector<char>>& files) {
    int error = 0;
    zip_t* zip = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (zip == nullptr) {
        return false;
    }
    for (auto& [filePath, data] : files) {
        // The buffers outlive zip_close, which is when libzip reads them
        zip_source_t* source = zip_source_buffer(zip, data.data(), data.size(), 0);
        if (source == nullptr || zip_file_add(zip, filePath.c_str(), source, ZIP_FL_OVERWRITE) < 0) {
            zip_source_free(source);
            zip_discard(zip);
            return false;
        }
    }
    return zip_close(zip) == 0;
}

bool ReadModelArchive(const std::string& path, std::map<std::string, std::vector<char>>& files) {
    int error = 0;
    zip_t* zip = zip_open(path.c_str(), ZIP_RDONLY, &error);
    if (zip == nullptr) {
        return false;
    }
    zip_int64_t count = zip_get_num_entries(zip, 0);
    for (zip_int64_t i = 0; i < count; i++) {
        zip_stat_t stat;
        if (zip_stat_index(zip, i, 0, &stat) != 0 || stat.size > MODEL_MAX_SIZE) {
            zip_discard(zip);
            return false;
        }
        std::vector<char> data(stat.size);
        zip_file_t* file = zip_fopen_index(zip, i, 0);
        if (file == nullptr || zip_fread(file, data.data(), stat.size) != (zip_int64_t)stat.size) {
            if (file != nullptr) {
                zip_fclose(file);
            }
            zip_discard(zip);
            return false;
        }
        zip_fclose(file);
        files[stat.name] = std::move(data);
    }
    zip_discard(zip);
    return true;
}

// Mods are the mounted archives that aren't the game's own
bool IsModArchive(const std::shared_ptr<Ship::Archive>& archive) {
    std::string fileName = std::filesystem::path(archive->GetPath()).filename().string();
    return !archive->HasGameVersion() && fileName != "soh.o2r" && !archive->GetPath().starts_with("anchor-model:");
}

std::vector<std::string> GetExcludedModelMods() {
    std::vector<std::string> excluded;
    std::stringstream stream(CVarGetString(CVAR_REMOTE_ANCHOR("Model.ExcludedMods"), ""));
    std::string name;
    while (std::getline(stream, name, MODEL_EXCLUDED_SEPARATOR)) {
        if (!name.empty()) {
            excluded.push_back(name);
        }
    }
    return excluded;
}

} // namespace

// MARK: - Our model

std::vector<Anchor::CoopModelSource> Anchor::GetCoopModelSources() {
    std::vector<CoopModelSource> sources;
    std::vector<std::string> excluded = GetExcludedModelMods();

    auto archives = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->GetArchives();
    for (auto& archive : *archives) {
        if (!IsModArchive(archive)) {
            continue;
        }

        size_t modelFiles = 0;
        for (auto& [hash, path] : *archive->ListFiles()) {
            std::string modelPath = path.starts_with("alt/") ? path.substr(4) : path;
            if (PuppetModels::IsModelPath(modelPath) && !path.ends_with(".meta")) {
                modelFiles++;
            }
        }
        if (modelFiles == 0) {
            continue;
        }

        CoopModelSource source;
        source.name = std::filesystem::path(archive->GetPath()).filename().string();
        source.modelFiles = modelFiles;
        source.included = std::find(excluded.begin(), excluded.end(), source.name) == excluded.end();
        sources.push_back(source);
    }
    return sources;
}

void Anchor::SetCoopModelSourceIncluded(const std::string& name, bool included) {
    std::vector<std::string> excluded = GetExcludedModelMods();
    std::erase(excluded, name);
    if (!included) {
        excluded.push_back(name);
    }

    std::string value;
    for (const std::string& entry : excluded) {
        value += entry + MODEL_EXCLUDED_SEPARATOR;
    }
    CVarSetString(CVAR_REMOTE_ANCHOR("Model.ExcludedMods"), value.c_str());
    CVarSave();
    ownModelNeedsBuilding = true;
}

// Bundles the Link files of our (picked) mods into one archive. Later mods win, like when they're mounted, and
// alternate assets only count while they're enabled.
void Anchor::BuildOwnCoopModel() {
    ownModelId = "";
    ownModelData = nullptr;

    auto resourceManager = Ship::Context::GetRawInstance()->GetResourceManager();
    bool useAltAssets = resourceManager->IsAltAssetsEnabled();
    std::vector<std::string> excluded = GetExcludedModelMods();

    std::map<std::string, std::vector<char>> files;
    for (auto& archive : *resourceManager->GetArchiveManager()->GetArchives()) {
        std::string name = std::filesystem::path(archive->GetPath()).filename().string();
        if (!IsModArchive(archive) || std::find(excluded.begin(), excluded.end(), name) != excluded.end()) {
            continue;
        }

        // Regular files first, so an archive's alternate assets replace its own regular ones
        for (bool alt : { false, true }) {
            if (alt && !useAltAssets) {
                continue;
            }
            for (auto& [hash, path] : *archive->ListFiles()) {
                if (path.starts_with("alt/") != alt || path.ends_with(".meta")) {
                    continue;
                }
                std::string modelPath = alt ? path.substr(4) : path;
                if (!PuppetModels::IsModelPath(modelPath)) {
                    continue;
                }
                auto file = archive->LoadFile(path);
                if (file != nullptr && file->Buffer != nullptr) {
                    files[modelPath] = *file->Buffer;
                }
            }
        }
    }

    // Our own Link is drawn with it when some of our mods were left out, else our mods give the same anyway
    auto sources = GetCoopModelSources();
    bool leavesModsOut = std::any_of(sources.begin(), sources.end(), [](auto& source) { return !source.included; });

    if (files.empty()) {
        PuppetModels::SetOwnModel("", leavesModsOut);
        SendPacket_UpdateClientState();
        return;
    }

    std::string modelId = HashModelFiles(files);
    std::string path = GetModelCachePath(modelId);
    std::error_code error;
    std::filesystem::create_directories(GetModelCacheDirectory(), error);
    if (!std::filesystem::exists(path) && !WriteModelArchive(path, files)) {
        SPDLOG_ERROR("[Anchor] Couldn't write our model to {}", path);
        SendPacket_UpdateClientState();
        return;
    }

    std::ifstream stream(path, std::ios::binary);
    auto data = std::make_shared<std::vector<uint8_t>>(std::istreambuf_iterator<char>(stream),
                                                       std::istreambuf_iterator<char>());
    if (data->size() > MODEL_MAX_SIZE) {
        Notification::Emit({ .message = "Your Link model is too big to be sent to teammates (over 64 MB)" });
        SendPacket_UpdateClientState();
        return;
    }

    ownModelId = modelId;
    ownModelData = data;
    PuppetModels::Load(modelId, path);
    PuppetModels::SetOwnModel(modelId, leavesModsOut);
    SendPacket_UpdateClientState();
}

// Called once a frame, connected or not, since the model picked in the Anchor menu is our own Link's too
void Anchor::UpdateOwnCoopModel() {
    // Our model is what we have equipped, which changes with alternate assets (the TAB key)
    static bool sAltAssetsWhenBuilt = false;
    bool altAssets = Ship::Context::GetRawInstance()->GetResourceManager()->IsAltAssetsEnabled();
    if (altAssets != sAltAssetsWhenBuilt) {
        sAltAssetsWhenBuilt = altAssets;
        ownModelNeedsBuilding = true;
    }
    if (ownModelNeedsBuilding) {
        ownModelNeedsBuilding = false;
        ScanCoopMods();
        BuildOwnCoopModel();
    }
}

static void RegisterOwnCoopModel() {
    COND_HOOK(OnGameFrameUpdate, true, []() {
        if (Anchor::Instance != nullptr) {
            Anchor::Instance->UpdateOwnCoopModel();
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterOwnCoopModel);

// MARK: - Teammates' models

// Called when a teammate's client state comes in. Their model is loaded if we have it, else asked for.
void Anchor::OnCoopClientModel(uint32_t clientId) {
    if (!clients.contains(clientId)) {
        return;
    }
    AnchorClient& client = clients[clientId];
    const std::string modelId = client.modelId;
    auto download = coopModelDownloads.find(modelId);
    if (download != coopModelDownloads.end() &&
        std::chrono::steady_clock::now() - download->second.lastChunkTime > std::chrono::seconds(30)) {
        coopModelDownloads.erase(download);
    }
    if (client.self || modelId.empty() || PuppetModels::IsLoaded(modelId) || coopModelDownloads.contains(modelId)) {
        return;
    }

    // Only ids we made ourselves are used as file names
    if (modelId.size() != 16 || modelId.find_first_not_of("0123456789abcdef") != std::string::npos) {
        return;
    }

    if (std::filesystem::exists(GetModelCachePath(modelId))) {
        PuppetModels::Load(modelId, GetModelCachePath(modelId));
        return;
    }

    if (client.modelSize > MODEL_MAX_SIZE || coopModelsDeclined.contains(modelId)) {
        return;
    }

    switch (CVarGetInteger(CVAR_REMOTE_ANCHOR("Model.Download"), 0)) {
        case 1: // Always
            SendPacket_ModelRequest(clientId, modelId);
            return;
        case 2: // Never
            return;
    }

    // Ask, once per model
    coopModelsDeclined.insert(modelId);
    std::string question = fmt::format("{} uses their own Link model ({:.1f} MB). Download it to see them with it?",
                                       client.name, client.modelSize / (1024.0f * 1024.0f));
    auto modal = std::static_pointer_cast<SohModalWindow>(
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow("Modal Window"));
    modal->RegisterPopup(
        "Teammate's Model", question, "Download", "Not now",
        [this, clientId, modelId]() {
            coopModelsDeclined.erase(modelId);
            SendPacket_ModelRequest(clientId, modelId);
        },
        nullptr);
}

void Anchor::SendPacket_ModelRequest(uint32_t clientId, const std::string& modelId) {
    coopModelDownloads[modelId] = { clientId, std::chrono::steady_clock::now(), {} };

    nlohmann::json payload;
    payload["type"] = MODEL_REQUEST;
    payload["targetClientId"] = clientId;
    payload["modelId"] = modelId;
    payload["quiet"] = true;
    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_ModelRequest(nlohmann::json payload) {
    uint32_t clientId = payload.value("clientId", (uint32_t)0);
    if (ownModelData == nullptr || payload.value("modelId", "") != ownModelId) {
        return;
    }

    // Already being sent to them
    for (const CoopModelUpload& upload : coopModelUploads) {
        if (upload.clientId == clientId) {
            return;
        }
    }
    coopModelUploads.push_back({ clientId, ownModelId, ownModelData, 0 });
}

// Called once a frame: sends the next chunk of a model a teammate asked for
void Anchor::SendCoopModelChunks() {
    if (coopModelUploads.empty()) {
        return;
    }

    CoopModelUpload& upload = coopModelUploads.front();
    size_t size = std::min((size_t)MODEL_CHUNK_SIZE, upload.data->size() - upload.offset);

    nlohmann::json payload;
    payload["type"] = MODEL_CHUNK;
    payload["targetClientId"] = upload.clientId;
    payload["modelId"] = upload.modelId;
    payload["offset"] = upload.offset;
    payload["total"] = upload.data->size();
    payload["data"] = EncodeBase64(upload.data->data() + upload.offset, size);
    payload["quiet"] = true;
    SendJsonToRemote(payload);

    upload.offset += size;
    if (upload.offset >= upload.data->size()) {
        coopModelUploads.erase(coopModelUploads.begin());
    }
}

void Anchor::HandlePacket_ModelChunk(nlohmann::json payload) {
    std::string modelId = payload.value("modelId", "");
    auto it = coopModelDownloads.find(modelId);
    if (it == coopModelDownloads.end()) {
        return;
    }
    CoopModelDownload& download = it->second;
    download.lastChunkTime = std::chrono::steady_clock::now();

    size_t offset = payload.value("offset", (size_t)0);
    size_t total = payload.value("total", (size_t)0);
    if (offset != download.data.size() || total > MODEL_MAX_SIZE ||
        !DecodeBase64(payload.value("data", ""), download.data) || download.data.size() > total) {
        SPDLOG_ERROR("[Anchor] Model {} came in broken, dropped", modelId);
        coopModelDownloads.erase(it);
        return;
    }
    if (download.data.size() < total) {
        return;
    }

    // Complete: kept only if it's really the model it says it is
    std::string path = GetModelCachePath(modelId);
    std::string tempPath = path + ".part";
    std::error_code error;
    std::filesystem::create_directories(GetModelCacheDirectory(), error);
    {
        std::ofstream stream(tempPath, std::ios::binary | std::ios::trunc);
        stream.write((const char*)download.data.data(), download.data.size());
    }
    coopModelDownloads.erase(it);

    std::map<std::string, std::vector<char>> files;
    bool isValid = ReadModelArchive(tempPath, files) && !files.empty() && HashModelFiles(files) == modelId;
    for (auto& [filePath, data] : files) {
        isValid = isValid && PuppetModels::IsModelPath(filePath);
    }
    if (!isValid) {
        SPDLOG_ERROR("[Anchor] Model {} doesn't match its id, dropped", modelId);
        std::filesystem::remove(tempPath, error);
        return;
    }

    std::filesystem::rename(tempPath, path, error);
    if (!error && PuppetModels::Load(modelId, path)) {
        for (auto& [clientId, client] : clients) {
            if (client.modelId == modelId) {
                Notification::Emit({ .prefix = client.name, .message = "Model downloaded" });
                break;
            }
        }
    }
}

#include "PuppetModels.h"

#include <chrono>
#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/resource/archive/O2rArchive.h>
#include <ship/utils/StrHash64.h>
#include <fast/resource/type/DisplayList.h>

#include "soh/resource/type/Skeleton.h"
#include "soh/OTRGlobals.h"
#include "soh/resource/type/SkeletonLimb.h"

extern "C" {
s32 Player_OverrideLimbDrawPause(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* arg);
#include <soh_assets.h>
#include <objects/object_link_boy/object_link_boy.h>
#include <objects/object_link_child/object_link_child.h>
#include "z64.h"
#include "z64player.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
}

namespace PuppetModels {

namespace {

constexpr std::string_view OTR_SIGNATURE = "__OTR__";
constexpr std::string_view ALT_PREFIX = "alt/";
constexpr std::string_view META_SUFFIX = ".meta";

// How long preparing models may take each frame, the rest waits for the next one
constexpr auto PREPARE_BUDGET = std::chrono::milliseconds(3);

std::string StripSignature(std::string_view path) {
    if (path.starts_with(OTR_SIGNATURE)) {
        path.remove_prefix(OTR_SIGNATURE.size());
    }
    return std::string(path);
}

// Model mods usually ship their files as alternate assets, they replace the regular path all the same
std::string WithoutAltPrefix(const std::string& path) {
    return path.starts_with(ALT_PREFIX) ? path.substr(ALT_PREFIX.size()) : path;
}

// The path a `.meta` alias file points at, empty if it has none (it's then the header of the file next to it)
bool ReadMetaTarget(const std::vector<char>& data, nlohmann::json& meta, std::string& target) {
    std::string text(data.data(), strnlen(data.data(), data.size()));
    meta = nlohmann::json::parse(text, nullptr, false);
    if (!meta.is_object() || (meta.contains("path") && !meta["path"].is_string())) {
        return false;
    }
    target = meta.value("path", "");
    return true;
}

std::vector<char> ReadArchiveFile(const std::shared_ptr<Ship::Archive>& archive, const std::string& path) {
    auto file = archive->LoadFile(path);
    return file != nullptr && file->Buffer != nullptr ? *file->Buffer : std::vector<char>();
}

std::string Dirname(const std::string& path) {
    size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "" : path.substr(0, slash);
}

/**
 * Presents a model's files of another archive (see SelectModelFiles) under a path prefix, so they can be mounted next
 * to everything else without replacing anything. Its `.meta` aliases are pointed at the prefixed paths.
 */
class PrefixedArchive final : public Ship::Archive {
  public:
    PrefixedArchive(std::shared_ptr<Ship::Archive> inner, std::string prefix)
        : Archive("anchor-model:" + prefix), mInner(std::move(inner)), mPrefix(std::move(prefix)) {
    }

    bool Open() override {
        std::vector<std::string> paths;
        for (auto& [hash, path] : *mInner->ListFiles()) {
            paths.push_back(path);
        }
        auto read = [this](const std::string& path) { return ReadArchiveFile(mInner, path); };
        mFiles = SelectModelFiles(paths, read, true);

        for (auto& [modelPath, innerPath] : mFiles) {
            if (!modelPath.ends_with(META_SUFFIX)) {
                continue;
            }
            nlohmann::json meta;
            std::string target;
            ReadMetaTarget(read(innerPath), meta, target);
            if (!target.empty()) {
                mAliasTargets.insert(WithoutAltPrefix(target));
                meta["path"] = mPrefix + WithoutAltPrefix(target);
            }
            std::string text = meta.dump();
            std::vector<char>& data = mMetas[modelPath] = std::vector<char>(text.begin(), text.end());
            data.push_back('\0');
        }

        for (auto& [modelPath, innerPath] : mFiles) {
            IndexFile(mPrefix + modelPath);
        }

        // Archives of another game version than the loaded one are refused, this one has the same as the game
        auto versions = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->GetGameVersions();
        if (!versions.empty()) {
            SetGameVersion(versions[0]);
        }
        return true;
    }

    bool Close() override {
        return true;
    }

    bool WriteFile(const std::string& filename, const std::vector<uint8_t>& data) override {
        return false;
    }

    std::shared_ptr<Ship::File> LoadFile(const std::string& filePath) override {
        if (!filePath.starts_with(mPrefix)) {
            return nullptr;
        }
        std::string modelPath = filePath.substr(mPrefix.size());
        auto meta = mMetas.find(modelPath);
        if (meta != mMetas.end()) {
            auto file = std::make_shared<Ship::File>();
            file->Buffer = std::make_shared<std::vector<char>>(meta->second);
            return file;
        }
        auto it = mFiles.find(modelPath);
        return it != mFiles.end() ? mInner->LoadFile(it->second) : nullptr;
    }

    std::shared_ptr<Ship::File> LoadFile(uint64_t hash) override {
        auto files = ListFiles();
        auto it = files->find(hash);
        return it != files->end() ? LoadFile(it->second) : nullptr;
    }

    // Whether this model has a file (or an alias in its place), by its regular path
    bool Has(const std::string& modelPath) const {
        return mFiles.contains(modelPath) || mMetas.contains(modelPath + std::string(META_SUFFIX));
    }

    // Files that are only there as what an alias points at
    bool IsAliasTarget(const std::string& modelPath) const {
        return mAliasTargets.contains(modelPath);
    }

    const std::map<std::string, std::string>& GetFiles() const {
        return mFiles;
    }

    const std::shared_ptr<Ship::Archive>& GetInner() const {
        return mInner;
    }

  private:
    std::shared_ptr<Ship::Archive> mInner;
    std::string mPrefix;
    std::map<std::string, std::string> mFiles;       // Regular path -> path in the inner archive
    std::map<std::string, std::vector<char>> mMetas; // Aliases, pointed at the prefixed paths
    std::set<std::string> mAliasTargets;
};

struct Model {
    std::string prefix;
    std::shared_ptr<PrefixedArchive> archive;
    // The regular path of each of the model's files by the hashes they may be referred to with
    std::unordered_map<uint64_t, std::string> pathsByHash;
    // Paths written into the model's display lists and limbs, kept alive as long as the model
    std::list<std::string> strings;
    // Its files are loaded a few each frame (see Update), then the references between them are fixed
    std::vector<std::string> toLoad;
    size_t loaded = 0;
    std::vector<std::shared_ptr<Ship::IResource>> resources;
    bool isReady = false;
};

std::map<std::string, Model> sModels;

struct Scope {
    Model* model;   // The dummy player's model, nullptr for the vanilla one
    Model* vanilla; // Always there while drawing
    int32_t tunic;
};
Scope* sScope = nullptr;

std::string sOwnModelId;
bool sDrawOwnLink = false;

std::chrono::steady_clock::time_point sPreviewUntil;
s16 sPreviewTurn = 0;

// Interned results of PuppetModels_Resolve, so the returned pointers stay valid
std::unordered_set<std::string> sResolvedPaths;

std::shared_ptr<Ship::ResourceManager> GetResourceManager() {
    return Ship::Context::GetRawInstance()->GetResourceManager();
}

// The base game archive the vanilla model comes from
std::shared_ptr<Ship::Archive> FindBaseArchive() {
    // Kept in a variable: a temporary in the range-for would be gone before the loop runs
    auto archives = GetResourceManager()->GetArchiveManager()->GetArchives();
    for (auto& archive : *archives) {
        if (archive->HasGameVersion()) {
            return archive;
        }
    }
    return nullptr;
}

// Where a model looks a Link file up: its own copy, else the vanilla one. Files that aren't Link's keep their path.
std::string FindTarget(const Model& model, const std::string& path) {
    std::string modelPath = WithoutAltPrefix(path);
    if (model.archive->Has(modelPath)) {
        return model.prefix + modelPath;
    }
    auto vanilla = sModels.find(VANILLA_MODEL_ID);
    if (IsModelPath(modelPath) && vanilla != sModels.end() && vanilla->second.archive->Has(modelPath)) {
        return vanilla->second.prefix + modelPath;
    }
    return "";
}

// The regular path a hash in one of the model's display lists refers to
std::string PathFromHash(const Model& model, uint64_t hash) {
    auto it = model.pathsByHash.find(hash);
    if (it != model.pathsByHash.end()) {
        return it->second;
    }
    const char* path = GetResourceManager()->GetArchiveManager()->HashToCString(hash);
    return path != nullptr ? std::string(path) : "";
}

void FixDisplayList(Model& model, Fast::DisplayList& displayList) {
    auto& instructions = displayList.Instructions;
    for (size_t i = 0; i < instructions.size(); i++) {
        Gfx& command = instructions[i];
        uint8_t opcode = (uint8_t)(command.words.w0 >> 24);

        switch (opcode) {
            // 128 bit commands, the second half is the hash of the path they refer to
            case G_SETTIMG_OTR_HASH:
            case G_DL_OTR_HASH:
            case G_VTX_OTR_HASH:
            case G_MTX_OTR:
            case G_MOVEMEM_OTR:
            case G_BRANCH_Z_OTR: {
                if (i + 1 >= instructions.size()) {
                    break;
                }
                Gfx& hashWords = instructions[++i];
                uint64_t hash = ((uint64_t)hashWords.words.w0 << 32) + (uint32_t)hashWords.words.w1;
                std::string target = FindTarget(model, PathFromHash(model, hash));
                if (!target.empty()) {
                    uint64_t newHash = CRC64(target.c_str());
                    hashWords.words.w0 = (uint32_t)(newHash >> 32);
                    hashWords.words.w1 = (uint32_t)newHash;
                }
                break;
            }
            case G_MARKER:
                i++;
                break;
            // Commands that refer to a path string
            case G_DL_OTR_FILEPATH:
            case G_SETTIMG_OTR_FILEPATH:
            case G_VTX_OTR_FILEPATH:
            case G_MTX_OTR_FILEPATH: {
                const char* path = (const char*)command.words.w1;
                if (path == nullptr) {
                    break;
                }
                std::string target = FindTarget(model, StripSignature(path));
                if (!target.empty()) {
                    bool hasSignature = std::string_view(path).starts_with(OTR_SIGNATURE);
                    model.strings.push_back(hasSignature ? std::string(OTR_SIGNATURE) + target : target);
                    command.words.w1 = (uintptr_t)model.strings.back().c_str();
                }
                break;
            }
        }
    }
}

void FixLimb(Model& model, SOH::SkeletonLimb& limb) {
    auto fix = [&](std::string& path) {
        if (path.empty()) {
            return false;
        }
        std::string target = FindTarget(model, StripSignature(path));
        if (target.empty()) {
            return false;
        }
        path = std::string(OTR_SIGNATURE) + target;
        return true;
    };

    // The limb data points into these strings, so it's pointed at the new ones like the limb factory does
    switch (limb.limbType) {
        case SOH::LimbType::LOD:
            if (fix(limb.dListPtr)) {
                limb.limbData.lodLimb.dLists[0] = (Gfx*)limb.dListPtr.c_str();
            }
            if (fix(limb.dList2Ptr)) {
                limb.limbData.lodLimb.dLists[1] = (Gfx*)limb.dList2Ptr.c_str();
            }
            break;
        case SOH::LimbType::Standard:
        case SOH::LimbType::Legacy:
            if (fix(limb.dListPtr)) {
                limb.limbData.standardLimb.dList = (Gfx*)limb.dListPtr.c_str();
            }
            break;
        case SOH::LimbType::Curve:
            if (fix(limb.dListPtr)) {
                limb.limbData.skelCurveLimb.dList[0] = (Gfx*)limb.dListPtr.c_str();
            }
            if (fix(limb.dList2Ptr)) {
                limb.limbData.skelCurveLimb.dList[1] = (Gfx*)limb.dList2Ptr.c_str();
            }
            break;
        case SOH::LimbType::Skin:
            if (limb.skinSegmentType == SOH::ZLimbSkinType::SkinType_DList && fix(limb.skinDList)) {
                limb.limbData.skinLimb.segment = (Gfx*)limb.skinDList.c_str();
            } else if (limb.skinSegmentType == SOH::ZLimbSkinType::SkinType_4 && fix(limb.skinDList2)) {
                limb.skinAnimLimbData.dlist = (Gfx*)limb.skinDList2.c_str();
            }
            break;
        default:
            break;
    }
}

void FixSkeleton(Model& model, SOH::Skeleton& skeleton) {
    for (size_t i = 0; i < skeleton.limbTable.size() && i < skeleton.skeletonHeaderSegments.size(); i++) {
        std::string target = FindTarget(model, StripSignature(skeleton.limbTable[i]));
        if (target.empty()) {
            continue;
        }
        auto limb = GetResourceManager()->LoadResource(target, true);
        if (limb != nullptr) {
            skeleton.skeletonHeaderSegments[i] = limb->GetRawPointer();
        }
    }
}

// Loads some more of the model's files, until the time is up. Once they're all loaded, the references between them are
// pointed at its own copies.
void Prepare(Model& model, std::chrono::steady_clock::time_point deadline) {
    auto resourceManager = GetResourceManager();
    while (model.loaded < model.toLoad.size()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return;
        }
        auto resource = resourceManager->LoadResource(model.prefix + model.toLoad[model.loaded++], true);
        if (resource != nullptr) {
            model.resources.push_back(resource);
        }
    }

    // Skeletons load their limbs, so the limbs are fixed first
    for (auto& resource : model.resources) {
        if (auto displayList = std::dynamic_pointer_cast<Fast::DisplayList>(resource)) {
            FixDisplayList(model, *displayList);
        } else if (auto limb = std::dynamic_pointer_cast<SOH::SkeletonLimb>(resource)) {
            FixLimb(model, *limb);
        }
    }
    for (auto& resource : model.resources) {
        if (auto skeleton = std::dynamic_pointer_cast<SOH::Skeleton>(resource)) {
            FixSkeleton(model, *skeleton);
        }
    }
    model.resources.clear();
    model.isReady = true;
    SPDLOG_INFO("[PuppetModels] Prepared model {}", model.prefix);
}

bool Mount(const std::string& modelId, std::shared_ptr<Ship::Archive> inner) {
    Model model;
    model.prefix = "anchor/" + modelId + "/";
    model.archive = std::make_shared<PrefixedArchive>(std::move(inner), model.prefix);
    model.archive->Load();
    if (!model.archive->IsLoaded()) {
        SPDLOG_ERROR("[PuppetModels] Couldn't mount model {}", modelId);
        return false;
    }
    {
        auto lock = OTRAudio_LockForArchiveChanges();
        if (GetResourceManager()->GetArchiveManager()->AddArchive(model.archive) == nullptr) {
            SPDLOG_ERROR("[PuppetModels] Couldn't mount model {}", modelId);
            return false;
        }
    }

    for (auto& [modelPath, innerPath] : model.archive->GetFiles()) {
        // An alias stands in for the file at its own path, what it points at loads through it
        bool isMeta = modelPath.ends_with(META_SUFFIX);
        std::string path = isMeta ? modelPath.substr(0, modelPath.size() - META_SUFFIX.size()) : modelPath;
        if (model.archive->IsAliasTarget(modelPath)) {
            continue;
        }
        model.pathsByHash[CRC64(path.c_str())] = path;
        model.pathsByHash[CRC64((std::string(ALT_PREFIX) + path).c_str())] = path;
        if (!isMeta) {
            model.pathsByHash[CRC64(innerPath.c_str())] = path;
        }
        model.toLoad.push_back(path);
    }

    sModels[modelId] = std::move(model);
    SPDLOG_INFO("[PuppetModels] Mounted model {} ({} files)", modelId, sModels[modelId].toLoad.size());
    return true;
}

bool EnsureVanillaModel() {
    if (sModels.contains(VANILLA_MODEL_ID)) {
        return true;
    }
    auto baseArchive = FindBaseArchive();
    return baseArchive != nullptr && Mount(VANILLA_MODEL_ID, baseArchive);
}

} // namespace

bool IsModelPath(const std::string& path) {
    return path.starts_with("objects/object_link_boy") || path.starts_with("objects/object_link_child") ||
           path.starts_with("objects/object_custom_equip");
}

std::map<std::string, std::string> SelectModelFiles(const std::vector<std::string>& paths, const FileReader& read,
                                                    bool useAltAssets) {
    // Every file by the path it's used at, alternate assets in place of the regular ones
    std::map<std::string, std::string> all;
    for (const std::string& path : paths) {
        bool isAlt = path.starts_with(ALT_PREFIX);
        std::string usedPath = WithoutAltPrefix(path);
        if ((!isAlt || useAltAssets) && (!all.contains(usedPath) || isAlt) && !path.ends_with("/")) {
            all[usedPath] = path;
        }
    }

    std::map<std::string, std::string> selected;
    std::set<std::string> targetFolders;
    for (auto& [usedPath, path] : all) {
        bool isMeta = usedPath.ends_with(META_SUFFIX);
        if (!IsModelPath(isMeta ? usedPath.substr(0, usedPath.size() - META_SUFFIX.size()) : usedPath)) {
            continue;
        }
        if (isMeta) {
            nlohmann::json meta;
            std::string target;
            if (!ReadMetaTarget(read(path), meta, target)) {
                continue;
            }
            if (!target.empty()) {
                auto targetFile = all.find(WithoutAltPrefix(target));
                if (targetFile == all.end()) {
                    continue;
                }
                selected.insert(*targetFile);
                if (!Dirname(targetFile->first).empty()) {
                    targetFolders.insert(Dirname(targetFile->first));
                }
            }
        }
        selected[usedPath] = path;
    }

    // What an alias points at usually comes with its vertices and textures next to it
    for (auto& [usedPath, path] : all) {
        if (targetFolders.contains(Dirname(usedPath))) {
            selected.emplace(usedPath, path);
        }
    }
    return selected;
}

bool Load(const std::string& modelId, const std::string& archivePath) {
    if (modelId.empty() || modelId == VANILLA_MODEL_ID || sModels.contains(modelId)) {
        return sModels.contains(modelId);
    }
    if (!EnsureVanillaModel()) {
        return false;
    }

    auto archive = std::make_shared<Ship::O2rArchive>(archivePath);
    archive->Load();
    if (!archive->IsLoaded()) {
        SPDLOG_ERROR("[PuppetModels] Couldn't open model archive {}", archivePath);
        return false;
    }
    return Mount(modelId, archive);
}

void Unload(const std::string& modelId) {
    auto it = sModels.find(modelId);
    if (it == sModels.end() || modelId == VANILLA_MODEL_ID) {
        return;
    }
    // Its resources stay cached under its own paths, nothing else refers to them
    {
        auto lock = OTRAudio_LockForArchiveChanges();
        GetResourceManager()->GetArchiveManager()->RemoveArchive(it->second.archive);
    }
    sModels.erase(it);
}

bool IsLoaded(const std::string& modelId) {
    return sModels.contains(modelId);
}

bool IsReady(const std::string& modelId) {
    auto it = sModels.find(modelId);
    return it != sModels.end() && it->second.isReady;
}

void Update() {
    auto deadline = std::chrono::steady_clock::now() + PREPARE_BUDGET;
    // The vanilla model first, the others fall back on it
    auto vanilla = sModels.find(VANILLA_MODEL_ID);
    if (vanilla != sModels.end() && !vanilla->second.isReady) {
        Prepare(vanilla->second, deadline);
        return;
    }
    for (auto& [modelId, model] : sModels) {
        if (!model.isReady) {
            Prepare(model, deadline);
            return;
        }
    }
}

// Models that are still being prepared aren't drawn with yet, nor is anything before the vanilla model is ready
bool BeginScope(const std::string& modelId, int32_t tunic) {
    static Scope scope;
    if (sScope != nullptr || !EnsureVanillaModel() || !IsReady(VANILLA_MODEL_ID)) {
        return false;
    }
    auto model = sModels.find(modelId);
    scope.model =
        model != sModels.end() && modelId != VANILLA_MODEL_ID && model->second.isReady ? &model->second : nullptr;
    scope.vanilla = &sModels[VANILLA_MODEL_ID];
    scope.tunic = tunic;
    sScope = &scope;
    return true;
}

DrawScope::DrawScope(const std::string& modelId, int32_t tunic) {
    BeginScope(modelId, tunic);
}

DrawScope::~DrawScope() {
    sScope = nullptr;
}

void SetOwnModel(const std::string& modelId, bool drawOwnLink) {
    bool wasDrawingOwnLink = sDrawOwnLink;
    sOwnModelId = modelId;
    sDrawOwnLink = drawOwnLink;

    // Back to the skeletons our mods give, which SkeletonPatcher keeps on our own Link
    if (wasDrawingOwnLink && !drawOwnLink) {
        SOH::SkeletonPatcher::UpdateSkeletons();
        SOH::SkeletonPatcher::UpdateCustomSkeletons();
    }
}

void ShowPreview() {
    sPreviewUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
}

void ApplySkeleton(SkelAnime* skelAnime, int32_t linkAge, int32_t tunic) {
    if (sScope == nullptr) {
        return;
    }

    static const char* adultTunicSkeletons[] = { gLinkAdultKokiriTunicSkel, gLinkAdultGoronTunicSkel,
                                                 gLinkAdultZoraTunicSkel };
    static const char* childTunicSkeletons[] = { gLinkChildKokiriTunicSkel, gLinkChildGoronTunicSkel,
                                                 gLinkChildZoraTunicSkel };
    bool isAdult = linkAge == LINK_AGE_ADULT;
    const char* baseSkeleton = isAdult ? gLinkAdultSkel : gLinkChildSkel;

    // The model's skeleton for this tunic, its regular one, or the vanilla one
    std::vector<std::string> candidates;
    if (sScope->model != nullptr) {
        if (tunic >= PLAYER_TUNIC_KOKIRI && tunic <= PLAYER_TUNIC_ZORA) {
            candidates.push_back(StripSignature((isAdult ? adultTunicSkeletons : childTunicSkeletons)[tunic]));
        }
        candidates.push_back(StripSignature(baseSkeleton));
    }

    SOH::Skeleton* skeleton = nullptr;
    for (const std::string& path : candidates) {
        if (sScope->model->archive->Has(path)) {
            skeleton = (SOH::Skeleton*)GetResourceManager()->LoadResource(sScope->model->prefix + path, true).get();
            break;
        }
    }
    if (skeleton == nullptr) {
        skeleton = (SOH::Skeleton*)GetResourceManager()
                       ->LoadResource(sScope->vanilla->prefix + StripSignature(baseSkeleton), true)
                       .get();
    }
    if (skeleton == nullptr || skelAnime->skeleton == skeleton->skeletonData.skeletonHeader.segment) {
        return;
    }

    // Like SkeletonPatcher does for our own player
    skelAnime->skeleton = skeleton->skeletonData.skeletonHeader.segment;
    uintptr_t skeletonHeader = (uintptr_t)skeleton->GetPointer();
    memcpy(&skelAnime->skeletonHeader, &skeletonHeader, sizeof(uintptr_t));
}

} // namespace PuppetModels

using namespace PuppetModels;

extern "C" const char* PuppetModels_Resolve(const char* path) {
    if (sScope == nullptr || path == nullptr) {
        return nullptr;
    }

    std::string modelPath = WithoutAltPrefix(StripSignature(path));
    if (!IsModelPath(modelPath)) {
        return nullptr;
    }

    std::string target;
    if (sScope->model != nullptr) {
        // Tunic specific files first, like ResourceMgr_ResolveLinkTunicDListPath does for our own player
        static const char* tunicSuffixes[] = { "_kokiri", "_goron", "_zora" };
        size_t folderEnd = modelPath.find('/', std::string_view("objects/").size());
        if (sScope->tunic >= PLAYER_TUNIC_KOKIRI && sScope->tunic <= PLAYER_TUNIC_ZORA &&
            folderEnd != std::string::npos) {
            std::string tunicPath =
                modelPath.substr(0, folderEnd) + tunicSuffixes[sScope->tunic] + modelPath.substr(folderEnd);
            if (sScope->model->archive->Has(tunicPath)) {
                target = sScope->model->prefix + tunicPath;
            }
        }
        if (target.empty() && sScope->model->archive->Has(modelPath)) {
            target = sScope->model->prefix + modelPath;
        }
    }
    if (target.empty() && sScope->vanilla->archive->Has(modelPath)) {
        target = sScope->vanilla->prefix + modelPath;
    }
    if (target.empty()) {
        return nullptr;
    }

    return sResolvedPaths.insert(std::string(OTR_SIGNATURE) + target).first->c_str();
}

extern "C" int32_t PuppetModels_BeginOwnDraw(SkelAnime* skelAnime, int32_t linkAge, int32_t tunic) {
    // Until our model is ready, our Link stays as our mods make him
    if (!sDrawOwnLink || (!sOwnModelId.empty() && !IsReady(sOwnModelId)) || !BeginScope(sOwnModelId, tunic)) {
        return false;
    }
    ApplySkeleton(skelAnime, linkAge, tunic);
    return true;
}

extern "C" void PuppetModels_EndOwnDraw(void) {
    sScope = nullptr;
}

extern "C" int32_t PuppetModels_HasFile(const char* path) {
    if (sScope == nullptr || path == nullptr) {
        return -1;
    }
    return sScope->model != nullptr && sScope->model->archive->Has(WithoutAltPrefix(StripSignature(path)));
}

extern "C" int32_t PuppetModels_IsCustomSkeleton(const char* skeletonPath) {
    if (sScope == nullptr) {
        return -1;
    }
    return sScope->model != nullptr && sScope->model->archive->Has(StripSignature(skeletonPath));
}

extern "C" void PuppetModels_DrawPreview(PlayState* play, Player* player) {
    if (std::chrono::steady_clock::now() > sPreviewUntil || (!sOwnModelId.empty() && !IsReady(sOwnModelId))) {
        return;
    }

    // Our own skeleton isn't touched, the preview draws with a copy
    SkelAnime skelAnime = player->skelAnime;
    s32 tunic = player->currentTunic;
    if (!BeginScope(sOwnModelId, tunic)) {
        return;
    }
    ApplySkeleton(&skelAnime, gSaveContext.linkAge, tunic);

    // To the right of our Link as the camera sees him, facing it
    s16 cameraYaw = Math_Vec3f_Yaw(&play->view.eye, &play->view.lookAt);
    Vec3f pos = player->actor.world.pos;
    pos.x += Math_SinS(cameraYaw - 0x4000) * 60.0f;
    pos.z += Math_CosS(cameraYaw - 0x4000) * 60.0f;
    sPreviewTurn += 0x100;
    Vec3s rot = { 0, (s16)(cameraYaw + 0x8000 + sPreviewTurn), 0 };

    u8 swordAndShield[2] = { (u8)SWORD_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD)),
                             (u8)SHIELD_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD)) };
    Matrix_SetTranslateRotateYXZ(pos.x, pos.y, pos.z, &rot);
    Matrix_Scale(player->actor.scale.x, player->actor.scale.y, player->actor.scale.z, MTXMODE_APPLY);
    func_80093C80(play);
    Player_DrawImpl(play, skelAnime.skeleton, skelAnime.jointTable, skelAnime.dListCount, 0, tunic,
                    player->currentBoots, 0, Player_OverrideLimbDrawPause, NULL, swordAndShield);
    sScope = nullptr;
}

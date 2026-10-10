#include "PuppetModels.h"

#include <list>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include <spdlog/spdlog.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/resource/archive/O2rArchive.h>
#include <ship/utils/StrHash64.h>
#include <fast/resource/type/DisplayList.h>

#include "soh/resource/type/Skeleton.h"
#include "soh/resource/type/SkeletonLimb.h"

extern "C" {
#include <soh_assets.h>
#include <objects/object_link_boy/object_link_boy.h>
#include <objects/object_link_child/object_link_child.h>
#include "z64.h"
#include "z64player.h"
}

namespace PuppetModels {

namespace {

constexpr std::string_view OTR_SIGNATURE = "__OTR__";
constexpr std::string_view ALT_PREFIX = "alt/";

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

/**
 * Presents another archive's files under a path prefix, so they can be mounted next to everything else without
 * replacing anything. Only files the filter accepts are presented, alternate assets in place of the regular ones.
 */
class PrefixedArchive final : public Ship::Archive {
  public:
    PrefixedArchive(std::shared_ptr<Ship::Archive> inner, std::string prefix,
                    std::function<bool(const std::string&)> filter)
        : Archive("anchor-model:" + prefix), mInner(std::move(inner)), mPrefix(std::move(prefix)),
          mFilter(std::move(filter)) {
    }

    bool Open() override {
        for (auto& [hash, path] : *mInner->ListFiles()) {
            std::string modelPath = WithoutAltPrefix(path);
            if (path.ends_with(".meta") || !mFilter(modelPath)) {
                continue;
            }
            if (!mFiles.contains(modelPath) || path.starts_with(ALT_PREFIX)) {
                mFiles[modelPath] = path;
            }
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
        auto it = mFiles.find(filePath.substr(mPrefix.size()));
        return it != mFiles.end() ? mInner->LoadFile(it->second) : nullptr;
    }

    std::shared_ptr<Ship::File> LoadFile(uint64_t hash) override {
        auto files = ListFiles();
        auto it = files->find(hash);
        return it != files->end() ? LoadFile(it->second) : nullptr;
    }

    // Whether this model has a file, by its regular path
    bool Has(const std::string& modelPath) const {
        return mFiles.contains(modelPath);
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
    std::function<bool(const std::string&)> mFilter;
    std::map<std::string, std::string> mFiles; // Regular path -> path in the inner archive
};

struct Model {
    std::string prefix;
    std::shared_ptr<PrefixedArchive> archive;
    // The regular path of each of the model's files by the hashes they may be referred to with
    std::unordered_map<uint64_t, std::string> pathsByHash;
    // Paths written into the model's display lists and limbs, kept alive as long as the model
    std::list<std::string> strings;
};

std::map<std::string, Model> sModels;

struct Scope {
    Model* model;   // The dummy player's model, nullptr for the vanilla one
    Model* vanilla; // Always there while drawing
    int32_t tunic;
};
Scope* sScope = nullptr;

// Interned results of PuppetModels_Resolve, so the returned pointers stay valid
std::unordered_set<std::string> sResolvedPaths;

std::shared_ptr<Ship::ResourceManager> GetResourceManager() {
    return Ship::Context::GetRawInstance()->GetResourceManager();
}

// The base game archive the vanilla model comes from
std::shared_ptr<Ship::Archive> FindBaseArchive() {
    for (auto& archive : *GetResourceManager()->GetArchiveManager()->GetArchives()) {
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

// Loads all of the model's files and points the references between them at its own copies
void Prepare(Model& model) {
    auto resourceManager = GetResourceManager();
    std::vector<std::shared_ptr<Ship::IResource>> resources;
    for (auto& [modelPath, innerPath] : model.archive->GetFiles()) {
        model.pathsByHash[CRC64(modelPath.c_str())] = modelPath;
        model.pathsByHash[CRC64(innerPath.c_str())] = modelPath;
        model.pathsByHash[CRC64((std::string(ALT_PREFIX) + modelPath).c_str())] = modelPath;

        auto resource = resourceManager->LoadResource(model.prefix + modelPath, true);
        if (resource != nullptr) {
            resources.push_back(resource);
        }
    }

    // Skeletons load their limbs, so the limbs are fixed first
    for (auto& resource : resources) {
        if (auto displayList = std::dynamic_pointer_cast<Fast::DisplayList>(resource)) {
            FixDisplayList(model, *displayList);
        } else if (auto limb = std::dynamic_pointer_cast<SOH::SkeletonLimb>(resource)) {
            FixLimb(model, *limb);
        }
    }
    for (auto& resource : resources) {
        if (auto skeleton = std::dynamic_pointer_cast<SOH::Skeleton>(resource)) {
            FixSkeleton(model, *skeleton);
        }
    }
}

bool Mount(const std::string& modelId, std::shared_ptr<Ship::Archive> inner) {
    Model model;
    model.prefix = "anchor/" + modelId + "/";
    model.archive = std::make_shared<PrefixedArchive>(std::move(inner), model.prefix, IsModelPath);
    model.archive->Load();
    if (!model.archive->IsLoaded() || GetResourceManager()->GetArchiveManager()->AddArchive(model.archive) == nullptr) {
        SPDLOG_ERROR("[PuppetModels] Couldn't mount model {}", modelId);
        return false;
    }

    Model& mounted = sModels[modelId] = std::move(model);
    Prepare(mounted);
    SPDLOG_INFO("[PuppetModels] Loaded model {} ({} files)", modelId, mounted.archive->GetFiles().size());
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
    return path.starts_with("objects/object_link_boy") || path.starts_with("objects/object_link_child");
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
    GetResourceManager()->GetArchiveManager()->RemoveArchive(it->second.archive);
    sModels.erase(it);
}

bool IsLoaded(const std::string& modelId) {
    return sModels.contains(modelId);
}

DrawScope::DrawScope(const std::string& modelId, int32_t tunic) {
    static Scope scope;
    if (!EnsureVanillaModel()) {
        return;
    }
    auto model = sModels.find(modelId);
    scope.model = model != sModels.end() && modelId != VANILLA_MODEL_ID ? &model->second : nullptr;
    scope.vanilla = &sModels[VANILLA_MODEL_ID];
    scope.tunic = tunic;
    sScope = &scope;
}

DrawScope::~DrawScope() {
    sScope = nullptr;
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

extern "C" int32_t PuppetModels_IsCustomSkeleton(const char* skeletonPath) {
    if (sScope == nullptr) {
        return -1;
    }
    return sScope->model != nullptr && sScope->model->archive->Has(StripSignature(skeletonPath));
}

#pragma once

#include <stdint.h>

#ifdef __cplusplus
#include <string>

struct SkelAnime;

/**
 * Per player models: teammates' dummy players are drawn with their own Link model (and equipment), while our own Link
 * keeps ours.
 *
 * Resources are looked up by path, the same for everyone, so a teammate's model archive can't just be mounted. Instead
 * it's mounted with all its paths moved under "anchor/<model id>/", and the references inside it (display lists,
 * limbs, skeletons) are pointed at the moved paths. While a dummy player draws, the lookups of Link's files go to its
 * model, then to the vanilla model (the base game's Link files, moved the same way), never to our own mods.
 *
 * A model is everything under objects/object_link_boy*, objects/object_link_child* and objects/object_custom_equip
 * (SoH's custom equipment, see customequipment.cpp).
 */
namespace PuppetModels {

// The model drawn for teammates without one of their own
inline const std::string VANILLA_MODEL_ID = "vanilla";

// Whether a path belongs to Link's model (and so to a model mod)
bool IsModelPath(const std::string& path);

// Mounts and prepares a model archive under the given id. Returns false if it couldn't be read.
bool Load(const std::string& modelId, const std::string& archivePath);
void Unload(const std::string& modelId);
bool IsLoaded(const std::string& modelId);

// Draws a dummy player with a model (empty for the vanilla one) until the scope ends
class DrawScope {
  public:
    DrawScope(const std::string& modelId, int32_t tunic);
    ~DrawScope();
};

// Puts the model's skeleton for the given age and tunic on a dummy player's SkelAnime. Call inside its DrawScope.
void ApplySkeleton(SkelAnime* skelAnime, int32_t linkAge, int32_t tunic);

} // namespace PuppetModels

extern "C" {
#endif

// While a dummy player draws: the path of its model's copy of a Link file, else NULL
const char* PuppetModels_Resolve(const char* path);
// While a dummy player draws: whether its model has a file (like a custom equipment part), else -1
int32_t PuppetModels_HasFile(const char* path);
// While a dummy player draws: whether its model replaces Link's skeleton, else -1
int32_t PuppetModels_IsCustomSkeleton(const char* skeletonPath);

#ifdef __cplusplus
}
#endif

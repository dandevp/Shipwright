#pragma once

#include <stdint.h>

#ifdef __cplusplus
#include <functional>
#include <map>
#include <string>
#include <vector>

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
 * (SoH's custom equipment, see customequipment.cpp), plus what `.meta` aliases there point at (see SelectModelFiles).
 */
namespace PuppetModels {

// The model drawn for teammates without one of their own
inline const std::string VANILLA_MODEL_ID = "vanilla";

// Whether a path belongs to Link's model (and so to a model mod)
bool IsModelPath(const std::string& path);

// Which of an archive's files (by their paths in it) make up a model: Link's files, `.meta` aliases in their place,
// and what those point at with everything in the same folder (its vertices, textures). By the path they're used at
// (alternate assets without "alt/", in place of the regular ones unless left out) -> path in the archive.
using FileReader = std::function<std::vector<char>(const std::string& path)>;
std::map<std::string, std::string> SelectModelFiles(const std::vector<std::string>& paths, const FileReader& read,
                                                    bool useAltAssets);

// Prepares the models mounted by Load a little more, call once a frame. Until it's ready, a model isn't drawn with.
void Update();
bool IsReady(const std::string& modelId);

// Mounts a model archive under the given id, to be prepared over the next frames (see Update). Returns false if it
// couldn't be read.
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

// Our own model, picked in the Anchor menu (empty for vanilla). Our own Link is drawn with it when it isn't what our
// mounted mods give anyway, i.e. when some of them were left out of it.
void SetOwnModel(const std::string& modelId, bool drawOwnLink);

// Shows our model next to our Link for a moment, call every frame while it should stay (see PuppetModels_DrawPreview)
void ShowPreview();

} // namespace PuppetModels

struct PlayState;
struct Player;

extern "C" {
#endif

// While a dummy player draws: the path of its model's copy of a Link file, else NULL
const char* PuppetModels_Resolve(const char* path);
// Draws our own Link with our picked model, if it differs from what our mods give. Returns whether it does, then
// PuppetModels_EndOwnDraw has to follow the drawing.
int32_t PuppetModels_BeginOwnDraw(struct SkelAnime* skelAnime, int32_t linkAge, int32_t tunic);
void PuppetModels_EndOwnDraw(void);
// After our Link is drawn: while the model picker shows it, our model beside him, turning around, in his pose
void PuppetModels_DrawPreview(struct PlayState* play, struct Player* player);
// While a dummy player draws: whether its model has a file (like a custom equipment part), else -1
int32_t PuppetModels_HasFile(const char* path);
// While a dummy player draws: whether its model replaces Link's skeleton, else -1
int32_t PuppetModels_IsCustomSkeleton(const char* skeletonPath);

#ifdef __cplusplus
}
#endif

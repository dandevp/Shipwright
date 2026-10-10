#include "SeedContext.h"
#include "static_data.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/item-tables/ItemTableManager.h"
#include "dungeon.h"
#include "logic.h"
#include "entrance.h"
#include "settings.h"
#include "rando_hash.h"
#include "fishsanity.h"
#include "macros.h"
#include "3drando/hints.hpp"
#include "soh/util.h"
#include "../kaleido.h"
#include "soh/Network/Archipelago/Archipelago.h"
#include "soh/Network/Archipelago/ArchipelagoConsoleWindow.h"
#include "soh/Enhancements/randomizer/Traps.h"
#include "soh/Enhancements/randomizer/rng.h"
#include "soh/Enhancements/randomizer/randomizer_check_tracker.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/Enhancements/randomizer/trial.h"

#include <algorithm>
#include <vector>

#include <fstream>
#include <spdlog/spdlog.h>
extern "C" {
#include <functions.h>
#include <variables.h>
}

namespace Rando {
std::weak_ptr<Context> Context::mContext;

Context::Context() {

    for (int i = 0; i < RC_MAX; i++) {
        itemLocationTable[i] = ItemLocation(static_cast<RandomizerCheck>(i));
    }
    mEntranceShuffler = std::make_shared<EntranceShuffler>();
    mDungeons = std::make_shared<Dungeons>();
    mLogic = std::make_shared<Logic>();
    mTrials = std::make_shared<Trials>();
}

RandomizerArea Context::GetAreaFromString(std::string str) {
    return (RandomizerArea)StaticData::areaNameToEnum[str];
}

int Context::CountEmptyLocations(const bool countShops) {
    auto ctx = Rando::Context::GetInstance();
    return static_cast<int>(count_if(allLocations.begin(), allLocations.end(), [ctx, countShops](const auto loc) {
        return ctx->GetItemLocation(loc)->GetPlacedRandomizerGet() == RG_NONE &&
               (countShops || Rando::StaticData::GetLocation(loc)->GetRCType() != RCTYPE_SHOP);
    }));
}

void Context::InitStaticData() {
    StaticData::HintTable_Init();
    StaticData::trialNameToEnum = StaticData::PopulateTranslationMap(StaticData::trialData);
    StaticData::hintNameToEnum = StaticData::PopulateTranslationMap(StaticData::hintNames);
    StaticData::hintTypeNameToEnum = StaticData::PopulateTranslationMap(StaticData::hintTypeNames);
    StaticData::areaNameToEnum = StaticData::PopulateTranslationMap(StaticData::areaNames);
    StaticData::InitLocationTable();
}

std::shared_ptr<Context> Context::CreateInstance() {
    if (mContext.expired()) {
        auto instance = std::make_shared<Context>();
        mContext = instance;
        GetInstance()->GetLogic()->SetContext(GetInstance());
        return instance;
    }
    return GetInstance();
}

std::shared_ptr<Context> Context::GetInstance() {
    return mContext.lock();
}

Hint* Context::GetHint(const RandomizerHint hintKey) {
    return &hintTable[hintKey];
}

void Context::AddHint(const RandomizerHint hintId, const Hint hint) {
    hintTable[hintId] = hint; // RANDOTODO this should probably be an rvalue
}

ItemLocation* Context::GetItemLocation(const RandomizerCheck locKey) {
    return &itemLocationTable[locKey];
}

ItemLocation* Context::GetItemLocation(size_t locKey) {
    return &itemLocationTable[static_cast<RandomizerCheck>(locKey)];
}

bool Context::IsLocationShuffled(const RandomizerCheck locKey) {
    return itemLocationTable[locKey].GetPlacedRandomizerGet() != RG_NONE;
}

ItemOverride& Context::GetItemOverride(RandomizerCheck locKey) {
    if (!overrides.contains(locKey)) {
        overrides.emplace(locKey, ItemOverride());
    }
    return overrides.at(locKey);
}

ItemOverride& Context::GetItemOverride(size_t locKey) {
    if (!overrides.contains(static_cast<RandomizerCheck>(locKey))) {
        overrides.emplace(static_cast<RandomizerCheck>(locKey), ItemOverride());
    }
    return overrides.at(static_cast<RandomizerCheck>(locKey));
}

void Context::PlaceItemInLocation(const RandomizerCheck locKey, const RandomizerGet item,
                                  const bool applyEffectImmediately, const bool setHidden) {
    const auto loc = GetItemLocation(locKey);
    SPDLOG_DEBUG("{} placed at {}", StaticData::RetrieveItem(item).GetName().GetEnglish(),
                 StaticData::GetLocation(locKey)->GetName());

    if (applyEffectImmediately || mOptions[RSK_NO_LOGIC].Is(RO_GENERIC_OFF)) {
        StaticData::RetrieveItem(item).ApplyEffect();
    }

    // TODO? Show Progress

    loc->SetPlacedItem(item);
    if (setHidden) {
        loc->SetHidden(true);
    }
}

void Context::AddLocation(const RandomizerCheck loc, std::vector<RandomizerCheck>* destination) {
    if (destination == nullptr) {
        destination = &allLocations;
    }
    destination->push_back(loc);
}

template <typename Container>
void Context::AddLocations(const Container& locations, std::vector<RandomizerCheck>* destination) {
    if (destination == nullptr) {
        destination = &allLocations;
    }
    destination->insert(destination->end(), std::cbegin(locations), std::cend(locations));
}

bool Context::IsQuestOfLocationActive(RandomizerCheck rc) {
    const auto loc = Rando::StaticData::GetLocation(rc);
    return loc->GetQuest() == RCQUEST_BOTH ||
           loc->GetQuest() == RCQUEST_MQ && mDungeons->GetDungeonFromScene(loc->GetScene())->IsMQ() ||
           loc->GetQuest() == RCQUEST_VANILLA && mDungeons->GetDungeonFromScene(loc->GetScene())->IsVanilla();
}

void Context::GenerateLocationPool() {
    allLocations.clear();
    overworldLocations.clear();
    // add wincon here so it is properly logged
    if (ctx->GetOption(RSK_WINCON).Get() > RO_WINCON_ANYWHERE) {
        AddLocation(RC_WINCON);
    }
    for (auto dungeon : ctx->GetDungeons()->GetDungeonList()) {
        dungeon->locations.clear();
    }
    for (Location& location : StaticData::GetLocationTable()) {
        // skip RCs that shouldn't be in the pool for any reason (i.e. settings, unsupported check type, etc.)
        // TODO: Exclude checks for some of the older shuffles from the pool too i.e. Frog Songs, Scrubs, etc.)
        if (location.GetRandomizerCheck() == RC_UNKNOWN_CHECK ||
            location.GetRandomizerCheck() == RC_WINCON || // already in pool
            (location.GetRandomizerCheck() == RC_GANONS_BOSS_KEY && mGBKCondition == RO_CHECK_TRIGGER_NONE) ||
            (location.GetRandomizerCheck() == RC_GANON_SOUL && mGanonsSoulCondition == RO_CHECK_TRIGGER_NONE) ||
            (location.GetRandomizerCheck() == RC_TOT_MASTER_SWORD &&
             mOptions[RSK_SHUFFLE_MASTER_SWORD].Is(RO_GENERIC_OFF)) ||
            (location.GetRandomizerCheck() == RC_KAK_100_GOLD_SKULLTULA_REWARD &&
             mOptions[RSK_SHUFFLE_100_GS_REWARD].Is(RO_GENERIC_OFF)) ||
            (location.GetRandomizerCheck() == RC_LH_SCARECROWS_SONG &&
             mOptions[RSK_SHUFFLE_SCARECROWS_SONG].Is(RO_GENERIC_OFF)) ||
            (location.GetRCType() == RCTYPE_CHEST_GAME && mOptions[RSK_SHUFFLE_CHEST_MINIGAME].Is(RO_GENERIC_OFF)) ||
            location.GetRCType() == RCTYPE_STATIC_HINT ||  // can't have items
            location.GetRCType() == RCTYPE_GOSSIP_STONE || // can't have items
            (location.GetRCType() == RCTYPE_FROG_SONG && mOptions[RSK_SHUFFLE_FROG_SONG_RUPEES].Is(RO_GENERIC_OFF)) ||
            (location.GetRCType() == RCTYPE_SCRUB && mOptions[RSK_SHUFFLE_SCRUBS].Is(RO_SCRUBS_OFF)) ||
            (location.GetRCType() == RCTYPE_SCRUB && mOptions[RSK_SHUFFLE_SCRUBS].Is(RO_SCRUBS_ONE_TIME_ONLY) &&
             !(location.GetRandomizerCheck() == RC_LW_DEKU_SCRUB_GROTTO_FRONT ||
               location.GetRandomizerCheck() == RC_LW_DEKU_SCRUB_NEAR_BRIDGE ||
               location.GetRandomizerCheck() == RC_HF_DEKU_SCRUB_GROTTO)) ||
            (location.GetRCType() == RCTYPE_BEGGAR && mOptions[RSK_SHUFFLE_BEGGAR].Is(RO_GENERIC_OFF)) ||
            (location.GetRCType() == RCTYPE_ADULT_TRADE && mOptions[RSK_SHUFFLE_ADULT_TRADE].Is(RO_GENERIC_OFF)) ||
            (location.GetRCType() == RCTYPE_COW && mOptions[RSK_SHUFFLE_COWS].Is(RO_GENERIC_OFF)) ||
            (location.GetRandomizerCheck() == RC_LH_HYRULE_LOACH &&
             mOptions[RSK_FISHSANITY].IsNot(RO_FISHSANITY_HYRULE_LOACH)) ||
            (location.GetRCType() == RCTYPE_FISH && !Fishsanity::GetFishLocationIncluded(&location)) ||
            (location.GetRCType() == RCTYPE_POT && mOptions[RSK_SHUFFLE_POTS].Is(RO_SHUFFLE_POTS_OFF)) ||
            (location.GetRCType() == RCTYPE_GRASS && mOptions[RSK_SHUFFLE_GRASS].Is(RO_SHUFFLE_GRASS_OFF)) ||
            (location.GetRCType() == RCTYPE_CRATE && mOptions[RSK_SHUFFLE_CRATES].Is(RO_SHUFFLE_CRATES_OFF)) ||
            (location.GetRCType() == RCTYPE_NLCRATE &&
             (mOptions[RSK_SHUFFLE_CRATES].Is(RO_SHUFFLE_CRATES_OFF) || mOptions[RSK_NO_LOGIC].Is(RO_GENERIC_OFF))) ||
            (location.GetRCType() == RCTYPE_SMALL_CRATE && mOptions[RSK_SHUFFLE_CRATES].Is(RO_SHUFFLE_CRATES_OFF)) ||
            (location.GetRCType() == RCTYPE_ROCK && !mOptions[RSK_SHUFFLE_ROCKS]) ||
            (location.GetRCType() == RCTYPE_BOULDER && mOptions[RSK_SHUFFLE_BOULDERS].Is(RO_SHUFFLE_BOULDERS_OFF)) ||
            (location.GetRCType() == RCTYPE_FOUNTAIN_FAIRY && !mOptions[RSK_SHUFFLE_FOUNTAIN_FAIRIES]) ||
            (location.GetRCType() == RCTYPE_STONE_FAIRY && !mOptions[RSK_SHUFFLE_STONE_FAIRIES]) ||
            (location.GetRCType() == RCTYPE_BEAN_FAIRY && !mOptions[RSK_SHUFFLE_BEAN_FAIRIES]) ||
            (location.GetRCType() == RCTYPE_SONG_FAIRY && !mOptions[RSK_SHUFFLE_SONG_FAIRIES]) ||
            (location.GetRCType() == RCTYPE_BUTTERFLY_FAIRY && !mOptions[RSK_SHUFFLE_BUTTERFLY_FAIRIES]) ||
            (location.GetRCType() == RCTYPE_TREE && !mOptions[RSK_SHUFFLE_TREES]) ||
            (location.GetRCType() == RCTYPE_NLTREE &&
             (!mOptions[RSK_SHUFFLE_TREES] || mOptions[RSK_NO_LOGIC].Is(RO_GENERIC_OFF))) ||
            (location.GetRCType() == RCTYPE_BUSH && !mOptions[RSK_SHUFFLE_BUSHES]) ||
            (location.GetRCType() == RCTYPE_ICICLE && !mOptions[RSK_SHUFFLE_ICICLES]) ||
            (location.GetRCType() == RCTYPE_RED_ICE && !mOptions[RSK_SHUFFLE_RED_ICE]) ||
            (location.GetRCType() == RCTYPE_SIGN && mOptions[RSK_SHUFFLE_SIGNS].Is(RO_SHUFFLE_SIGNS_OFF)) ||
            (location.GetRCType() == RCTYPE_WONDER_ITEM &&
             mOptions[RSK_SHUFFLE_WONDER_ITEMS].Is(RO_SHUFFLE_WONDER_ITEMS_OFF)) ||
            (location.GetRCType() == RCTYPE_FREESTANDING &&
             mOptions[RSK_SHUFFLE_FREESTANDING].Is(RO_SHUFFLE_FREESTANDING_OFF)) ||
            (location.GetRCType() == RCTYPE_SILVER && !mOptions[RSK_SHUFFLE_SILVER]) ||
            (location.GetRCType() == RCTYPE_BEEHIVE && !mOptions[RSK_SHUFFLE_BEEHIVES])) {
            continue;
        }
        if (location.IsOverworld()) {
            // Skip stuff that is shuffled to dungeon only, i.e. tokens, pots, etc., or other checks that
            // should not have a shuffled item.
            if ((location.GetRCType() == RCTYPE_FREESTANDING &&
                 mOptions[RSK_SHUFFLE_FREESTANDING].Is(RO_SHUFFLE_FREESTANDING_DUNGEONS)) ||
                (location.GetRCType() == RCTYPE_POT && mOptions[RSK_SHUFFLE_POTS].Is(RO_SHUFFLE_POTS_DUNGEONS)) ||
                (location.GetRCType() == RCTYPE_GRASS && mOptions[RSK_SHUFFLE_GRASS].Is(RO_SHUFFLE_GRASS_DUNGEONS)) ||
                (location.GetRCType() == RCTYPE_WONDER_ITEM &&
                 mOptions[RSK_SHUFFLE_WONDER_ITEMS].Is(RO_SHUFFLE_WONDER_ITEMS_DUNGEONS)) ||
                (location.GetRCType() == RCTYPE_CRATE && mOptions[RSK_SHUFFLE_CRATES].Is(RO_SHUFFLE_CRATES_DUNGEONS)) ||
                (location.GetRCType() == RCTYPE_NLCRATE &&
                 mOptions[RSK_SHUFFLE_CRATES].Is(RO_SHUFFLE_CRATES_DUNGEONS) &&
                 mOptions[RSK_NO_LOGIC].Is(RO_GENERIC_ON)) ||
                (location.GetRCType() == RCTYPE_SMALL_CRATE &&
                 mOptions[RSK_SHUFFLE_CRATES].Is(RO_SHUFFLE_CRATES_DUNGEONS)) ||
                (location.GetRCType() == RCTYPE_BOULDER &&
                 mOptions[RSK_SHUFFLE_BOULDERS].Is(RO_SHUFFLE_BOULDERS_DUNGEONS)) ||
                (location.GetRCType() == RCTYPE_SIGN && mOptions[RSK_SHUFFLE_SIGNS].Is(RO_SHUFFLE_SIGNS_DUNGEONS))) {
                continue;
            }
            // If we've gotten past all the conditions where an overworld location should not be
            // shuffled, add it to the pool.
            AddLocation(location.GetRandomizerCheck(), &overworldLocations);
            AddLocation(location.GetRandomizerCheck());
        } else { // is a dungeon check
            auto* dungeon = GetDungeon(location.GetArea() - RCAREA_DEKU_TREE);
            if (location.GetQuest() == RCQUEST_BOTH || (location.GetQuest() == RCQUEST_MQ) == dungeon->IsMQ()) {
                if ((location.GetRCType() == RCTYPE_FREESTANDING &&
                     mOptions[RSK_SHUFFLE_FREESTANDING].Is(RO_SHUFFLE_FREESTANDING_OVERWORLD)) ||
                    (location.GetRCType() == RCTYPE_POT && mOptions[RSK_SHUFFLE_POTS].Is(RO_SHUFFLE_POTS_OVERWORLD)) ||
                    (location.GetRCType() == RCTYPE_GRASS &&
                     mOptions[RSK_SHUFFLE_GRASS].Is(RO_SHUFFLE_GRASS_OVERWORLD)) ||
                    (location.GetRCType() == RCTYPE_WONDER_ITEM &&
                     mOptions[RSK_SHUFFLE_WONDER_ITEMS].Is(RO_SHUFFLE_WONDER_ITEMS_OVERWORLD)) ||
                    (location.GetRCType() == RCTYPE_CRATE &&
                     mOptions[RSK_SHUFFLE_CRATES].Is(RO_SHUFFLE_CRATES_OVERWORLD)) ||
                    (location.GetRCType() == RCTYPE_NLCRATE &&
                     mOptions[RSK_SHUFFLE_CRATES].Is(RO_SHUFFLE_CRATES_OVERWORLD) &&
                     mOptions[RSK_NO_LOGIC].Is(RO_GENERIC_ON)) ||
                    (location.GetRCType() == RCTYPE_SMALL_CRATE &&
                     mOptions[RSK_SHUFFLE_CRATES].Is(RO_SHUFFLE_CRATES_OVERWORLD)) ||
                    (location.GetRCType() == RCTYPE_BOULDER &&
                     mOptions[RSK_SHUFFLE_BOULDERS].Is(RO_SHUFFLE_BOULDERS_OVERWORLD)) ||
                    (location.GetRCType() == RCTYPE_SIGN &&
                     mOptions[RSK_SHUFFLE_SIGNS].Is(RO_SHUFFLE_SIGNS_OVERWORLD))) {
                    continue;
                }
                // also add to that dungeon's location list.
                AddLocation(location.GetRandomizerCheck(), &dungeon->locations);
                AddLocation(location.GetRandomizerCheck());
            }
        }
    }
}

void Context::AddExcludedOptions() {
    for (auto& loc : StaticData::GetLocationTable()) {
        // Checks of these types don't have items, skip them.
        if (loc.GetRandomizerCheck() == RC_UNKNOWN_CHECK || loc.GetRandomizerCheck() == RC_WINCON ||
            loc.GetRCType() == RCTYPE_STATIC_HINT || loc.GetRCType() == RCTYPE_GOSSIP_STONE) {
            continue;
        }
        AddLocation(loc.GetRandomizerCheck(), &everyPossibleLocation);
        Option* excludedOption = loc.GetExcludedOption();
        auto& areaOptions = Settings::GetInstance()->GetExcludeOptionsForArea(loc.GetArea());
        const bool alreadyAdded = std::any_of(areaOptions.begin(), areaOptions.end(), [&](const Option* option) {
            return option->GetName() == excludedOption->GetName();
        });
        if (!alreadyAdded) {
            areaOptions.push_back(excludedOption);
        }
    }
}

std::vector<RandomizerCheck> Context::GetLocations(const std::vector<RandomizerCheck>& locationPool,
                                                   const RandomizerCheckType checkType) {
    std::vector<RandomizerCheck> locationsOfType;
    for (RandomizerCheck locKey : locationPool) {
        if (StaticData::GetLocation(locKey)->GetRCType() == checkType) {
            locationsOfType.push_back(locKey);
        }
    }
    return locationsOfType;
}

void Context::ClearItemLocations() {
    for (size_t i = 0; i < itemLocationTable.size(); i++) {
        GetItemLocation(static_cast<RandomizerCheck>(i))->ResetVariables();
    }
}

void Context::ItemReset() {
    for (const RandomizerCheck il : allLocations) {
        GetItemLocation(il)->ResetVariables();
    }

    for (const RandomizerCheck il : StaticData::dungeonRewardLocations) {
        GetItemLocation(il)->ResetVariables();
    }

    GetItemLocation(RC_GIFT_FROM_RAURU)->ResetVariables();
    GetItemLocation(RC_LINKS_POCKET)->ResetVariables();
}

void Context::LocationReset() {
    for (auto& il : itemLocationTable) {
        il.RemoveFromPool();
    }
}

void Context::HintReset() {
    for (const RandomizerCheck il : StaticData::GetGossipStoneLocations()) {
        GetItemLocation(il)->ResetVariables();
    }
    for (Hint& hint : hintTable) {
        hint.ResetVariables();
    }
}

void Context::CreateItemOverrides() {
    SPDLOG_DEBUG("NOW CREATING OVERRIDES");
    for (RandomizerCheck locKey : allLocations) {
        const auto loc = StaticData::GetLocation(locKey);
        // If this is an ice trap, store the disguise model in iceTrapModels
        const auto itemLoc = GetItemLocation(locKey);
        if (itemLoc->GetPlacedRandomizerGet() == RG_ICE_TRAP) {
            ItemOverride val(locKey, Traps::GetTrapTrickModel(&rando_state));
            iceTrapModels[locKey] = val.LooksLike();
            Traps::TrickName trickName = Traps::GetTrapName(
                val.LooksLike(), static_cast<RandoIceTrapNames>(mOptions[RSK_ICE_TRAP_NAMES].Get()), &rando_state);
            val.SetTrickName(trickName.name);
            val.SetTrickArticle(trickName.article);
            // If this is ice trap is in a shop, change the name based on what the model will look like
            overrides[locKey] = val;
        }
        SPDLOG_DEBUG("{}: {}", loc->GetName(), itemLoc->GetPlacedItemName().GetEnglish());
    }
    SPDLOG_DEBUG("Overrides Created: {}", std::to_string(overrides.size()));
}

bool Context::IsSeedGenerated() const {
    return mSeedGenerated;
}

void Context::SetSeedGenerated(const bool seedGenerated) {
    mSeedGenerated = seedGenerated;
}

bool Context::IsSpoilerLoaded() const {
    return mSpoilerLoaded;
}

void Context::SetSpoilerLoaded(const bool spoilerLoaded) {
    mSpoilerLoaded = spoilerLoaded;
}

void Context::AddReceivedArchipelagoItem(const RandomizerGet item) {
    mAPreceiveQueue.emplace(item);
}

GetItemEntry Context::GetArchipelagoGIEntry() {
    if (mAPreceiveQueue.empty()) {
        // Something must have gone wrong here, just give a rupee
        return ItemTableManager::Instance->RetrieveItemEntry(MOD_NONE, GI_HEART);
    }

    // Get the first item from the archipelago queue
    RandomizerGet itemId = mAPreceiveQueue.front();
    assert(itemId != RG_NONE);

    Item& item = StaticData::RetrieveItem(itemId);
    GetItemEntry itemEntry = item.GetGIEntry_Copy();
    mAPreceiveQueue.pop();
    return itemEntry;
}

GetItemEntry Context::GetFinalGIEntry(const RandomizerCheck rc, const bool checkObtainability, const GetItemID ogItemId,
                                      bool spoilAreas) {
    const auto itemLoc = GetItemLocation(rc);
    if (itemLoc->GetPlacedRandomizerGet() == RG_NONE) {
        if (ogItemId != GI_NONE) {
            return ItemTableManager::Instance->RetrieveItemEntry(MOD_NONE, ogItemId);
        }
        return ItemTableManager::Instance->RetrieveItemEntry(
            MOD_NONE, StaticData::RetrieveItem(StaticData::GetLocation(rc)->GetVanillaItem()).GetItemID());
    }
    if (checkObtainability && OTRGlobals::Instance->gRandomizer->GetItemObtainabilityFromRandomizerGet(
                                  itemLoc->GetPlacedRandomizerGet()) != CAN_OBTAIN) {
        if (spoilAreas) {
            CheckTracker::SpoilAreaFromCantObtain(itemLoc->GetPlacedRandomizerGet());
        }
        return ItemTableManager::Instance->RetrieveItemEntry(MOD_NONE, GI_RUPEE_BLUE);
    }
    GetItemEntry giEntry = itemLoc->GetPlacedItem().GetGIEntry_Copy();
    if (overrides.contains(rc)) {
        const auto fakeGiEntry = StaticData::RetrieveItem(overrides[rc].LooksLike()).GetGIEntry();
        giEntry.gid = fakeGiEntry->gid;
        giEntry.gi = fakeGiEntry->gi;
        giEntry.drawItemId = fakeGiEntry->drawItemId;
        giEntry.drawModIndex = fakeGiEntry->drawModIndex;
        giEntry.drawFunc = fakeGiEntry->drawFunc;
    }
    return giEntry;
}

void Context::ParseSpoiler(const char* spoilerFileName) {
    std::ifstream spoilerFileStream(SohUtils::Sanitize(spoilerFileName));
    if (!spoilerFileStream) {
        return;
    }
    mSeedGenerated = false;
    mSpoilerLoaded = false;
    try {
        nlohmann::json spoilerFileJson;
        spoilerFileStream >> spoilerFileJson;
        spoilerFileStream.close();
        ParseHashIconIndexesJson(spoilerFileJson);
        Rando::Settings::GetInstance()->ParseJson(spoilerFileJson);
        ParseItemLocationsJson(spoilerFileJson);
        ParseTricksJson(spoilerFileJson);
        mEntranceShuffler->ParseJson(spoilerFileJson);
        ParseHintJson(spoilerFileJson);
        mDungeons->ParseJson(spoilerFileJson);
        mTrials->ParseJson(spoilerFileJson);
        mSpoilerLoaded = true;
        mSeedGenerated = false;
    } catch (...) { SPDLOG_ERROR("Failed to load Spoiler File: {}", spoilerFileName); }
}

void Context::ParseArchipelago() {
    mSeedGenerated = false;
    mSpoilerLoaded = false;
    mEntranceShuffler->UnshuffleAllEntrances();
    mDungeons->ResetAllDungeons();
    mTrials->RemoveAllTrials();

    Rando::Settings::GetInstance()->ResetExcludedLocations();
    ArchipelagoClient& apClient = ArchipelagoClient::GetInstance();
    ParseArchipelagoItemsLocations(apClient.GetScoutedItems());
    ParseArchipelagoOptions();
    ParseArchipelagoTricks();
    ParseArchipelagoExcludedLocations();
    ParseArchipelagoHints();
}

void Context::ParseHashIconIndexesJson(const nlohmann::json& spoilerFileJson) {
    nlohmann::json hashJson = spoilerFileJson.value("file_hash", nlohmann::json());
    int index = 0;
    for (auto it = hashJson.begin(); it != hashJson.end(); ++it) {
        hashIconIndexes[index] = gSeedTextures[it.value()].id;
        index++;
    }
}

void Context::ParseItemLocationsJson(const nlohmann::json& spoilerFileJson) {
    nlohmann::json locationsJson = spoilerFileJson.value("locations", nlohmann::json());
    for (auto it = locationsJson.begin(); it != locationsJson.end(); ++it) {
        RandomizerCheck rc = StaticData::locationNameToEnum[it.key()];
        if (it->is_structured()) {
            nlohmann::json itemJson = *it;
            for (auto itemit = itemJson.begin(); itemit != itemJson.end(); ++itemit) {
                if (itemit.key() == "item") {
                    itemLocationTable[rc].SetPlacedItem(StaticData::itemNameToEnum[itemit.value().get<std::string>()]);
                } else if (itemit.key() == "price") {
                    itemLocationTable[rc].SetCustomPrice(itemit.value().get<uint16_t>());
                } else if (itemit.key() == "model") {
                    overrides[rc] = ItemOverride(rc, StaticData::itemNameToEnum[itemit.value().get<std::string>()]);
                } else if (itemit.key() == "trickName") {
                    overrides[rc].SetTrickName(Text(itemit.value().get<std::string>()));
                } else if (itemit.key() == "trickArticle") {
                    overrides[rc].SetTrickArticle(Text(itemit.value().get<std::string>()));
                }
            }
        } else {
            itemLocationTable[rc].SetPlacedItem(StaticData::itemNameToEnum[it.value().get<std::string>()]);
        }
    }
}

void Context::ParseArchipelagoOptions() {
    // Set options to what Archipelago expects. Need to slowly convert these to options in apworld and
    // load those in instead.

    nlohmann::json slotData = ArchipelagoClient::GetInstance().GetSlotData();
    mOptions[RSK_NO_LOGIC].Set(RO_GENERIC_NO);
    mOptions[RSK_FOREST].Set(slotData["closed_forest"]);
    mOptions[RSK_DOOR_OF_TIME].Set(slotData["door_of_time"]);
    mOptions[RSK_ZORAS_FOUNTAIN].Set(slotData["zoras_fountain"]);
    mOptions[RSK_SLEEPING_WATERFALL].Set(slotData["sleeping_waterfall"]);
    mOptions[RSK_JABU_OPEN].Set(slotData["jabu_jabu"]);
    mOptions[RSK_STARTING_AGE].Set(slotData["starting_age"]);
    mOptions[RSK_SELECTED_STARTING_AGE].Set(slotData["starting_age"]);
    mOptions[RSK_GERUDO_FORTRESS].Set(slotData["fortress_carpenters"]);
    // The apworld still numbers the bridge options from before Triforce Pieces was inserted ahead of Greg
    uint8_t rainbowBridge = slotData["rainbow_bridge"];
    mOptions[RSK_RAINBOW_BRIDGE].Set(rainbowBridge >= RO_BRIDGE_TRIFORCE_PIECES ? rainbowBridge + 1 : rainbowBridge);
    mOptions[RSK_RAINBOW_BRIDGE_STONE_COUNT].Set(slotData["rainbow_bridge_stones_required"]);
    mOptions[RSK_RAINBOW_BRIDGE_MEDALLION_COUNT].Set(slotData["rainbow_bridge_medallions_required"]);
    mOptions[RSK_RAINBOW_BRIDGE_REWARD_COUNT].Set(slotData["rainbow_bridge_dungeon_rewards_required"]);
    mOptions[RSK_RAINBOW_BRIDGE_DUNGEON_COUNT].Set(slotData["rainbow_bridge_dungeons_required"]);
    mOptions[RSK_RAINBOW_BRIDGE_TOKEN_COUNT].Set(slotData["rainbow_bridge_skull_tokens_required"]);
    mOptions[RSK_BRIDGE_OPTIONS].Set(slotData["rainbow_bridge_greg_modifier"]);
    if (slotData["skip_ganons_trials"] == 0) {
        mOptions[RSK_GANONS_TRIALS].Set(RO_GANONS_TRIALS_SKIP);
        mOptions[RSK_TRIAL_COUNT].Set(0);
        mTrials->RemoveAllTrials();
    } else {
        mOptions[RSK_GANONS_TRIALS].Set(RO_GANONS_TRIALS_SET_NUMBER);
        mOptions[RSK_TRIAL_COUNT].Set(slotData["ganons_trials_count"]);
        std::vector<TrialKey> requiredTrials;
        for (const nlohmann::basic_json<>& trialString : slotData["required_trials"]) {
            if (trialString == "Forest Trial") {
                requiredTrials.emplace_back(TrialKey::TK_FOREST_TRIAL);
            } else if (trialString == "Fire Trial") {
                requiredTrials.emplace_back(TrialKey::TK_FIRE_TRIAL);
            } else if (trialString == "Water Trial") {
                requiredTrials.emplace_back(TrialKey::TK_WATER_TRIAL);
            } else if (trialString == "Shadow Trial") {
                requiredTrials.emplace_back(TrialKey::TK_SHADOW_TRIAL);
            } else if (trialString == "Spirit Trial") {
                requiredTrials.emplace_back(TrialKey::TK_SPIRIT_TRIAL);
            } else if (trialString == "Light Trial") {
                requiredTrials.emplace_back(TrialKey::TK_LIGHT_TRIAL);
            }
        }

        for (auto& trial : mTrials->GetTrialList()) {
            trial->SetAsSkipped();
            if (std::find(requiredTrials.begin(), requiredTrials.end(), trial->GetTrialKey()) != requiredTrials.end()) {
                trial->SetAsRequired();
            }
        }
    }

    mOptions[RSK_MEDALLION_LOCKED_TRIALS].Set(slotData["medallion_locked_trials"]);
    if (slotData["ocarina_of_time"] == 0) {
        mOptions[RSK_STARTING_OCARINA].Set(RO_STARTING_OCARINA_OFF);
    } else if (slotData["ocarina_of_time"] == 1) {
        mOptions[RSK_STARTING_OCARINA].Set(RO_STARTING_OCARINA_FAIRY);
    } else if (slotData["ocarina_of_time"] == 2) {
        mOptions[RSK_STARTING_OCARINA].Set(RO_STARTING_OCARINA_TIME);
    }
    mOptions[RSK_SHUFFLE_OCARINA].Set(slotData["shuffle_ocarinas"]);
    mOptions[RSK_SHUFFLE_OCARINA_BUTTONS].Set(slotData["shuffle_ocarina_buttons"]);
    mOptions[RSK_SHUFFLE_SWIM].Set(slotData["shuffle_swim"]);
    mOptions[RSK_STARTING_DEKU_SHIELD].Set(slotData["start_with_deku_shield"]);
    mOptions[RSK_STARTING_KOKIRI_SWORD].Set(slotData["start_with_kokiri_sword"]);
    mOptions[RSK_STARTING_MASTER_SWORD].Set(slotData["start_with_master_sword"]);
    mOptions[RSK_STARTING_ZELDAS_LULLABY].Set(slotData["start_with_zeldas_lullaby"]);
    mOptions[RSK_STARTING_EPONAS_SONG].Set(slotData["start_with_eponas_song"]);
    mOptions[RSK_STARTING_SARIAS_SONG].Set(slotData["start_with_sarias_song"]);
    mOptions[RSK_STARTING_SUNS_SONG].Set(slotData["start_with_suns_song"]);
    mOptions[RSK_STARTING_SONG_OF_TIME].Set(slotData["start_with_song_of_time"]);
    mOptions[RSK_STARTING_SONG_OF_STORMS].Set(slotData["start_with_song_of_storms"]);
    mOptions[RSK_STARTING_MINUET_OF_FOREST].Set(slotData["start_with_minuet"]);
    mOptions[RSK_STARTING_BOLERO_OF_FIRE].Set(slotData["start_with_bolero"]);
    mOptions[RSK_STARTING_SERENADE_OF_WATER].Set(slotData["start_with_serenade"]);
    mOptions[RSK_STARTING_REQUIEM_OF_SPIRIT].Set(slotData["start_with_requiem"]);
    mOptions[RSK_STARTING_NOCTURNE_OF_SHADOW].Set(slotData["start_with_nocturne"]);
    mOptions[RSK_STARTING_PRELUDE_OF_LIGHT].Set(slotData["start_with_prelude"]);
    mOptions[RSK_SHUFFLE_KOKIRI_SWORD].Set(slotData["shuffle_kokiri_sword"]);
    mOptions[RSK_SHUFFLE_MASTER_SWORD].Set(slotData["shuffle_master_sword"]);
    mOptions[RSK_SHUFFLE_CHILD_WALLET].Set(slotData["shuffle_childs_wallet"]);
    mOptions[RSK_INCLUDE_TYCOON_WALLET].Set(slotData["shuffle_tycoon_wallet"]);
    if (slotData["shuffle_dungeon_rewards"] == 0) {
        mOptions[RSK_SHUFFLE_DUNGEON_REWARDS].Set(RO_DUNGEON_REWARDS_VANILLA);
    } else if (slotData["shuffle_dungeon_rewards"] == 1) {
        mOptions[RSK_SHUFFLE_DUNGEON_REWARDS].Set(RO_DUNGEON_REWARDS_END_OF_DUNGEON);
    } else if (slotData["shuffle_dungeon_rewards"] == 2) {
        mOptions[RSK_SHUFFLE_DUNGEON_REWARDS].Set(RO_DUNGEON_REWARDS_ANY_DUNGEON);
    } else if (slotData["shuffle_dungeon_rewards"] == 3) {
        mOptions[RSK_SHUFFLE_DUNGEON_REWARDS].Set(RO_DUNGEON_REWARDS_OVERWORLD);
    } else if (slotData["shuffle_dungeon_rewards"] == 4) {
        mOptions[RSK_SHUFFLE_DUNGEON_REWARDS].Set(RO_DUNGEON_REWARDS_ANYWHERE);
    }
    if (slotData["shuffle_songs"] == 0) {
        mOptions[RSK_SHUFFLE_SONGS].Set(RO_SONG_SHUFFLE_OFF);
    } else if (slotData["shuffle_songs"] == 1) {
        mOptions[RSK_SHUFFLE_SONGS].Set(RO_SONG_SHUFFLE_SONG_LOCATIONS);
    } else if (slotData["shuffle_songs"] == 2) {
        mOptions[RSK_SHUFFLE_SONGS].Set(RO_SONG_SHUFFLE_DUNGEON_REWARDS);
    } else if (slotData["shuffle_songs"] == 3) {
        mOptions[RSK_SHUFFLE_SONGS].Set(RO_SONG_SHUFFLE_ANYWHERE);
    }
    if (slotData["shuffle_skull_tokens"] == 3) {
        mOptions[RSK_SHUFFLE_TOKENS].Set(RO_TOKENSANITY_ALL);
    } else if (slotData["shuffle_skull_tokens"] == 2) {
        mOptions[RSK_SHUFFLE_TOKENS].Set(RO_TOKENSANITY_OVERWORLD);
    } else if (slotData["shuffle_skull_tokens"] == 1) {
        mOptions[RSK_SHUFFLE_TOKENS].Set(RO_TOKENSANITY_DUNGEONS);
    } else {
        mOptions[RSK_SHUFFLE_TOKENS].Set(RO_TOKENSANITY_OFF);
    }
    mOptions[RSK_SHOPSANITY].Set(slotData["shuffle_shops"]);
    mOptions[RSK_SHOPSANITY_COUNT].Set(slotData["shuffle_shops_item_amount"]);
    mOptions[RSK_SHOPSANITY_PRICES].Set(RO_PRICE_FIXED);
    mOptions[RSK_SHOPSANITY_PRICES_FIXED_PRICE].Set(1);
    mOptions[RSK_SHOPSANITY_PRICES_RANGE_1].Set(0);
    mOptions[RSK_SHOPSANITY_PRICES_RANGE_2].Set(0);
    mOptions[RSK_SHOPSANITY_PRICES_NO_WALLET_WEIGHT].Set(0);
    mOptions[RSK_SHOPSANITY_PRICES_CHILD_WALLET_WEIGHT].Set(0);
    mOptions[RSK_SHOPSANITY_PRICES_ADULT_WALLET_WEIGHT].Set(0);
    mOptions[RSK_SHOPSANITY_PRICES_GIANT_WALLET_WEIGHT].Set(0);
    mOptions[RSK_SHOPSANITY_PRICES_TYCOON_WALLET_WEIGHT].Set(0);
    mOptions[RSK_SHOPSANITY_PRICES_AFFORDABLE].Set(0);
    if (slotData["shuffle_scrubs"] == 2) {
        mOptions[RSK_SHUFFLE_SCRUBS].Set(RO_SCRUBS_ALL);
    } else if (slotData["shuffle_scrubs"] == 1) {
        mOptions[RSK_SHUFFLE_SCRUBS].Set(RO_SCRUBS_ONE_TIME_ONLY);
    } else {
        mOptions[RSK_SHUFFLE_SCRUBS].Set(RO_SCRUBS_OFF);
    }
    mOptions[RSK_SCRUBS_PRICES].Set(RO_PRICE_FIXED);
    mOptions[RSK_SCRUBS_PRICES_FIXED_PRICE].Set(1);
    mOptions[RSK_SCRUBS_PRICES_RANGE_1].Set(0);
    mOptions[RSK_SCRUBS_PRICES_RANGE_2].Set(0);
    mOptions[RSK_SCRUBS_PRICES_NO_WALLET_WEIGHT].Set(0);
    mOptions[RSK_SCRUBS_PRICES_CHILD_WALLET_WEIGHT].Set(0);
    mOptions[RSK_SCRUBS_PRICES_ADULT_WALLET_WEIGHT].Set(0);
    mOptions[RSK_SCRUBS_PRICES_GIANT_WALLET_WEIGHT].Set(0);
    mOptions[RSK_SCRUBS_PRICES_TYCOON_WALLET_WEIGHT].Set(0);
    mOptions[RSK_SCRUBS_PRICES_AFFORDABLE].Set(0);
    mOptions[RSK_SHUFFLE_BEEHIVES].Set(slotData["shuffle_beehives"]);
    mOptions[RSK_SHUFFLE_COWS].Set(slotData["shuffle_cows"]);
    // Ship shows Malon Egg location if weird egg is shuffled even with "Skip Child Zelda" on
    // So manually set this to off.
    if (slotData["skip_child_zelda"] == 0) {
        mOptions[RSK_SHUFFLE_WEIRD_EGG].Set(slotData["shuffle_weird_egg"]);
    } else {
        mOptions[RSK_SHUFFLE_WEIRD_EGG].Set(RO_WEIRD_EGG_SKIP_TALON);
    }
    mOptions[RSK_SHUFFLE_GERUDO_MEMBERSHIP_CARD].Set(slotData["shuffle_gerudo_membership_card"]);
    mOptions[RSK_SHUFFLE_POTS].Set(slotData["shuffle_pots"]);
    mOptions[RSK_SHUFFLE_CRATES].Set(slotData["shuffle_crates"]);
    mOptions[RSK_SHUFFLE_TREES].Set(slotData["shuffle_trees"]);
    mOptions[RSK_SHUFFLE_BUSHES].Set(RO_GENERIC_NO);
    mOptions[RSK_SHUFFLE_FROG_SONG_RUPEES].Set(slotData["shuffle_frog_song_rupees"]);
    mOptions[RSK_ITEM_POOL].Set(0);
    mOptions[RSK_BASE_ICE_TRAPS].Set(0);
    mOptions[RSK_ADDITIONAL_ICE_TRAPS].Set(0);
    mOptions[RSK_ICE_TRAP_PERCENT].Set(0);
    mOptions[RSK_GOSSIP_STONE_HINTS].Set(RO_GOSSIP_STONES_NONE);
    mOptions[RSK_TOT_ALTAR_HINT].Set(slotData["tot_altar_hint"]);
    mOptions[RSK_GANONDORF_HINT].Set(slotData["ganondorf_hint"]);
    mOptions[RSK_SHEIK_LA_HINT].Set(slotData["sheik_la_hint"]);
    mOptions[RSK_BOSS_KEY_HINT].Set(slotData["boss_key_hint"]);
    mOptions[RSK_DAMPES_DIARY_HINT].Set(slotData["dampe_diary_hint"]);
    mOptions[RSK_GREG_HINT].Set(slotData["greg_hint"]);
    // Loach not currently enabled in AP
    mOptions[RSK_LOACH_HINT].Set(RO_GENERIC_OFF); // slotData["hyrule_loach_hint"]);
    mOptions[RSK_SARIA_HINT].Set(slotData["saria_hint"]);
    mOptions[RSK_MIDO_HINT].Set(slotData["mido_hint"]);
    mOptions[RSK_FROGS_HINT].Set(slotData["frog_game_hint"]);
    mOptions[RSK_OOT_HINT].Set(slotData["ocarina_of_time_hint"]);
    mOptions[RSK_KAK_10_SKULLS_HINT].Set(slotData["gs_10_hint"]);
    mOptions[RSK_KAK_20_SKULLS_HINT].Set(slotData["gs_20_hint"]);
    mOptions[RSK_KAK_30_SKULLS_HINT].Set(slotData["gs_30_hint"]);
    mOptions[RSK_KAK_40_SKULLS_HINT].Set(slotData["gs_40_hint"]);
    mOptions[RSK_KAK_50_SKULLS_HINT].Set(slotData["gs_50_hint"]);
    mOptions[RSK_KAK_100_SKULLS_HINT].Set(slotData["gs_100_hint"]);
    mOptions[RSK_MASK_SHOP_HINT].Set(slotData["mask_shop_hint"]);
    mOptions[RSK_BIGGORON_HINT].Set(slotData["big_goron_hint"]);
    mOptions[RSK_BIG_POES_HINT].Set(slotData["big_poe_hint"]);
    mOptions[RSK_CHICKENS_HINT].Set(slotData["chicken_hint"]);
    mOptions[RSK_MALON_HINT].Set(slotData["malon_hint"]);
    mOptions[RSK_HBA_HINT].Set(slotData["horseback_archery_hint"]);
    mOptions[RSK_WARP_SONG_HINTS].Set(slotData["warp_song_hint"]);
    mOptions[RSK_SCRUB_TEXT_HINT].Set(slotData["scrub_hints"]);
    mOptions[RSK_MERCHANT_TEXT_HINT].Set(slotData["merchant_hints"]);
    mOptions[RSK_FISHING_POLE_HINT].Set(slotData["fishing_pole_hint"]);
    if (slotData["hint_clarity"] == 0) {
        mOptions[RSK_HINT_CLARITY].Set(RO_HINT_CLARITY_OBSCURE);
    } else if (slotData["hint_clarity"] == 1) {
        mOptions[RSK_HINT_CLARITY].Set(RO_HINT_CLARITY_AMBIGUOUS);
    } else if (slotData["hint_clarity"] == 2) {
        mOptions[RSK_HINT_CLARITY].Set(RO_HINT_CLARITY_CLEAR);
    }
    mOptions[RSK_HINT_DISTRIBUTION].Set(0);
    if (slotData["maps_and_compasses"] == 0) {
        mOptions[RSK_SHUFFLE_MAPANDCOMPASS].Set(RO_DUNGEON_ITEM_LOC_STARTWITH);
    } else if (slotData["maps_and_compasses"] == 1) {
        mOptions[RSK_SHUFFLE_MAPANDCOMPASS].Set(RO_DUNGEON_ITEM_LOC_VANILLA);
    } else if (slotData["maps_and_compasses"] == 2) {
        mOptions[RSK_SHUFFLE_MAPANDCOMPASS].Set(RO_DUNGEON_ITEM_LOC_OWN_DUNGEON);
    } else if (slotData["maps_and_compasses"] == 3) {
        mOptions[RSK_SHUFFLE_MAPANDCOMPASS].Set(RO_DUNGEON_ITEM_LOC_ANY_DUNGEON);
    } else if (slotData["maps_and_compasses"] == 4) {
        mOptions[RSK_SHUFFLE_MAPANDCOMPASS].Set(RO_DUNGEON_ITEM_LOC_OVERWORLD);
    } else if (slotData["maps_and_compasses"] == 5) {
        mOptions[RSK_SHUFFLE_MAPANDCOMPASS].Set(RO_DUNGEON_ITEM_LOC_ANYWHERE);
    }
    if (slotData["small_key_shuffle"] == 0) {
        mOptions[RSK_KEYSANITY].Set(RO_DUNGEON_ITEM_LOC_STARTWITH);
    } else if (slotData["small_key_shuffle"] == 1) {
        mOptions[RSK_KEYSANITY].Set(RO_DUNGEON_ITEM_LOC_VANILLA);
    } else if (slotData["small_key_shuffle"] == 2) {
        mOptions[RSK_KEYSANITY].Set(RO_DUNGEON_ITEM_LOC_OWN_DUNGEON);
    } else if (slotData["small_key_shuffle"] == 3) {
        mOptions[RSK_KEYSANITY].Set(RO_DUNGEON_ITEM_LOC_ANY_DUNGEON);
    } else if (slotData["small_key_shuffle"] == 4) {
        mOptions[RSK_KEYSANITY].Set(RO_DUNGEON_ITEM_LOC_OVERWORLD);
    } else if (slotData["small_key_shuffle"] == 5) {
        mOptions[RSK_KEYSANITY].Set(RO_DUNGEON_ITEM_LOC_ANYWHERE);
    }
    if (slotData["gerudo_fortress_key_shuffle"] == 0) {
        mOptions[RSK_GERUDO_KEYS].Set(RO_GERUDO_KEYS_VANILLA);
    } else if (slotData["gerudo_fortress_key_shuffle"] == 1) {
        mOptions[RSK_GERUDO_KEYS].Set(RO_GERUDO_KEYS_ANY_DUNGEON);
    } else if (slotData["gerudo_fortress_key_shuffle"] == 2) {
        mOptions[RSK_GERUDO_KEYS].Set(RO_GERUDO_KEYS_OVERWORLD);
    } else if (slotData["gerudo_fortress_key_shuffle"] == 3) {
        mOptions[RSK_GERUDO_KEYS].Set(RO_GERUDO_KEYS_ANYWHERE);
    }
    if (slotData["boss_key_shuffle"] == 0) {
        mOptions[RSK_KEYSANITY].Set(RO_DUNGEON_ITEM_LOC_STARTWITH);
    } else if (slotData["boss_key_shuffle"] == 1) {
        mOptions[RSK_BOSS_KEYSANITY].Set(RO_DUNGEON_ITEM_LOC_VANILLA);
    } else if (slotData["boss_key_shuffle"] == 2) {
        mOptions[RSK_BOSS_KEYSANITY].Set(RO_DUNGEON_ITEM_LOC_OWN_DUNGEON);
    } else if (slotData["boss_key_shuffle"] == 3) {
        mOptions[RSK_BOSS_KEYSANITY].Set(RO_DUNGEON_ITEM_LOC_ANY_DUNGEON);
    } else if (slotData["boss_key_shuffle"] == 4) {
        mOptions[RSK_BOSS_KEYSANITY].Set(RO_DUNGEON_ITEM_LOC_OVERWORLD);
    } else if (slotData["boss_key_shuffle"] == 5) {
        mOptions[RSK_BOSS_KEYSANITY].Set(RO_DUNGEON_ITEM_LOC_ANYWHERE);
    }
    // The apworld still has the LACS-* values, which were replaced by Trigger-* values (see ConfigVersion7Updater)
    mOptions[RSK_GBK_STONE_COUNT].Set(slotData["ganons_castle_boss_key_stones_required"]);
    mOptions[RSK_GBK_MEDALLION_COUNT].Set(slotData["ganons_castle_boss_key_medallions_required"]);
    mOptions[RSK_GBK_REWARD_COUNT].Set(slotData["ganons_castle_boss_key_dungeon_rewards_required"]);
    mOptions[RSK_GBK_DUNGEON_COUNT].Set(slotData["ganons_castle_boss_key_dungeons_required"]);
    mOptions[RSK_GBK_TOKEN_COUNT].Set(slotData["ganons_castle_boss_key_skull_tokens_required"]);
    mOptions[RSK_GBK_OPTIONS].Set(slotData["ganons_castle_boss_key_greg_modifier"]);
    switch (slotData["ganons_castle_boss_key"].get<int>()) {
        case 0:
            mOptions[RSK_GANONS_BOSS_KEY].Set(RO_GANON_BOSS_KEY_VANILLA);
            break;
        case 1:
            mOptions[RSK_GANONS_BOSS_KEY].Set(RO_GANON_BOSS_KEY_ANYWHERE);
            break;
        case 2: // LACS-Vanilla (Shadow and Spirit Medallions)
            mOptions[RSK_GANONS_BOSS_KEY].Set(RO_GANON_BOSS_KEY_MEDALLIONS);
            mOptions[RSK_GBK_MEDALLION_COUNT].Set(2);
            break;
        case 3:
            mOptions[RSK_GANONS_BOSS_KEY].Set(RO_GANON_BOSS_KEY_STONES);
            break;
        case 4:
            mOptions[RSK_GANONS_BOSS_KEY].Set(RO_GANON_BOSS_KEY_MEDALLIONS);
            break;
        case 5:
            mOptions[RSK_GANONS_BOSS_KEY].Set(RO_GANON_BOSS_KEY_REWARDS);
            break;
        case 6:
            mOptions[RSK_GANONS_BOSS_KEY].Set(RO_GANON_BOSS_KEY_DUNGEONS);
            break;
        case 7:
            mOptions[RSK_GANONS_BOSS_KEY].Set(RO_GANON_BOSS_KEY_TOKENS);
            break;
    }
    mOptions[RSK_SKIP_CHILD_STEALTH].Set(RO_GENERIC_NO);
    // Skip Child Zelda is now starting with an unshuffled Zelda's Letter, which also opens the Kakariko gate.
    // An open gate without skipping Zelda has no setting anymore, Archipelago_InitSaveFile opens it.
    mOptions[RSK_SHUFFLE_ZELDAS_LETTER].Set(RO_GENERIC_NO);
    mOptions[RSK_STARTING_ZELDAS_LETTER].Set(slotData["skip_child_zelda"]);
    mOptions[RSK_STARTING_STICKS].Set(slotData["start_with_stick_ammo"]);
    mOptions[RSK_STARTING_NUTS].Set(slotData["start_with_nut_ammo"]);
    mOptions[RSK_STARTING_BEANS].Set(slotData["start_with_magic_beans"]);
    mOptions[RSK_FULL_WALLETS].Set(slotData["full_wallets"]);
    mOptions[RSK_SHUFFLE_CHEST_MINIGAME].Set(RO_GENERIC_NO);
    mOptions[RSK_BIG_POE_COUNT].Set(slotData["big_poe_target_count"]);
    mOptions[RSK_SKIP_EPONA_RACE].Set(slotData["skip_epona_race"]);
    // Completing the mask quest is now starting with every mask
    bool completeMaskQuest = slotData["complete_mask_quest"] == 1;
    mOptions[RSK_SHUFFLE_MASKS].Set(completeMaskQuest);
    for (RandomizerSettingKey mask :
         { RSK_STARTING_KEATON_MASK, RSK_STARTING_SKULL_MASK, RSK_STARTING_SPOOKY_MASK, RSK_STARTING_BUNNY_HOOD,
           RSK_STARTING_GORON_MASK, RSK_STARTING_ZORA_MASK, RSK_STARTING_GERUDO_MASK, RSK_STARTING_MASK_OF_TRUTH }) {
        mOptions[mask].Set(completeMaskQuest);
    }
    mOptions[RSK_STARTING_SCARECROWS_SONG].Set(slotData["skip_scarecrows_song"]);
    mOptions[RSK_SKIP_PLANTING_BEANS].Set(RO_GENERIC_NO);
    mOptions[RSK_SKULLS_SUNS_SONG].Set(slotData["skulls_sun_song"]);
    mOptions[RSK_SHUFFLE_ADULT_TRADE].Set(slotData["shuffle_adult_trade_items"]);
    mOptions[RSK_SHUFFLE_MERCHANTS].Set(slotData["shuffle_merchants"]);
    mOptions[RSK_MERCHANT_PRICES].Set(0);
    mOptions[RSK_MERCHANT_PRICES_FIXED_PRICE].Set(0);
    mOptions[RSK_MERCHANT_PRICES_RANGE_1].Set(0);
    mOptions[RSK_MERCHANT_PRICES_RANGE_2].Set(0);
    mOptions[RSK_MERCHANT_PRICES_NO_WALLET_WEIGHT].Set(0);
    mOptions[RSK_MERCHANT_PRICES_CHILD_WALLET_WEIGHT].Set(0);
    mOptions[RSK_MERCHANT_PRICES_ADULT_WALLET_WEIGHT].Set(0);
    mOptions[RSK_MERCHANT_PRICES_GIANT_WALLET_WEIGHT].Set(0);
    mOptions[RSK_MERCHANT_PRICES_TYCOON_WALLET_WEIGHT].Set(0);
    mOptions[RSK_MERCHANT_PRICES_AFFORDABLE].Set(0);
    mOptions[RSK_BLUE_FIRE_ARROWS].Set(slotData["blue_fire_arrows"]);
    mOptions[RSK_SUNLIGHT_ARROWS].Set(slotData["sunlight_arrows"]);
    mOptions[RSK_SLINGBOW_BREAK_BEEHIVES].Set(slotData["slingbow_break_beehives"]);
    mOptions[RSK_ENABLE_BOMBCHU_DROPS].Set(slotData["bombchu_drops"]);
    mOptions[RSK_BOMBCHU_BAG].Set(slotData["bombchu_bag"]);
    if (slotData["start_with_links_pocket"] == 0) {
        mOptions[RSK_LINKS_POCKET].Set(RO_LINKS_POCKET_DUNGEON_REWARD);
    } else if (slotData["start_with_links_pocket"] == 1) {
        mOptions[RSK_LINKS_POCKET].Set(RO_LINKS_POCKET_ADVANCEMENT);
    } else if (slotData["start_with_links_pocket"] == 2) {
        mOptions[RSK_LINKS_POCKET].Set(RO_LINKS_POCKET_ANYTHING);
    } else if (slotData["start_with_links_pocket"] == 3) {
        mOptions[RSK_LINKS_POCKET].Set(RO_LINKS_POCKET_NOTHING);
    }
    mOptions[RSK_MQ_DUNGEON_RANDOM].Set(0);
    mOptions[RSK_MQ_DUNGEON_COUNT].Set(0);
    mOptions[RSK_MQ_DUNGEON_SET].Set(0);
    mOptions[RSK_MQ_DEKU_TREE].Set(0);
    mOptions[RSK_MQ_DODONGOS_CAVERN].Set(0);
    mOptions[RSK_MQ_JABU_JABU].Set(0);
    mOptions[RSK_MQ_FOREST_TEMPLE].Set(0);
    mOptions[RSK_MQ_FIRE_TEMPLE].Set(0);
    mOptions[RSK_MQ_WATER_TEMPLE].Set(0);
    mOptions[RSK_MQ_SPIRIT_TEMPLE].Set(0);
    mOptions[RSK_MQ_SHADOW_TEMPLE].Set(0);
    mOptions[RSK_MQ_BOTTOM_OF_THE_WELL].Set(0);
    mOptions[RSK_MQ_ICE_CAVERN].Set(0);
    mOptions[RSK_MQ_GTG].Set(0);
    mOptions[RSK_MQ_GANONS_CASTLE].Set(0);
    if (slotData["key_rings"] == 0) {
        mOptions[RSK_KEYRINGS].Set(RO_KEYRINGS_OFF);
    } else if (slotData["key_rings"] == 1) {
        mOptions[RSK_KEYRINGS].Set(RO_KEYRINGS_COUNT);
    } else if (slotData["key_rings"] == 2) {
        mOptions[RSK_KEYRINGS].Set(RO_KEYRINGS_SELECTION);
    }
    mOptions[RSK_KEYRINGS_RANDOM_COUNT].Set(slotData["key_rings_count"]);
    mOptions[RSK_KEYRINGS_GERUDO_FORTRESS].Set(slotData["gerudo_fortress_key_ring"]);
    mOptions[RSK_KEYRINGS_FOREST_TEMPLE].Set(slotData["forest_temple_key_ring"]);
    mOptions[RSK_KEYRINGS_FIRE_TEMPLE].Set(slotData["fire_temple_key_ring"]);
    mOptions[RSK_KEYRINGS_WATER_TEMPLE].Set(slotData["water_temple_key_ring"]);
    mOptions[RSK_KEYRINGS_SPIRIT_TEMPLE].Set(slotData["spirit_temple_key_ring"]);
    mOptions[RSK_KEYRINGS_SHADOW_TEMPLE].Set(slotData["shadow_temple_key_ring"]);
    mOptions[RSK_KEYRINGS_BOTTOM_OF_THE_WELL].Set(slotData["bottom_of_the_well_key_ring"]);
    mOptions[RSK_KEYRINGS_GTG].Set(slotData["gerudo_training_ground_key_ring"]);
    mOptions[RSK_KEYRINGS_GANONS_CASTLE].Set(slotData["ganons_castle_key_ring"]);
    mOptions[RSK_SHUFFLE_ENTRANCES].Set(0);
    mOptions[RSK_SHUFFLE_DUNGEON_ENTRANCES].Set(0);
    mOptions[RSK_SHUFFLE_OVERWORLD_ENTRANCES].Set(0);
    mOptions[RSK_SHUFFLE_INTERIOR_ENTRANCES].Set(0);
    mOptions[RSK_SHUFFLE_THIEVES_HIDEOUT_ENTRANCES].Set(0);
    mOptions[RSK_SHUFFLE_GROTTO_ENTRANCES].Set(0);
    mOptions[RSK_SHUFFLE_OWL_DROPS].Set(0);
    mOptions[RSK_SHUFFLE_WARP_SONGS].Set(0);
    mOptions[RSK_SHUFFLE_OVERWORLD_SPAWNS].Set(0);
    mOptions[RSK_MIXED_ENTRANCE_POOLS].Set(0);
    mOptions[RSK_MIX_DUNGEON_ENTRANCES].Set(0);
    mOptions[RSK_MIX_BOSS_ENTRANCES].Set(0);
    mOptions[RSK_MIX_OVERWORLD_ENTRANCES].Set(0);
    mOptions[RSK_MIX_INTERIOR_ENTRANCES].Set(0);
    mOptions[RSK_MIX_THIEVES_HIDEOUT_ENTRANCES].Set(0);
    mOptions[RSK_MIX_GROTTO_ENTRANCES].Set(0);
    mOptions[RSK_DECOUPLED_ENTRANCES].Set(0);
    mOptions[RSK_STARTING_SKULLTULA_TOKEN].Set(0);
    uint8_t slotDataStartingHearts = slotData["starting_hearts"];
    mOptions[RSK_STARTING_HEARTS].Set(slotDataStartingHearts - 1);
    mOptions[RSK_DAMAGE_MULTIPLIER].Set(0);
    mOptions[RSK_ALL_CHECKS_REACHABLE].Set(0);
    mOptions[RSK_SHUFFLE_BOSS_ENTRANCES].Set(0);
    mOptions[RSK_SHUFFLE_100_GS_REWARD].Set(slotData["shuffle_100_gs_reward"]);
    // Triforce Hunt is now a total of pieces (0 being off) and a win or Ganon's Boss Key trigger
    // (see ConfigVersion7Updater)
    uint8_t triforcePieceTotal = slotData["triforce_hunt_pieces_total"];
    uint8_t triforcePieceRequired = slotData["triforce_hunt_pieces_required"];
    mOptions[RSK_WINCON].Set(RO_WINCON_DEFEAT_GANON);
    mOptions[RSK_TRIFORCE_HUNT_PIECES_TOTAL].Set(0);
    switch (slotData["triforce_hunt"].get<int>()) {
        case 1: // Win
            mOptions[RSK_WINCON].Set(RO_WINCON_TRIFORCE_PIECES);
            mOptions[RSK_WINCON_TRIFORCE_COUNT].Set(triforcePieceRequired);
            mOptions[RSK_TRIFORCE_HUNT_PIECES_TOTAL].Set(triforcePieceTotal);
            break;
        case 2: // Ganon's Boss Key
            mOptions[RSK_GANONS_BOSS_KEY].Set(RO_GANON_BOSS_KEY_TRIFORCE_PIECES);
            mOptions[RSK_GBK_TRIFORCE_COUNT].Set(triforcePieceRequired);
            mOptions[RSK_TRIFORCE_HUNT_PIECES_TOTAL].Set(triforcePieceTotal);
            break;
    }
    mOptions[RSK_SHUFFLE_BEAN_SOULS].Set(RO_GENERIC_NO);
    // The apworld's "On + Ganon" is now On plus Ganon's Soul anywhere
    bool shuffleGanonsSoul = slotData["shuffle_boss_souls"] == 2;
    mOptions[RSK_SHUFFLE_BOSS_SOULS].Set(shuffleGanonsSoul ? RO_BOSS_SOULS_ON
                                                           : slotData["shuffle_boss_souls"].get<int>());
    mOptions[RSK_GANONS_SOUL].Set(shuffleGanonsSoul ? RO_GANONS_SOUL_ANYWHERE : RO_GANONS_SOUL_STARTWITH);
    if (slotData["shuffle_fish"] == 0) {
        mOptions[RSK_FISHSANITY].Set(RO_FISHSANITY_OFF);
    } else if (slotData["shuffle_fish"] == 1) {
        mOptions[RSK_FISHSANITY].Set(RO_FISHSANITY_POND);
    } else if (slotData["shuffle_fish"] == 2) {
        mOptions[RSK_FISHSANITY].Set(RO_FISHSANITY_OVERWORLD);
    } else if (slotData["shuffle_fish"] == 3) {
        mOptions[RSK_FISHSANITY].Set(RO_FISHSANITY_BOTH);
    }
    mOptions[RSK_FISHSANITY_POND_COUNT].Set(15);
    mOptions[RSK_FISHSANITY_AGE_SPLIT].Set(1);
    mOptions[RSK_SHUFFLE_FISHING_POLE].Set(slotData["shuffle_fishing_pole"]);
    mOptions[RSK_INFINITE_UPGRADES].Set(slotData["infinite_upgrades"]);
    mOptions[RSK_SKELETON_KEY].Set(slotData["skeleton_key"]);
    mOptions[RSK_SHUFFLE_DEKU_STICK_BAG].Set(slotData["shuffle_deku_stick_bag"]);
    mOptions[RSK_SHUFFLE_DEKU_NUT_BAG].Set(slotData["shuffle_deku_nut_bag"]);
    mOptions[RSK_SHUFFLE_FREESTANDING].Set(slotData["shuffle_freestanding_items"]);
    mOptions[RSK_SHUFFLE_FOUNTAIN_FAIRIES].Set(slotData["shuffle_fountain_fairies"]);
    mOptions[RSK_SHUFFLE_STONE_FAIRIES].Set(slotData["shuffle_stone_fairies"]);
    mOptions[RSK_SHUFFLE_BEAN_FAIRIES].Set(slotData["shuffle_bean_fairies"]);
    mOptions[RSK_SHUFFLE_SONG_FAIRIES].Set(slotData["shuffle_song_fairies"]);
    mOptions[RSK_LOCK_OVERWORLD_DOORS].Set(slotData["lock_overworld_doors"]);
    mOptions[RSK_SHUFFLE_GRASS].Set(slotData["shuffle_grass"]);
    mOptions[RSK_ROCS_FEATHER].Set(slotData["rocs_feather"]);
    SetSeed(slotData["archipelago_seed"]);
}

void Context::ParseArchipelagoTricks() {
    Context::ResetTrickOptions();

    nlohmann::json slotData = ArchipelagoClient::GetInstance().GetSlotData();

    if (slotData["enable_all_tricks"] == 0) {
        nlohmann::json enabledTricksJson = slotData["tricks_in_logic"];
        const auto& settings = Rando::Settings::GetInstance();

        for (auto it : enabledTricksJson) {
            int rt = settings->GetRandomizerTrickByName(it);
            if (rt != -1) {
                mTrickOptions[rt].Set(RO_GENERIC_ON);
            }
        }
    } else {
        for (int count = 0; count < RT_MAX; count++) {
            mTrickOptions[count].Set(RO_GENERIC_ON);
        }
    }
}

void Context::ParseArchipelagoExcludedLocations() {
    // Maybe eventually we can add locations that are excluded on AP's side.
    // For now, remove all of them to prevent seed bleed from normal rando seeds.
    const auto ctx = Rando::Context::GetInstance();
    for (int count = 0; count < RC_MAX; count++) {
        ctx->GetItemLocation(count)->SetExcludedOption(RO_GENERIC_OFF);
    };
}

void Context::ParseArchipelagoItemsLocations(const std::vector<ArchipelagoClient::ApItem>& scouted_items) {
    const int Slot = ArchipelagoClient::GetInstance().GetSlot();
    nlohmann::json slotData = ArchipelagoClient::GetInstance().GetSlotData();

    // Zero out the location table first
    for (int rc = 1; rc < RC_MAX; rc++) {
        itemLocationTable[rc].SetPlacedItem(RG_NONE);

        if (StaticData::GetLocation((RandomizerCheck)rc)->GetRCType() == RCTYPE_SKULL_TOKEN) {
            itemLocationTable[rc].SetPlacedItem(RG_GOLD_SKULLTULA_TOKEN);
        }
    }

    for (const ArchipelagoClient::ApItem& ap_item : scouted_items) {
        const RandomizerCheck rc = StaticData::locationNameToEnum[ap_item.locationName];

        if (Slot == ap_item.playerNumber) {
            // Our item
            SPDLOG_TRACE("Populated item {} at location {}", ap_item.itemName, ap_item.locationName);
            const RandomizerGet item = StaticData::itemNameToEnum[ap_item.itemName];
            itemLocationTable[rc].SetPlacedItem(item);

            if (item == RG_ICE_TRAP) {
                RandomizerGet iceTrapItem = ArchipelagoClient::GetInstance().GetIceTrapItem();
                overrides[rc] = ItemOverride(rc, iceTrapItem);
                Traps::TrickName trickName =
                    Traps::GetTrapName(iceTrapItem, static_cast<RandoIceTrapNames>(mOptions[RSK_ICE_TRAP_NAMES].Get()));
                overrides[rc].SetTrickName(trickName.name);
                overrides[rc].SetTrickArticle(trickName.article);
            }
        } else {
            // Other player item
            // If progressive or trap bit flag is set, make item progressive.
            if (ap_item.flags & (1 << 0) || ap_item.flags & (1 << 2)) {
                itemLocationTable[rc].SetPlacedItem(RG_ARCHIPELAGO_ITEM_PROGRESSIVE);
                // If useful bit flag is on, make item useful.
            } else if (ap_item.flags & (1 << 1)) {
                itemLocationTable[rc].SetPlacedItem(RG_ARCHIPELAGO_ITEM_USEFUL);
                // None of these flags being present means it's junk.
            } else {
                itemLocationTable[rc].SetPlacedItem(RG_ARCHIPELAGO_ITEM_JUNK);
            }
        }
    }

    // Place vanilla shop items
    nlohmann::json vanillaShopItems = slotData["shop_vanilla_items"];
    for (auto it = vanillaShopItems.begin(); it != vanillaShopItems.end(); it++) {
        std::string location = it.key();
        std::string itemName = it.value();
        const RandomizerCheck rc = StaticData::locationNameToEnum[location];
        const RandomizerGet item = StaticData::itemNameToEnum[itemName];
        itemLocationTable[rc].SetPlacedItem(item);
    }

    // Set all shop, scrub and merchant prices
    nlohmann::json shopPrices = slotData["shop_prices"];
    for (auto it = shopPrices.begin(); it != shopPrices.end(); it++) {
        std::string location = it.key();
        uint16_t price = it.value();
        const RandomizerCheck rc = StaticData::locationNameToEnum[location];
        itemLocationTable[rc].SetCustomPrice(price);
    }
}
void Context::ParseArchipelagoHints() {
    const auto& ApHintData = ArchipelagoClient::GetInstance().foreignHints;
    const auto ctx = Rando::Context::GetInstance();
    for (const auto& ApHint : ApHintData) {
        const RandomizerHint hintKey = ApHint.first;
        const StaticHintInfo hintInfo = StaticData::staticHintInfoMap[ApHint.first];
        std::vector<RandomizerArea> areas;
        for (const ArchipelagoClient::ApForeignHint& hintData : ApHint.second) {
            areas.emplace_back(RA_ARCHIPELAGO_FOREIGN);
        }
        if (areas.empty()) {
            areas.emplace_back(RA_NONE);
        }
        HintType hintType;
        std::vector<RandomizerHintTextKey> textKeys;
        switch (hintKey) {
            case RH_ALTAR_CHILD:
                hintType = HINT_TYPE_ALTAR_CHILD;
                break;
            case RH_ALTAR_ADULT:
                hintType = HINT_TYPE_ALTAR_ADULT;
                break;
            case RH_GANONDORF_HINT:
                hintType = HINT_TYPE_AREA;
                if (ctx->GetOption(RSK_SHUFFLE_MASTER_SWORD) &&
                    ctx->GetOption(RSK_STARTING_MASTER_SWORD).Is(RO_GENERIC_OFF)) {
                    textKeys = { RHT_GANONDORF_HINT_LA_ONLY, RHT_GANONDORF_HINT_MS_ONLY, RHT_GANONDORF_HINT_LA_AND_MS };
                } else {
                    textKeys = { RHT_GANONDORF_HINT_LA_ONLY };
                }
                break;
            case RH_GANONDORF_JOKE:
                continue; // just create a random joke
            default:
                hintType = hintInfo.type;
                break;
        }
        Hint hint = Hint(hintKey, hintType, textKeys, {}, areas);
        AddHint(hintKey, hint);
    }
    CreateStaticHints();
}

void Context::WriteHintJson(nlohmann::ordered_json& spoilerFileJson) {
    for (Hint hint : hintTable) {
        hint.logHint(spoilerFileJson);
    }
}

void Context::ParseHintJson(const nlohmann::json& spoilerFileJson) {
    nlohmann::json gossipHintsJson = spoilerFileJson.value("Gossip Stone Hints", nlohmann::json());
    for (auto hintData : gossipHintsJson.items()) {
        RandomizerHint hint = (RandomizerHint)StaticData::hintNameToEnum[hintData.key()];
        AddHint(hint, Hint(hint, hintData.value()));
    }
    nlohmann::json staticHintsJson = spoilerFileJson.value("Static Hints", nlohmann::json());
    for (auto hintData : staticHintsJson.items()) {
        RandomizerHint hint = (RandomizerHint)StaticData::hintNameToEnum[hintData.key()];
        AddHint(hint, Hint(hint, hintData.value()));
    }
    CreateStaticHints();
}

void Context::ParseTricksJson(const nlohmann::json& spoilerFileJson) {
    nlohmann::json enabledTricksJson = spoilerFileJson.value("enabledTricks", nlohmann::json());
    const auto& settings = Rando::Settings::GetInstance();
    for (auto it : enabledTricksJson) {
        int rt = settings->GetRandomizerTrickByName(it);
        if (rt != -1) {
            mTrickOptions[rt].Set(RO_GENERIC_ON);
        }
    }
}

std::shared_ptr<EntranceShuffler> Context::GetEntranceShuffler() {
    return mEntranceShuffler;
}

std::shared_ptr<Dungeons> Context::GetDungeons() {
    return mDungeons;
}

DungeonInfo* Context::GetDungeon(size_t key) const {
    return mDungeons->GetDungeon(static_cast<DungeonKey>(key));
}

DungeonInfo* Context::GetDungeonFromScene(SceneID scene) const {
    return mDungeons->GetDungeonFromScene(scene);
}

std::shared_ptr<Logic> Context::GetLogic() {
    if (mLogic.get() == nullptr) {
        mLogic = std::make_shared<Logic>();
    }
    return mLogic;
}

std::shared_ptr<Trials> Context::GetTrials() {
    return mTrials;
}

TrialInfo* Context::GetTrial(size_t key) const {
    return mTrials->GetTrial(static_cast<TrialKey>(key));
}

TrialInfo* Context::GetTrial(TrialKey key) const {
    return mTrials->GetTrial(key);
}

Sprite* Context::GetSeedTexture(const uint8_t index) {
    return &gSeedTextures[index];
}

OptionValue& Context::GetOption(const RandomizerSettingKey key) {
    return mOptions[key];
}

OptionValue& Context::GetTrickOption(const RandomizerTrick key) {
    return mTrickOptions[key];
}

OptionValue& Context::GetLocationOption(const RandomizerCheck key) {
    return itemLocationTable[key].GetExcludedOption();
}

RandoOptionCheckTrigger Context::GBKCondition() const {
    return mGBKCondition;
}

void Context::GBKCondition(RandoOptionCheckTrigger condition) {
    mGBKCondition = condition;
}

RandoOptionCheckTrigger Context::GanonsSoulCondition() const {
    return mGanonsSoulCondition;
}

void Context::GanonsSoulCondition(RandoOptionCheckTrigger condition) {
    mGanonsSoulCondition = condition;
}

RandoOptionWincon Context::WinCondition() const {
    return mWinCondition;
}

void Context::WinCondition(RandoOptionWincon condition) {
    mWinCondition = condition;
}

std::shared_ptr<Kaleido> Context::GetKaleido() {
    if (mKaleido == nullptr) {
        mKaleido = std::make_shared<Kaleido>();
    }
    return mKaleido;
}

std::string Context::GetHash() const {
    return mHash;
}

void Context::SetHash(std::string hash) {
    mHash = std::move(hash);
}

uint8_t Context::GetBombchuCapacity() {
    switch (mLogic->GetSaveContext()->ship.quest.data.randomizer.bombchuUpgradeLevel) {
        case 0:
            return 0;
        case 1:
            return 20;
        case 2:
            return 30;
        case 3:
            return 50;
        default:
            return 0;
    }
}

void Context::HandleGetBombchuBag() {
    if (GetOption(RSK_BOMBCHU_BAG).Is(RO_BOMBCHU_BAG_SINGLE)) {
        if (INV_CONTENT(ITEM_BOMBCHU) == ITEM_NONE) {
            INV_CONTENT(ITEM_BOMBCHU) = ITEM_BOMBCHU;
            AMMO(ITEM_BOMBCHU) = 20;
        } else if (OTRGlobals::Instance->gRandomizer->GetRandoSettingValue(RSK_INFINITE_UPGRADES)) {
            Flags_SetRandomizerInf(RAND_INF_HAS_INFINITE_BOMBCHUS);
        } else {
            AMMO(ITEM_BOMBCHU) += 10;
            if (AMMO(ITEM_BOMBCHU) > 50) {
                AMMO(ITEM_BOMBCHU) = 50;
            }
        }
        return;
    }
    switch (mLogic->GetSaveContext()->ship.quest.data.randomizer.bombchuUpgradeLevel) {
        case 0:
        case 1:
        case 2:
            mLogic->GetSaveContext()->ship.quest.data.randomizer.bombchuUpgradeLevel++;
            if (INV_CONTENT(ITEM_BOMBCHU) == ITEM_NONE) {
                INV_CONTENT(ITEM_BOMBCHU) = ITEM_BOMBCHU;
            } else if (GetOption(RSK_INFINITE_UPGRADES).Is(RO_INF_UPGRADES_CONDENSED_PROGRESSIVE)) {
                Flags_SetRandomizerInf(RAND_INF_HAS_INFINITE_BOMBCHUS);
            }
            AMMO(ITEM_BOMBCHU) = GetBombchuCapacity();
            return;
        case 3:
            if (GetOption(RSK_INFINITE_UPGRADES).IsNot(RO_INF_UPGRADES_OFF)) {
                Flags_SetRandomizerInf(RAND_INF_HAS_INFINITE_BOMBCHUS);
            }
            return;
    }
}

const std::string& Context::GetSeedString() const {
    return mSeedString;
}

void Context::SetSeedString(std::string seedString) {
    mSeedString = std::move(seedString);
}

uint32_t Context::GetSeed() const {
    return mFinalSeed;
}

void Context::SetSeed(const uint32_t seed) {
    mFinalSeed = seed;
}
} // namespace Rando
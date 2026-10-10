#pragma once

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

/**
 * Settings the room owner decides for everyone while Co-op Sync is on, like the enemy randomizer and Hyper Enemies.
 * Mirrored enemies have to be the same actor acting the same way in everyone's game, so these can't differ between
 * teammates. The owner's values come with the room state and replace each player's own while they're in the room,
 * without changing their saved settings.
 */
namespace CoopSettings {

// Registers CVars whose value the room owner decides. onChange runs (on the game thread) whenever the value in effect
// of one of them changes, after the ShipInit functions listening to it re-ran.
struct Shared {
    Shared(std::initializer_list<std::string> cvars, std::function<void()> onChange = nullptr);
    Shared(std::vector<std::string> cvars, std::function<void()> onChange = nullptr);
};

// Whether the room owner's values are in effect
bool IsEnforced();

// The value in effect: the room owner's while enforced (defaultValue when the owner has it unset), else our own
int32_t GetInteger(const char* cvarName, int32_t defaultValue);

// The room's seed for anything random about enemies, so it comes out the same for everyone. Only valid while enforced.
uint32_t GetEnemySeed();

// Used by Anchor: the registered CVars, sent with the room state by the room owner
const std::vector<std::string>& GetSharedCVars();
// Used by Anchor, once a frame: re-registers the hooks of settings whose value in effect changed
void Update();

} // namespace CoopSettings

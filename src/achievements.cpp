#include "achievements.h"

#include <algorithm>
#include <ctime>

#include "blocks.h"
#include "inventory.h"

namespace {

struct Def { AdvId id; const char* key; const char* title; const char* desc; };

const Def kDefs[] = {
    {AdvId::OpeningBook, "minecraft:story/root", "Minecraft",
     "The beginning of a long journey"},
    {AdvId::GettingAnUpgrade, "minecraft:story/upgrade_tools", "Getting an Upgrade",
     "Construct a better pickaxe"},
    {AdvId::CraftIronPickaxe, "minecraft:story/iron_tools", "Isn't It Iron Pick",
     "Upgrade your pickaxe"},
    {AdvId::MineStone, "minecraft:story/mine_stone", "Stone Age",
     "Mine stone with your new pickaxe"},
    {AdvId::MineDiamond, "minecraft:story/mine_diamond", "Diamonds!",
     "Acquire diamonds"},
    {AdvId::AcquireHardenedClay, "minecraft:story/acquire_hardened_clay", "Redstone [WIP]",
     "Mine a block of terracotta"},
    {AdvId::MakeBread, "minecraft:story/make_bread", "Bake Bread",
     "Turn wheat into bread"},
    {AdvId::TalkToVillager, "minecraft:story/talk_to_villager", "What a Deal!",
     "Successfully trade with a Villager"},
    {AdvId::SleepInBed, "minecraft:story/sleep_in_bed", "Sweet Dreams",
     "Sleep in a bed to change your sleep cycle"},
    {AdvId::KillMob, "minecraft:story/kill_a_mob", "Monster Hunter",
     "Attack and defeat a hostile mob"},
    {AdvId::KillCreeper, "minecraft:story/kill_creeper", "Not Today, Thank You",
     "Defeat a Creeper"},
    {AdvId::KillEnderman, "minecraft:story/kill_enderman", "The End?",
     "Defeat an Enderman"},
    {AdvId::ShootBow, "minecraft:story/shoot_arrow", "Take Aim",
     "Shoot something with a Bow"},
    {AdvId::EatFood, "minecraft:story/eat_food", "A Balanced Diet",
     "Eat everything on the food spectrum"},
    {AdvId::EnchantItem, "minecraft:story/enchant_item", "Enchanter",
     "Enchant an item at an Enchanting Table"},
    {AdvId::BrewPotion, "minecraft:story/brew_potion", "Local Brewery",
     "Brew a potion"},
    {AdvId::GoToNether, "minecraft:story/enter_the_nether", "Into the Nether",
     "Build a Nether Portal and enter it"},
    {AdvId::GoToEnd, "minecraft:story/enter_the_end", "The End?",
     "Locate an End Portal and enter it"},
    {AdvId::DefeatEnderDragon, "minecraft:end/kill_dragon", "The End.",
     "Defeat the Ender Dragon"},
    {AdvId::TheEnd, "minecraft:end/root", "The End of the Game",
     "Behold the credits"},
};

} // namespace

AchievementBook::AchievementBook() {
    entries_.reserve(static_cast<size_t>(AdvId::Count));
    for (const Def& d : kDefs) entries_.push_back({d.id, d.key, d.title, d.desc, false, 0});
}

void AchievementBook::grant(AdvId id) {
    if (static_cast<size_t>(id) >= entries_.size()) return;
    Advancement& a = entries_[static_cast<size_t>(id)];
    if (a.achieved) return;
    a.achieved = true;
    a.timestamp = static_cast<uint64_t>(std::time(nullptr));
}

bool AchievementBook::has(AdvId id) const {
    if (static_cast<size_t>(id) >= entries_.size()) return false;
    return entries_[static_cast<size_t>(id)].achieved;
}

int AchievementBook::grantedCount() const {
    int n = 0;
    for (const auto& a : entries_) if (a.achieved) ++n;
    return n;
}

void AchievementBook::reset() {
    for (auto& a : entries_) { a.achieved = false; a.timestamp = 0; }
}

const char* advTitle(AdvId id) {
    if (static_cast<size_t>(id) >= static_cast<size_t>(AdvId::Count)) return "";
    return kDefs[static_cast<size_t>(id)].title;
}

void AchievementBook::onBlockBroken(uint8_t block) {
    grant(AdvId::OpeningBook);
    if (block == STONE) grant(AdvId::MineStone);
}

void AchievementBook::onCraft(uint16_t itemId) {
    if (itemId >= ITEM_STONE_PICKAXE && itemId <= ITEM_NETHERITE_PICKAXE) {
        grant(AdvId::GettingAnUpgrade);
        if (itemId >= ITEM_IRON_PICKAXE) grant(AdvId::CraftIronPickaxe);
    }
    if (itemId == ITEM_BREAD) grant(AdvId::MakeBread);
}

void AchievementBook::onMobKilled(uint8_t mobType) {
    grant(AdvId::KillMob);
    // 11 = Creeper, 12 = Enderman (matches MobType enum ordering in mobs.h)
    if (mobType == 11) grant(AdvId::KillCreeper);
    if (mobType == 12) grant(AdvId::KillEnderman);
}

void AchievementBook::onItemPicked(uint16_t itemId) {
    if (itemId == ITEM_DIAMOND) grant(AdvId::MineDiamond);
    if (itemId == ITEM_ENDER_EYE) grant(AdvId::GoToEnd);
}

void AchievementBook::onDimensionEntered(int dim) {
    if (dim == -1) grant(AdvId::GoToNether);
    if (dim == 1) grant(AdvId::GoToEnd);
}

void AchievementBook::onDragonDefeated() {
    grant(AdvId::DefeatEnderDragon);
    grant(AdvId::TheEnd);
}

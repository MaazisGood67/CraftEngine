// CraftEngine - Minecraft advancements ("achievements") tracker.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

enum class AdvId : uint16_t {
    OpeningBook,          // "Minecraft" - take your first steps
    GettingAnUpgrade,     // craft a better pickaxe
    CraftIronPickaxe,
    MineStone,
    MineDiamond,
    AcquireHardenedClay,
    MakeBread,
    TalkToVillager,
   SleepInBed,
    KillMob,
    KillCreeper,
    KillEnderman,
    ShootBow,
    EatFood,
    EnchantItem,
    BrewPotion,
    GoToNether,
    GoToEnd,
    DefeatEnderDragon,    // "The End?" -> "The End."
    TheEnd,               // credits
    Count
};

struct Advancement {
    AdvId id;
    const char* key;       // vanilla advancement id
    const char* title;
    const char* description;
    bool achieved = false;
    uint64_t timestamp = 0;
};

class AchievementBook {
public:
    AchievementBook();
    void grant(AdvId id);
    bool has(AdvId id) const;
    int  grantedCount() const;
    void reset();
    const std::vector<Advancement>& all() const { return entries_; }

    // Called from gameplay events.
    void onBlockBroken(uint8_t block);
    void onCraft(uint16_t itemId);
    void onMobKilled(uint8_t mobType);
    void onItemPicked(uint16_t itemId);
    void onDimensionEntered(int dim);
    void onDragonDefeated();

private:
    std::vector<Advancement> entries_;
};

const char* advTitle(AdvId id);

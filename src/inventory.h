// CraftEngine - vanilla-style inventory, item stacks and crafting grid.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Item ids used by the engine. The complete 26.3 item list lives in
// features.h (generated from the jar); these are the ones wired into play.
enum ItemId : uint16_t {
    ITEM_AIR = 0,
    ITEM_GRASS_BLOCK, ITEM_DIRT, ITEM_STONE, ITEM_COBBLESTONE,
    ITEM_OAK_PLANKS, ITEM_OAK_LOG, ITEM_OAK_LEAVES, ITEM_SAND,
    ITEM_BEDROCK, ITEM_WATER_BUCKET, ITEM_BUCKET,
    ITEM_WOODEN_PICKAXE, ITEM_STONE_PICKAXE, ITEM_IRON_PICKAXE,
    ITEM_DIAMOND_PICKAXE, ITEM_NETHERITE_PICKAXE,
    ITEM_WOODEN_SWORD, ITEM_STONE_SWORD, ITEM_IRON_SWORD,
    ITEM_DIAMOND_SWORD, ITEM_NETHERITE_SWORD,
    ITEM_WOODEN_AXE, ITEM_STONE_AXE, ITEM_IRON_AXE,
    ITEM_DIAMOND_AXE, ITEM_NETHERITE_AXE,
    ITEM_WOODEN_SHOVEL, ITEM_STONE_SHOVEL, ITEM_IRON_SHOVEL,
    ITEM_DIAMOND_SHOVEL, ITEM_NETHERITE_SHOVEL,
    ITEM_WOODEN_HOE, ITEM_STONE_HOE, ITEM_IRON_HOE,
    ITEM_DIAMOND_HOE, ITEM_NETHERITE_HOE,
    ITEM_BREAD, ITEM_APPLE, ITEM_GOLDEN_APPLE, ITEM_COOKED_BEEF,
    ITEM_COAL, ITEM_IRON_INGOT, ITEM_DIAMOND, ITEM_STICK,
    ITEM_FLINT_AND_STEEL, ITEM_TORCH, ITEM_ENDER_PEARL,
    ITEM_ENDER_EYE, ITEM_BLAZE_ROD, ITEM_FIREWORK,
    ITEM_ARMOR_HELMET, ITEM_ARMOR_CHEST, ITEM_ARMOR_LEGS, ITEM_ARMOR_BOOTS,
    ITEM_SHIELD, ITEM_BOW, ITEM_ARROW, ITEM_CROSSBOW,
    ITEM_COUNT
};

struct ItemStack {
    uint16_t id = ITEM_AIR;
    uint16_t count = 0;
    int32_t  damage = 0;   // durability used

    bool empty() const { return id == ITEM_AIR || count == 0; }
    void clear() { id = ITEM_AIR; count = 0; damage = 0; }
};

// vanilla static properties of an item
struct ItemDef {
    ItemId id;
    const char* name;
    int maxStack;
    int maxDurability; // 0 = not damageable
    int toolTier;      // 0 = not a tool, 1 wood .. 5 netherite
    float attackDamage;
    float attackSpeed;
};

const ItemDef& itemDef(uint16_t id);
const char* itemName(uint16_t id);
int itemMaxStack(uint16_t id);
float itemAttackDamage(uint16_t id);

// Player container: 9 hotbar + 27 main + 4 armor + 1 offhand + 2x2 craft.
class Inventory {
public:
    static constexpr int HOTBAR = 9;
    static constexpr int MAIN   = 27;
    static constexpr int STORAGE = HOTBAR + MAIN; // 36
    static constexpr int ARMOR  = 4;
    static constexpr int OFFHAND = 1;
    static constexpr int CRAFT  = 4;

    ItemStack slots[STORAGE + ARMOR + OFFHAND + CRAFT + 1]{}; // last = craft output

    ItemStack& hotbar(int i) { return slots[i >= 0 && i < HOTBAR ? i : 0]; }
    const ItemStack& hotbar(int i) const { return slots[i >= 0 && i < HOTBAR ? i : 0]; }
    ItemStack& main(int i) { return slots[HOTBAR + (i >= 0 && i < MAIN ? i : 0)]; }
    ItemStack& armor(int i) { return slots[STORAGE + (i >= 0 && i < ARMOR ? i : 0)]; }
    const ItemStack& armor(int i) const { return slots[STORAGE + (i >= 0 && i < ARMOR ? i : 0)]; }
    ItemStack& offhand() { return slots[STORAGE + ARMOR]; }
    ItemStack& craft(int i) { return slots[STORAGE + ARMOR + OFFHAND + (i >= 0 && i < CRAFT ? i : 0)]; }
    ItemStack& craftOutput() { return slots[STORAGE + ARMOR + OFFHAND + CRAFT]; }

    int add(ItemStack stack);   // returns leftover count
    bool canFit(const ItemStack& stack) const;
    void swap(int a, int b);
    ItemStack take(int slot, int amount);
    void damageItem(int slot, int amount);
    void updateCrafting();
    void clearCrafting();
    void clear();
    uint16_t selected = 0; // hotbar index
    ItemStack& selectedStack() { return hotbar(static_cast<int>(selected)); }
    const ItemStack& selectedStack() const { return hotbar(static_cast<int>(selected)); }
};

#include "inventory.h"

#include <algorithm>
#include <cstring>

namespace {

const ItemDef kItems[] = {
    {ITEM_AIR, "air", 0, 0, 0, 0, 0},
    {ITEM_GRASS_BLOCK, "Grass Block", 64, 0, 0, 0, 0},
    {ITEM_DIRT, "Dirt", 64, 0, 0, 0, 0},
    {ITEM_STONE, "Stone", 64, 0, 0, 0, 0},
    {ITEM_COBBLESTONE, "Cobblestone", 64, 0, 0, 0, 0},
    {ITEM_OAK_PLANKS, "Oak Planks", 64, 0, 0, 0, 0},
    {ITEM_OAK_LOG, "Oak Log", 64, 0, 0, 0, 0},
    {ITEM_OAK_LEAVES, "Oak Leaves", 64, 0, 0, 0, 0},
    {ITEM_SAND, "Sand", 64, 0, 0, 0, 0},
    {ITEM_BEDROCK, "Bedrock", 64, 0, 0, 0, 0},
    {ITEM_WATER_BUCKET, "Water Bucket", 1, 0, 0, 0, 0},
    {ITEM_BUCKET, "Bucket", 16, 0, 0, 0, 0},
    {ITEM_WOODEN_PICKAXE, "Wooden Pickaxe", 1, 59, 1, 2, 1.2f},
    {ITEM_STONE_PICKAXE, "Stone Pickaxe", 1, 131, 2, 3, 1.2f},
    {ITEM_IRON_PICKAXE, "Iron Pickaxe", 1, 250, 3, 4, 1.2f},
    {ITEM_DIAMOND_PICKAXE, "Diamond Pickaxe", 1, 1561, 4, 5, 1.2f},
    {ITEM_NETHERITE_PICKAXE, "Netherite Pickaxe", 1, 2031, 5, 6, 1.2f},
    {ITEM_WOODEN_SWORD, "Wooden Sword", 1, 59, 1, 4, 1.6f},
    {ITEM_STONE_SWORD, "Stone Sword", 1, 131, 2, 5, 1.6f},
    {ITEM_IRON_SWORD, "Iron Sword", 1, 250, 3, 6, 1.6f},
    {ITEM_DIAMOND_SWORD, "Diamond Sword", 1, 1561, 4, 7, 1.6f},
    {ITEM_NETHERITE_SWORD, "Netherite Sword", 1, 2031, 5, 8, 1.6f},
    {ITEM_WOODEN_AXE, "Wooden Axe", 1, 59, 1, 7, 0.8f},
    {ITEM_STONE_AXE, "Stone Axe", 1, 131, 2, 9, 0.8f},
    {ITEM_IRON_AXE, "Iron Axe", 1, 250, 3, 9, 0.8f},
    {ITEM_DIAMOND_AXE, "Diamond Axe", 1, 1561, 4, 9, 0.8f},
    {ITEM_NETHERITE_AXE, "Netherite Axe", 1, 2031, 5, 10, 0.8f},
    {ITEM_WOODEN_SHOVEL, "Wooden Shovel", 1, 59, 1, 2, 1.0f},
    {ITEM_STONE_SHOVEL, "Stone Shovel", 1, 131, 2, 3, 1.0f},
    {ITEM_IRON_SHOVEL, "Iron Shovel", 1, 250, 3, 4, 1.0f},
    {ITEM_DIAMOND_SHOVEL, "Diamond Shovel", 1, 1561, 4, 5, 1.0f},
    {ITEM_NETHERITE_SHOVEL, "Netherite Shovel", 1, 2031, 5, 6, 1.0f},
    {ITEM_WOODEN_HOE, "Wooden Hoe", 1, 59, 1, 1, 1.0f},
    {ITEM_STONE_HOE, "Stone Hoe", 1, 131, 2, 1, 1.0f},
    {ITEM_IRON_HOE, "Iron Hoe", 1, 250, 3, 1, 1.0f},
    {ITEM_DIAMOND_HOE, "Diamond Hoe", 1, 1561, 4, 1, 1.0f},
    {ITEM_NETHERITE_HOE, "Netherite Hoe", 1, 2031, 5, 1, 1.0f},
    {ITEM_BREAD, "Bread", 64, 0, 0, 0, 0},
    {ITEM_APPLE, "Apple", 64, 0, 0, 0, 0},
    {ITEM_GOLDEN_APPLE, "Golden Apple", 64, 0, 0, 0, 0},
    {ITEM_COOKED_BEEF, "Steak", 64, 0, 0, 0, 0},
    {ITEM_COAL, "Coal", 64, 0, 0, 0, 0},
    {ITEM_IRON_INGOT, "Iron Ingot", 64, 0, 0, 0, 0},
    {ITEM_DIAMOND, "Diamond", 64, 0, 0, 0, 0},
    {ITEM_STICK, "Stick", 64, 0, 0, 0, 0},
    {ITEM_FLINT_AND_STEEL, "Flint and Steel", 1, 64, 0, 0, 0},
    {ITEM_TORCH, "Torch", 64, 0, 0, 0, 0},
    {ITEM_ENDER_PEARL, "Ender Pearl", 16, 0, 0, 0, 0},
    {ITEM_ENDER_EYE, "Eye of Ender", 16, 0, 0, 0, 0},
    {ITEM_BLAZE_ROD, "Blaze Rod", 64, 0, 0, 0, 0},
    {ITEM_FIREWORK, "Firework Rocket", 64, 0, 0, 0, 0},
    {ITEM_ARMOR_HELMET, "Diamond Helmet", 1, 363, 0, 0, 0},
    {ITEM_ARMOR_CHEST, "Diamond Chestplate", 1, 528, 0, 0, 0},
    {ITEM_ARMOR_LEGS, "Diamond Leggings", 1, 463, 0, 0, 0},
    {ITEM_ARMOR_BOOTS, "Diamond Boots", 1, 325, 0, 0, 0},
    {ITEM_SHIELD, "Shield", 1, 336, 0, 0, 0},
    {ITEM_BOW, "Bow", 1, 384, 0, 0, 0},
    {ITEM_ARROW, "Arrow", 64, 0, 0, 0, 0},
    {ITEM_CROSSBOW, "Crossbow", 1, 326, 0, 0, 0},
};

const ItemDef kFallback{ITEM_AIR, "Unknown", 64, 0, 0, 0, 0};

} // namespace

const ItemDef& itemDef(uint16_t id) {
    if (id < ITEM_COUNT) return kItems[id];
    return kFallback;
}
const char* itemName(uint16_t id) { return itemDef(id).name; }
int itemMaxStack(uint16_t id) { return itemDef(id).maxStack; }
float itemAttackDamage(uint16_t id) { return itemDef(id).attackDamage; }

int Inventory::add(ItemStack stack) {
    if (stack.empty()) return 0;
    const int max = itemMaxStack(stack.id);
    // Stack into existing slots first.
    for (int i = 0; i < STORAGE; ++i) {
        ItemStack& s = slots[i];
        if (s.id == stack.id && static_cast<int>(s.count) < max) {
            const int room = max - static_cast<int>(s.count);
            const int put = std::min(room, static_cast<int>(stack.count));
            s.count = static_cast<uint16_t>(s.count + put);
            stack.count = static_cast<uint16_t>(stack.count - put);
            if (stack.count == 0) return 0;
        }
    }
    // Then into empty slots.
    for (int i = 0; i < STORAGE && stack.count > 0; ++i) {
        if (slots[i].empty()) {
            const int put = std::min(max, static_cast<int>(stack.count));
            slots[i] = {stack.id, static_cast<uint16_t>(put), stack.damage};
            stack.count = static_cast<uint16_t>(stack.count - put);
        }
    }
    return static_cast<int>(stack.count);
}

bool Inventory::canFit(const ItemStack& stack) const {
    if (stack.empty()) return true;
    const int max = itemMaxStack(stack.id);
    int room = 0;
    for (int i = 0; i < STORAGE; ++i) {
        const ItemStack& s = slots[i];
        if (s.empty()) room += max;
        else if (s.id == stack.id) room += max - static_cast<int>(s.count);
    }
    return room >= static_cast<int>(stack.count);
}

void Inventory::swap(int a, int b) {
    if (a < 0 || b < 0 || a >= static_cast<int>(sizeof(slots)/sizeof(slots[0])) ||
        b >= static_cast<int>(sizeof(slots)/sizeof(slots[0]))) return;
    std::swap(slots[a], slots[b]);
}

ItemStack Inventory::take(int slot, int amount) {
    if (slot < 0 || slot >= static_cast<int>(sizeof(slots)/sizeof(slots[0]))) return {};
    ItemStack& s = slots[slot];
    if (s.empty()) return {};
    const int got = std::min(amount, static_cast<int>(s.count));
    ItemStack out{s.id, static_cast<uint16_t>(got), s.damage};
    s.count = static_cast<uint16_t>(s.count - got);
    if (s.count == 0) s.clear();
    return out;
}

void Inventory::damageItem(int slot, int amount) {
    if (slot < 0 || slot >= static_cast<int>(sizeof(slots)/sizeof(slots[0]))) return;
    ItemStack& s = slots[slot];
    const int max = itemDef(s.id).maxDurability;
    if (max <= 0) return;
    s.damage += amount;
    if (s.damage >= max) s.clear(); // item breaks (vanilla: plays break sound)
}

void Inventory::clearCrafting() {
    for (int i = 0; i < CRAFT; ++i) craft(i).clear();
    craftOutput().clear();
}

void Inventory::updateCrafting() {
    craftOutput().clear();
    // Planks from a log: 1 log -> 4 planks.
    ItemStack log = {};
    int logCount = 0;
    for (int i = 0; i < CRAFT; ++i) if (craft(i).id == ITEM_OAK_LOG) { log = craft(i); ++logCount; }
    if (logCount == 1) craftOutput() = {ITEM_OAK_PLANKS, 4, 0};

    // Sticks: 2 planks vertical -> 4 sticks.
    if (craft(0).id == ITEM_OAK_PLANKS && craft(2).id == ITEM_OAK_PLANKS) {
        craftOutput() = {ITEM_STICK, 4, 0};
    }

    // Wooden pickaxe: 3 planks top row + 2 sticks centred below.
    if (craft(0).id == ITEM_OAK_PLANKS && craft(1).id == ITEM_OAK_PLANKS &&
        craft(2).id == ITEM_OAK_PLANKS && craft(3).id == ITEM_STICK) {
        craftOutput() = {ITEM_WOODEN_PICKAXE, 1, 0};
    }

    // Torch: coal above stick.
    if (craft(0).id == ITEM_COAL && craft(2).id == ITEM_STICK) {
        craftOutput() = {ITEM_TORCH, 4, 0};
    }
}

void Inventory::clear() {
    for (auto& s : slots) s.clear();
    selected = 0;
}

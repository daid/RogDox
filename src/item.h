#pragma once

enum class ItemType
{
    Dagger,
    Sword,
    Hammer,
    Shield,
    Armor,
    HealingPotion,
};

static int itemTypeToTileIdx(ItemType type) {
    switch(type) {
    case ItemType::Dagger: return 0;
    case ItemType::Sword: return 1 + 1 * 8;
    case ItemType::Hammer: return 4 + 4 * 8;
    case ItemType::Shield: return 1 + 11 * 8;
    case ItemType::Armor: return 5 + 12 * 8;
    case ItemType::HealingPotion: return 1 + 19 * 8;
    }
    return 0;
}

static int weaponDamage(ItemType type) {
    switch(type) {
    case ItemType::Dagger: return 2;
    case ItemType::Sword: return 4;
    case ItemType::Hammer: return 6;
    case ItemType::Shield: return 0;
    case ItemType::Armor: return 0;
    case ItemType::HealingPotion: return 0;
    }
    return 0;
}

static int weaponDurability(ItemType type) {
    switch(type) {
    case ItemType::Dagger: return 0;
    case ItemType::Sword: return 10;
    case ItemType::Hammer: return 8;
    case ItemType::Shield: return 0;
    case ItemType::Armor: return 0;
    case ItemType::HealingPotion: return 0;
    }
    return 0;
}
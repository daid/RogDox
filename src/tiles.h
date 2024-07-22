#pragma once

static inline constexpr int tileset_w = 21;
static inline constexpr std::array<int, 5> door_tiles = {16*21+0, 16*21+1, 16*21+2, 16*21+4, 16*21+6};

static bool isDoor(int tile) {
    for(auto n : door_tiles) if (n == tile) return true;
    return false;
}

static bool isWall(int tile) {
    if (tile < 0) return false;
    if (tile < 6*tileset_w) return true;
    return false;
}

static bool isSolid(int tile) {
    if (isWall(tile)) return true;
    if (isDoor(tile)) return true;
    return false;
}

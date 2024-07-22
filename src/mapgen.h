#pragma once

#include "array2.h"
#include "tiles.h"
#include "direction.h"
#include "item.h"
#include <sp2/math/rect.h>
#include <sp2/random.h>
#define MAP_SIZE 16

class MapGen
{
public:
    struct Room {
        sp::Rect2i rect;
        int tileset = 0;
        bool connected = false;

        sp::Vector2i randomPoint() {
            return {sp::irandom(rect.position.x+1, rect.position.x+rect.size.x-2), sp::irandom(rect.position.y+1, rect.position.y+rect.size.y-2)};
        }
    };

    MapGen(int connection_flags, int depth) {
        tiles.fill(-2);

        for(int n=0;n<50-int(rooms.size()*10);n++)
            tryAddRoom();
        for(auto& r : rooms) {
            for(auto p : r.rect) {
                tiles[p] = r.tileset * 21;
            }
            for(auto p : sp::Rect2i(r.rect.position+sp::Vector2i{1, 1}, r.rect.size-sp::Vector2i{2,2})) {
                tiles[p] = -1;
            }
        }
        rooms[0].connected = true;
        while(unconnectedRooms()) {
            connectRoom();
        }
        connectRoom();
        if (connection_flags) {
            int flags = connection_flags & (1 << sp::irandom(0, 3));
            while(!flags) {
                flags = connection_flags & (1 << sp::irandom(0, 3));
            }
            flags |= connection_flags & sp::irandom(0, 15);
            for(auto d : DirectionIterator{}) {
                if (flags & (1 << int(d))) {
                    buildSideConnection(d);
                }
            }
        }
        for(auto& c : corridors) {
            std::vector<sp::Vector2i> door_options;
            for(auto p : c) {
                if (isSolid(tiles[p+offset(Direction::Up)]) && isSolid(tiles[p+offset(Direction::Down)]))
                    door_options.push_back(p);
                if (isSolid(tiles[p+offset(Direction::Left)]) && isSolid(tiles[p+offset(Direction::Right)]))
                    door_options.push_back(p);
            }
            if (!door_options.empty()) {
                auto tile = *sp::randomSelect(door_tiles);
                tiles[*sp::randomSelect(door_options)] = tile;
            }
        }
        for(auto p : sp::Rect2i({0, 0}, {tiles.W, tiles.H})) {
            if (tiles[p] == -2)
                tiles[p] = closestRoom(p)->tileset * 21;
        }
        for(auto p : sp::Rect2i({0, 1}, {tiles.W, tiles.H-1})) {
            if (isWall(tiles[p]) && !isWall(tiles[p-sp::Vector2i(0,1)])) {
                tiles[p] += 1;
            }
            if (tiles[p] == -1 && sp::chance(25)) {
                int d = sp::irandom(0, 3*6);
                tiles[p] = ((d % 3) + 4) + (((d / 3) + 6) * 21);
            }
        }

        for(auto& r : rooms) {
            for(int n=0; n<sp::irandom(2, 8); n++) {
                auto p = r.randomPoint();
                if (isFree(p)) {
                    if (sp::chance(50) && depth < 2)
                        enemies.push_back({p, "slime"});
                    else if (sp::chance(50))
                        enemies.push_back({p, "bigslime"});
                    else if (sp::chance(50) || depth < 2)
                        enemies.push_back({p, "skeleton"});
                    else
                        enemies.push_back({p, "golem"});
                }
            }
        }

        for(auto& r : rooms) {
            for(int n=0; n<1; n++) {
                auto p = r.randomPoint();
                if (isFree(p)) {
                    if (sp::chance(70)) {
                        items.push_back({p, ItemType::HealingPotion});
                    } else if (sp::chance(50)) {
                        if (sp::chance(50))
                            items.push_back({p, ItemType::Sword});
                        else
                            items.push_back({p, ItemType::Hammer});
                    } else {
                        if (sp::chance(50))
                            items.push_back({p, ItemType::Armor});
                        else
                            items.push_back({p, ItemType::Shield});
                    }
                }
            }
        }

        for(auto& r : rooms) {
            for(int n=0; n<5; n++) {
                auto p = r.randomPoint();
                if (isFree(p)) {
                    poi.push_back(p);
                }
            }
        }
        while (poi.size() < 3) {
            auto it = sp::randomSelect(enemies);
            poi.push_back(it->pos);
            enemies.erase(it);
        }
    }

    bool isFree(sp::Vector2i p) {
        if (isSolid(tiles[p])) return false;
        for(auto& e : enemies) if (e.pos == p) return false;
        for(auto& i : items) if (i.pos == p) return false;
        for(auto& i : poi) if (i == p) return false;
        return true;
    }

    void buildSideConnection(Direction d) {
        sp::Vector2i target;
        switch(d) {
        case Direction::Up: target = {tiles.W / 2, tiles.H-1}; break;
        case Direction::Down: target = {tiles.W / 2, 0}; break;
        case Direction::Left: target = {0, tiles.H / 2}; break;
        case Direction::Right: target = {tiles.W-1, tiles.H / 2}; break;
        }
        auto room = closestRoom(target);
        auto c = sp::Vector2i(sp::irandom(room->rect.position.x+1, room->rect.position.x+room->rect.size.x-2), sp::irandom(room->rect.position.y+1, room->rect.position.y+room->rect.size.y-2));
        switch(d) {
        case Direction::Up: target.x = c.x; break;
        case Direction::Down: target.x = c.x; break;
        case Direction::Left: target.y = c.y; break;
        case Direction::Right: target.y = c.y; break;
        }
        while(c != target) {
            tiles[c] = -1;
            if (c.x < target.x) c.x++;
            if (c.x > target.x) c.x--;
            if (c.y < target.y) c.y++;
            if (c.y > target.y) c.y--;
        }
        tiles[target] = -1;
    }

    Room* closestRoom(sp::Vector2i p) {
        float best_d = 100000;
        Room* best = &rooms[0];
        for(auto& r : rooms) {
            auto d = sp::Vector2f(r.rect.center() - p);
            if (d.length() < best_d) {
                best_d = d.length();
                best = &r;
            }
        }
        return best;
    }

    bool unconnectedRooms() {
        for(auto& r : rooms) {
            if (!r.connected) return true;
        }
        return false;
    }

    void connectRoom() {
        float best_d = 10000.0f;
        Room* best0 = nullptr;
        Room* best1 = nullptr;
        for(auto& r0 : rooms) {
            for(auto& r1 : rooms) {
                if (r0.connected && !r1.connected) {
                    auto d = sp::Vector2f(r0.rect.center() - r1.rect.center()).length();
                    if (d < best_d) {
                        best_d = d;
                        best0 = &r0;
                        best1 = &r1;
                    }
                }
            }
        }
        if (!best0) {
            best0 = &rooms[sp::irandom(0, rooms.size()-1)];
            best1 = &rooms[sp::irandom(0, rooms.size()-1)];
        }
        if (best0 == best1) return;
        auto start = best0->randomPoint();
        auto end = best1->randomPoint();
        best1->connected = true;
        tiles[start] = -1;
        std::vector<sp::Vector2i> corridor;
        auto delta = end - start;
        if (std::abs(delta.x) < std::abs(delta.y)) {
            auto crossover = sp::irandom(std::min(start.y, end.y), std::max(start.y, end.y));
            while(start!=end) {
                if (start.y == crossover && start.x != end.x) {
                    if (start.x < end.x) start.x++; else start.x--;
                } else {
                    if (start.y < end.y) start.y++; else start.y--;
                }
                tiles[start] = -1;
                corridor.push_back(start);
            }
        } else {
            auto crossover = sp::irandom(std::min(start.x, end.x), std::max(start.x, end.x));
            while(start!=end) {
                if (start.x == crossover && start.y != end.y) {
                    if (start.y < end.y) start.y++; else start.y--;
                } else {
                    if (start.x < end.x) start.x++; else start.x--;
                }
                tiles[start] = -1;
                corridor.push_back(start);
            }
        }
        tiles[end] = -1;
        corridors.push_back(corridor);
    }

    bool tryAddRoom()
    {
        auto size = sp::Vector2i(sp::irandom(5, 8), sp::irandom(5, 8));
        auto pos = sp::Vector2i(sp::irandom(-size.x, tiles.W), sp::irandom(-size.y, tiles.H));
        if (pos.x < 0) pos.x = 0;
        if (pos.y < 0) pos.y = 0;
        if (pos.x + size.x >= int(tiles.W)) pos.x = tiles.W - size.x;
        if (pos.y + size.y >= int(tiles.H)) pos.y = tiles.H - size.y;
        auto rect = sp::Rect2i(pos, size);
        if (hasRoom(rect))
            return false;
        rooms.push_back({rect, sp::irandom(0, 5)});
        return true;
    }

    Room* hasRoom(sp::Rect2i rect) {
        for(auto& r : rooms) {
            if (r.rect.overlaps(rect)) {
                return &r;
            }
        }
        return nullptr;
    }

    Room* hasRoom(sp::Vector2i p) {
        for(auto& r : rooms) {
            if (r.rect.contains(p)) {
                return &r;
            }
        }
        return nullptr;
    }

    std::vector<Room> rooms;
    std::vector<std::vector<sp::Vector2i>> corridors;
    Array2<int, MAP_SIZE, MAP_SIZE> tiles;
    struct Enemy {
        sp::Vector2i pos;
        sp::string type;
    };
    std::vector<Enemy> enemies;
    struct Item {
        sp::Vector2i pos;
        ItemType type;
    };
    std::vector<Item> items;
    std::vector<sp::Vector2i> poi;
};
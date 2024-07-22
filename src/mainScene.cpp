#include "mainScene.h"
#include "ingameMenu.h"
#include "main.h"
#include "mapgen.h"
#include "fov.h"
#include "astar.h"

#include <sp2/graphics/gui/loader.h>
#include <sp2/graphics/mesh/obj.h>
#include <sp2/random.h>
#include <sp2/engine.h>
#include <sp2/scene/camera.h>
#include <sp2/graphics/textureManager.h>
#include <sp2/graphics/meshdata.h>
#include <sp2/scene/tilemap.h>
#include <sp2/tween.h>
#include <sp2/script/environment.h>
#include <sp2/graphics/gui/widget/image.h>
#include <sp2/audio/sound.h>
#include <optional>


class Map;
class Pawn;
class Item;
sp::PList<Map> maps;
class Map : public sp::Node
{
public:
    Map(sp::P<Map> parent, sp::Vector2i pos) : Map(parent->getParent(), parent->depth + 1, parent->getConnectionFlagsFor(pos)) {
        parentmap = parent;
        parent->tiles[pos].submap = this;
        parent->tiles[pos].tile = -1;
        this->pos = pos;

        auto position = parent->tilemap->getPosition2D();
        position.x += parent->scale * pos.x;
        position.y += parent->scale * pos.y;
        shadowmap->setPosition(position);
        tilemap->setPosition(position);
        bgmap->setPosition(position);
    }

    Map(sp::P<sp::Node> parent, int depth, int connection_flags=0);

    sp::Vector2i popPoi() {
        sp2assert(!poi.empty(), "Trying to get a POI but none available.");
        auto it = randomSelect(poi);
        auto p = *it;
        poi.erase(it);
        return p;
    }

    int getConnectionFlagsFor(sp::Vector2i pos) {
        int result = 0;
        for(auto d : DirectionIterator{}) {
            if (!::isSolid(tiles[pos+offset(d)].tile)) {
                result |= 1 << int(d);
            }
        }
        return result;
    }

    std::optional<sp::Vector2i> bestOpenEdgeTile(Direction dir) {
        auto start = sp::Vector2i(0, 0);
        auto move = sp::Vector2i(0, 0);
        switch(dir) {
        case Direction::Up: start = {0, tiles.H-1}; move = {1, 0}; break;
        case Direction::Down: move = {1, 0}; break;
        case Direction::Left: move = {0, 1}; break;
        case Direction::Right: start = {tiles.W-1, 0}; move = {0, 1}; break;
        }
        std::optional<sp::Vector2i> best;
        int best_value = 1000;
        for(int n=0; n<tiles.W; n++) {
            if (!tiles[start].isSolid()) {
                auto value = std::abs(n - tiles.W/2);
                if (value < best_value) {
                    best = start;
                    best_value = value;
                }
            }
            start += move;
        }
        return best;
    }

    int depth = 0;
    double scale = 1.0;
    sp::P<sp::Tilemap> shadowmap;
    sp::P<sp::Tilemap> tilemap;
    sp::P<sp::Tilemap> bgmap;
    struct Tile {
        int tile = -1;
        int seen = -1;
        bool visible = false;
        sp::P<Map> submap;
        sp::P<Pawn> pawn;
        sp::P<Item> item;

        void see() { seen = tile; visible = true; }
        bool isSolid() { return ::isSolid(tile); }
        bool canMoveTo() { return !isSolid() && !pawn; }
    };
    Array2<Tile, MAP_SIZE, MAP_SIZE> tiles;
    sp::P<Map> parentmap;
    sp::Vector2i pos;
    std::vector<sp::Vector2i> poi;
};

void updateFoV();
void actEnemies();
class Pawn : public sp::Node
{
public:
    Pawn(sp::P<sp::Node> parent) : sp::Node(parent) {
    }

    sp::P<Map> map;
    sp::Vector2i pos;

    void onUpdate(float delta) override {
        if (!animations.empty())
        {
            animation_time += delta * animations.size();
            if (animations[0](animation_time))
            {
                animations.erase(animations.begin());
                animation_time = 0.0f;

                if (map->tiles[pos].visible) {
                    render_data.type = sp::RenderData::Type::Normal;
                } else {
                    render_data.type = sp::RenderData::Type::None;
                }
            }
        }
    }

    void setPos(sp::P<Map> m, sp::Vector2i p) {
        if (map) {
            sp2assert(map->tiles[pos].pawn == this, "pawn issue");
            map->tiles[pos].pawn = nullptr;
        }
        map = m;
        pos = p;
        camera_position = map->tilemap->getPosition2D() + (sp::Vector2d(p) + sp::Vector2d(.5, .5)) * map->scale;
        setPosition(camera_position);
        sp2assert(map->tiles[pos].pawn == nullptr, "pawn issue");
        sp2assert(map->tiles[pos].submap == nullptr, "pawn issue");
        map->tiles[p].pawn = this;
        if (map->tiles[pos].visible) {
            render_data.type = sp::RenderData::Type::Normal;
        } else {
            render_data.type = sp::RenderData::Type::None;
        }
        float s = map->scale;
        render_data.scale = {s, s, s};
    }

    bool tryMove(Direction dir) {
        auto target = pos + offset(dir);
        auto target_map = map;
        if (target.x < 0 || target.y < 0 || target.x >= map->tiles.W || target.y >= map->tiles.H) {
            if (map->parentmap) {
                target = map->pos + offset(dir);
                target_map = map->parentmap;
            } else {
                // Tried to move outside of top level map.
                return false;
            }
        }
        if (target_map->tiles[target].submap) {
            auto t = target_map->tiles[target].submap->bestOpenEdgeTile(dir + 2);
            if (!t.has_value()) {
                bumpAnim(dir);
                return false;
            }
            target_map = target_map->tiles[target].submap;
            target = t.value();
        }
        if (!target_map->tiles[target].canMoveTo()) {
            bumpAnim(dir);
            bumpInto(target_map->tiles[target]);
            return false;
        }
        moveTo(target, target_map);
        postMove();
        return true;
    }

    virtual void postMove() {}
    virtual void bumpInto(Map::Tile& tile) {}
    virtual void takeDamage(int amount) {}

    void bumpAnim(Direction dir) {
        auto start = map->tilemap->getPosition2D() + (sp::Vector2d(pos) + sp::Vector2d(.5, .5)) * map->scale;
        auto end = start + sp::Vector2d(offset(dir)) * map->scale * 0.5;
        queue([this, start, end](float time) {
            time *= 8.0f;
            sp::Vector2d p;
            if (time < .5)
                p = sp::Tween<sp::Vector2d>::easeInCubic(time, 0.0, 0.5, start, end);
            else
                p = sp::Tween<sp::Vector2d>::easeOutCubic(time, 0.5, 1.0, end, start);
            setPosition(p + sp::Vector2d(0, std::max(0.0, 0.1 * map->scale * std::sin(time * sp::pi))));
            return time >= 1.0f;
        });
    }

    void moveTo(sp::Vector2i target, sp::P<Map> target_map) {
        if (target_map == map) {
            auto start = map->tilemap->getPosition2D() + (sp::Vector2d(pos) + sp::Vector2d(.5, .5)) * map->scale;
            map->tiles[pos].pawn = nullptr;
            pos = target;
            map->tiles[pos].pawn = this;
            auto end = map->tilemap->getPosition2D() + (sp::Vector2d(pos) + sp::Vector2d(.5, .5)) * map->scale;
            queue([this, start, end](float time) {
                time *= 8.0f;
                camera_position = sp::Tween<sp::Vector2d>::easeInOutCubic(time, 0.0, 1.0, start, end);
                setPosition(camera_position + sp::Vector2d(0, std::max(0.0, 0.1 * map->scale * std::sin(time * sp::pi))));
                return time >= 1.0f;
            });
        } else {
            auto start = map->tilemap->getPosition2D() + (sp::Vector2d(pos) + sp::Vector2d(.5, .5)) * map->scale;
            auto start_scale = map->scale;
            map->tiles[pos].pawn = nullptr;
            map = target_map;
            pos = target;
            map->tiles[pos].pawn = this;
            auto end = map->tilemap->getPosition2D() + (sp::Vector2d(pos) + sp::Vector2d(.5, .5)) * map->scale;
            auto end_scale = map->scale;
            queue([this, start, end, start_scale, end_scale](float time) {
                time *= 2.0f;
                camera_position = sp::Tween<sp::Vector2d>::easeInOutCubic(time, 0.0, 1.0, start, end);
                setPosition(camera_position + sp::Vector2d(0, std::max(0.0, 0.1 * map->scale * std::sin(time * sp::pi))));
                float s = sp::Tween<double>::easeInOutCubic(time, 0.0, 1.0, start_scale, end_scale);
                render_data.scale = {s, s, s};
                return time >= 1.0f;
            });
        }
    }

    void queue(const std::function<bool(float)>& f)
    {
        animations.emplace_back(f);
    }

    sp::Vector2d camera_position;
    float animation_time = 0.0f;
    std::vector<std::function<bool(float)>> animations;
};

class Item : public sp::Node
{
public:
    Item(sp::P<sp::Node> parent, ItemType type) : sp::Node(parent), type(type) {
        int tile_idx = itemTypeToTileIdx(type);
        render_data.mesh = sp::MeshData::createQuad({1, 1},
            {(tile_idx % 8) / 8.0f, (tile_idx / 8) / 22.0f},
            {(tile_idx % 8 + 1) / 8.0f, (tile_idx / 8 + 1) / 22.0f});
        render_data.shader = sp::Shader::get("internal:basic.shader");
        render_data.type = sp::RenderData::Type::Normal;
        render_data.texture = sp::texture_manager.get("32rogues/items.png");
        render_data.order = -1;
    }

    void setPos(sp::P<Map> m, sp::Vector2i p) {
        if (map) {
            sp2assert(map->tiles[pos].item == this, "item issue");
            map->tiles[pos].item = nullptr;
        }
        map = m;
        pos = p;
        setPosition(map->tilemap->getPosition2D() + (sp::Vector2d(p) + sp::Vector2d(.5, .5)) * map->scale);
        sp2assert(map->tiles[pos].item == nullptr, "item issue");
        sp2assert(map->tiles[pos].submap == nullptr, "item issue");
        map->tiles[p].item = this;
        if (map->tiles[pos].visible) {
            render_data.type = sp::RenderData::Type::Normal;
        } else {
            render_data.type = sp::RenderData::Type::None;
        }
        float s = map->scale;
        render_data.scale = {s, s, s};
    }

    sp::P<Map> map;
    sp::Vector2i pos;
    ItemType type;
};

class Effect;
sp::PList<Effect> effect_list;
class Effect : public sp::Node
{
public:
    Effect(sp::P<sp::Node> parent) : sp::Node(parent) {
        effect_list.add(this);
        int tile_idx = 22 * 21 + sp::irandom(0, 1);
        render_data.mesh = sp::MeshData::createQuad({1, 1},
            {(tile_idx % 21) / 21.0f, (tile_idx / 21) / 24.0f},
            {(tile_idx % 21 + 1) / 21.0f, (tile_idx / 21 + 1) / 24.0f});
        render_data.shader = sp::Shader::get("internal:basic.shader");
        render_data.type = sp::RenderData::Type::Transparent;
        render_data.texture = sp::texture_manager.get("32rogues/tiles.png");
        render_data.order = 10;
        render_data.color.a = 0.8;
    }
};

class Player : public Pawn
{
public:
    Player(sp::P<sp::Node> parent) : Pawn(parent) {
        auto tile_idx = sp::irandom(0, 24);
        if (tile_idx == 4 + 5 * 3) tile_idx = sp::irandom(0, 15);
        render_data.mesh = sp::MeshData::createQuad({1, 1},
            {(tile_idx % 5) * 0.2f, (tile_idx / 5) * 0.2f},
            {(tile_idx % 5) * 0.2f + 0.2f, (tile_idx / 5) * 0.2f + 0.2f});
        render_data.shader = sp::Shader::get("internal:basic.shader");
        render_data.type = sp::RenderData::Type::Normal;
        render_data.texture = sp::texture_manager.get("32rogues/rogues.png");
    }

    void onFixedUpdate() override 
    {
        if (health <= 0) return;
        if (controller.left.getDown()) { preMove(); tryMove(Direction::Left); }
        if (controller.right.getDown()) { preMove(); tryMove(Direction::Right); }
        if (controller.up.getDown()) { preMove(); tryMove(Direction::Up); }
        if (controller.down.getDown()) { preMove(); tryMove(Direction::Down); }
    }

    void preMove() {
        for(auto e : effect_list)
            e.destroy();
    }

    void onUpdate(float delta) override {
        Pawn::onUpdate(delta);

        getScene()->getCamera()->setPosition(camera_position);
        getScene()->getCamera()->setOrtographic({5*render_data.scale.x, 5*render_data.scale.x});
    }

    void postMove() override {
        if (map->tiles[pos].item) {
            switch(map->tiles[pos].item->type) {
            case ItemType::Dagger:
            case ItemType::Sword:
            case ItemType::Hammer:
                weapon = map->tiles[pos].item->type;
                weapon_durability = weapon_durability_max = weaponDurability(weapon);
                sp::audio::Sound::play("sfx/pickup.wav");
                break;
            case ItemType::Shield: has_shield = true; sp::audio::Sound::play("sfx/pickup.wav"); break;
            case ItemType::Armor: has_armor = true; sp::audio::Sound::play("sfx/pickup.wav"); break;
            case ItemType::HealingPotion:
                health = std::min(max_health, health + 4);
                sp::audio::Sound::play("sfx/heal.wav");
                break;
            }
            map->tiles[pos].item.destroy();
        } else {
            sp::audio::Sound::play("sfx/step.wav");
        }
        actEnemies();
        updateFoV();
    }

    void takeDamage(int amount) override
    {
        if (has_shield) { has_shield = false; sp::audio::Sound::play("sfx/armorHit.wav"); return; }
        if (has_armor) { has_armor = false; sp::audio::Sound::play("sfx/armorHit.wav"); return; }
        health = std::max(0, health - amount);
        if (health <= 0) {
            map->tiles[pos].tile = 22 * 21 + sp::irandom(0, 1);
            render_data.mesh = nullptr;
            sp::audio::Sound::play("sfx/death.wav");
        } else {
            sp::audio::Sound::play("sfx/hitHurt2.wav");
        }
        auto e = new Effect(this);
        e->render_data.scale = render_data.scale;
    }

    void bumpInto(Map::Tile& tile) override {
        if (isDoor(tile.tile)) {
            tile.tile = -1;
        }
        if (tile.pawn) {
            sp::audio::Sound::play("sfx/hitHurt.wav");
            tile.pawn->takeDamage(weaponDamage(weapon));
            if (weapon_durability_max) {
                weapon_durability -= 1;
                if (weapon_durability <= 0) {
                    weapon_durability_max = weapon_durability = 0;
                    weapon = ItemType::Dagger;
                }
            }
        }
        actEnemies();
        updateFoV();
    }

    int max_health = 3 * 4;
    int health = 3 * 4;
    ItemType weapon = ItemType::Dagger;
    int weapon_durability = 0;
    int weapon_durability_max = 0;
    bool has_armor = false;
    bool has_shield = false;
};
sp::P<Player> player;

int luaYield(lua_State* L) { return lua_yield(L, 0); }
void luaMove(Direction d);
int luaPathFind(lua_State* L);
bool luaIsVisible();
void luaSetHealth(int amount);
int luaGetHealth();
void luaSetSprite(int idx);
void luaSetCanBreakWalls(bool enabled);
namespace sp::script { static inline Direction convertFromLua(lua_State* L, typeIdentifier<Direction>, int index) {
    int n = lua_tointeger(L, index);
    return static_cast<Direction>(n & 3);
}
}


class Enemy;
sp::PList<Enemy> enemies;
sp::P<Enemy> current_enemy;
class Enemy : public Pawn
{
public:
    Enemy(sp::P<sp::Node> parent, sp::string type) : Pawn(parent) {
        auto tile_idx = 8;
        render_data.mesh = sp::MeshData::createQuad({1, 1},
            {(tile_idx % 7) * 1.0f / 7.0f, (tile_idx / 7) * 1.0f / 8.0f},
            {(tile_idx % 7) * 1.0f / 7.0f + 1.0f / 7.0f, (tile_idx / 7) * 1.0f / 8.0f + 1.0f / 8.0f});
        render_data.shader = sp::Shader::get("internal:basic.shader");
        render_data.type = sp::RenderData::Type::Normal;
        render_data.texture = sp::texture_manager.get("32rogues/monsters.png");
        enemies.add(this);

        current_enemy = this;
        env.setGlobal("yield", luaYield);
        env.setGlobal("move", luaMove);
        env.setGlobal("pathFind", luaPathFind);
        env.setGlobal("isVisible", luaIsVisible);
        env.setGlobal("setHealth", luaSetHealth);
        env.setGlobal("getHealth", luaGetHealth);
        env.setGlobal("setSprite", luaSetSprite);
        env.setGlobal("setCanBreakWalls", luaSetCanBreakWalls);
        env.setGlobal("irandom", sp::irandom);
        env.setGlobal("random", sp::random);
        env.load("enemy/"+type+".lua");
    }

    void takeDamage(int amount) override
    {
        queue([](float time) {
            time *= 8.0f;
            return time >= 1.0f;
        });

        health -= amount;
        if (health <= 0) {
            map->tiles[pos].tile = 22 * 21 + sp::irandom(0, 1);
            delete this;
        } else {
            auto e = new Effect(this);
            e->render_data.scale = render_data.scale;
        }
    }

    void act() {
        current_enemy = this;
        if (!act_coroutine) {
            act_coroutine = env.callCoroutine("act").value();
        } else {
            if (!act_coroutine->resume().value())
                act_coroutine = nullptr;
        }
    }

    void bumpInto(Map::Tile& tile) override {
        if (tile.pawn == player) {
            tile.pawn->takeDamage(2);
        }
        if (tile.isSolid() && can_break_walls) {
            tile.tile = -1;
        }
    }

    sp::script::Environment env;
    sp::script::CoroutinePtr act_coroutine;
    int health = 2;
    bool can_break_walls = false;
};
void luaMove(Direction d)
{
    current_enemy->tryMove(d);
}

struct PathNode {
    Map* map;
    sp::Vector2i pos;

    bool operator==(const PathNode& other) const { return map == other.map && pos == other.pos; }
};
namespace std { template <> struct hash<PathNode> { size_t operator()(const PathNode& p) const {
    size_t h1 = hash<void*>()(p.map);
    size_t h2 = hash<sp::Vector2i>()(p.pos);
    return h1 ^ (h2 << 1);
}};}

std::vector<std::pair<PathNode, float>> getNeighbors(PathNode n)
{
    std::vector<std::pair<PathNode, float>> result;
    for(auto d : DirectionIterator{}) {
        auto p = n.pos + offset(d);
        if (p.x < 0 || p.y < 0 || p.x >= n.map->tiles.W || p.y >= n.map->tiles.H) {
            if (n.map->parentmap) {
                p = n.map->pos + offset(d);
                if (!n.map->parentmap->tiles[p].isSolid()) {
                    result.push_back({{*n.map->parentmap, p}, n.map->parentmap->tiles[p].pawn ? 1.2f : 1.0f});
                } else if (current_enemy->can_break_walls) {
                    result.push_back({{*n.map->parentmap, p}, 2.0f});
                }
            }
        } else if (n.map->tiles[p].submap) {
            auto t = n.map->tiles[p].submap->bestOpenEdgeTile(d + 2);
            if (t.has_value()) {
                result.push_back({{*n.map->tiles[p].submap, t.value()}, n.map->tiles[p].submap->tiles[t.value()].pawn ? 1.2f : 1.0f});
            }
        } else if (!n.map->tiles[p].isSolid()) {
            result.push_back({{n.map, p}, n.map->tiles[p].pawn ? 1.2f : 1.0f});
        } else if (current_enemy->can_break_walls) {
            result.push_back({{n.map, p}, 2.0f});
        }
    }
    return result;
}

float getDistance(PathNode a, PathNode b) {
    return sp::Vector2f(b.pos - a.pos).length();
}

int luaPathFind(lua_State* L) {
    auto path = AStar<PathNode>({*current_enemy->map, current_enemy->pos}, {*player->map, player->pos}, &getNeighbors, &getDistance);
    if (!path.empty()) {
        Direction res = Direction::Up;
        if (path[0].map == *current_enemy->map) {
            res = toDirection(path[0].pos - current_enemy->pos);
        } else {
            for(auto d : DirectionIterator{}) {
                auto p = current_enemy->pos + offset(d);
                if (p.x < 0 || p.y < 0 || p.x >= current_enemy->map->tiles.W || p.y >= current_enemy->map->tiles.H) {
                    if (current_enemy->map->parentmap == path[0].map) {
                        res = d;
                        break;
                    }
                } else if (current_enemy->map->tiles[p].submap == path[0].map) {
                    res = d;
                    break;
                }
            }
        }
        lua_pushnumber(L, static_cast<int>(res));
    } else {
        lua_pushnil(L);
    }
    return 1;
}
bool luaIsVisible() { return current_enemy->map->tiles[current_enemy->pos].visible; }
void luaSetHealth(int amount) { current_enemy->health = amount; }
int luaGetHealth() { return current_enemy->health; }
void luaSetSprite(int tile_idx)
{
    current_enemy->render_data.mesh = sp::MeshData::createQuad({1, 1},
        {(tile_idx % 7) * 1.0f / 7.0f, (tile_idx / 7) * 1.0f / 8.0f},
        {(tile_idx % 7) * 1.0f / 7.0f + 1.0f / 7.0f, (tile_idx / 7) * 1.0f / 8.0f + 1.0f / 8.0f});
}
void luaSetCanBreakWalls(bool enabled) { current_enemy->can_break_walls = enabled; }

void actEnemies() {
    for(auto e : enemies) {
        e->act();
    }
}

void updateFoV() {
    for(auto map : maps) {
        for(auto& t : map->tiles) {
            t.visible = false;
        }
    }

    auto map = player->map;
    VisitFOV(player->pos, 10, [map](Direction dir, sp::Vector2i p) -> bool {
        if (p.x < 0) {
            if (map->parentmap) map->parentmap->tiles[map->pos+sp::Vector2i(-1, 0)].see();
            return false;
        }
        if (p.x >= map->tiles.W) {
            if (map->parentmap) map->parentmap->tiles[map->pos+sp::Vector2i(1, 0)].see();
            return false;
        }
        if (p.y < 0) {
            if (map->parentmap) map->parentmap->tiles[map->pos+sp::Vector2i(0, -1)].see();
            return false;
        }
        if (p.y >= map->tiles.H) {
            if (map->parentmap) map->parentmap->tiles[map->pos+sp::Vector2i(0, 1)].see();
            return false;
        }
        if (map->tiles[p].submap) {
            switch(dir) {
            case Direction::Down: for(int n=0; n<map->tiles[p].submap->tiles.W; n++) map->tiles[p].submap->tiles[{n, map->tiles[p].submap->tiles.H-1}].see(); break;
            case Direction::Up: for(int n=0; n<map->tiles[p].submap->tiles.W; n++) map->tiles[p].submap->tiles[{n, 0}].see(); break;
            case Direction::Left: for(int n=0; n<map->tiles[p].submap->tiles.H; n++) map->tiles[p].submap->tiles[{map->tiles[p].submap->tiles.W-1, n}].see(); break;
            case Direction::Right: for(int n=0; n<map->tiles[p].submap->tiles.H; n++) map->tiles[p].submap->tiles[{0, n}].see(); break;
            }
            return false;
        }
        map->tiles[p].see();
        if (isSolid(map->tiles[p].tile)) return false;
        return true;
    });
    
    for(auto map : maps) {
        for(auto p : sp::Rect2i({0, 0}, {map->tiles.W, map->tiles.H})) {
            map->tilemap->setTile(p, -1);
            map->bgmap->setTile(p, -1);
            map->shadowmap->setTile(p, -1);
            if (map->tiles[p].visible) {
                map->tilemap->setTile(p, map->tiles[p].tile);
                map->bgmap->setTile(p, 21*6);
            }else if (map->tiles[p].seen > -1) {
                map->shadowmap->setTile(p, map->tiles[p].seen);
            }
            if (map->tiles[p].pawn && map->tiles[p].pawn->animations.empty()) {
                if (map->tiles[p].visible) {
                    map->tiles[p].pawn->render_data.type = sp::RenderData::Type::Normal;
                } else {
                    map->tiles[p].pawn->render_data.type = sp::RenderData::Type::None;
                }
            }
            if (map->tiles[p].item) {
                if (map->tiles[p].visible) {
                    map->tiles[p].item->render_data.type = sp::RenderData::Type::Normal;
                } else {
                    map->tiles[p].item->render_data.type = sp::RenderData::Type::None;
                }
            }
        }
    }
}

Map::Map(sp::P<sp::Node> parent, int depth, int connection_flags) : sp::Node(parent), depth(depth) {
    maps.add(this);
    for(int n=0; n<depth; n++)
        scale /= MAP_SIZE;
    shadowmap = new sp::Tilemap(this, "32rogues/tiles.png", scale, scale, 21, 24);
    tilemap = new sp::Tilemap(this, "32rogues/tiles.png", scale, scale, 21, 24);
    bgmap = new sp::Tilemap(this, "32rogues/tiles.png", scale, scale, 21, 24);
    MapGen mg{connection_flags, depth};
    for(auto p : sp::Rect2i({0, 0}, {tiles.W, tiles.H}))
        tiles[p].tile = mg.tiles[p];
    poi = mg.poi;
    shadowmap->render_data.shader = sp::Shader::get("gray.shader");
    
    shadowmap->render_data.order = -1000 + 2 * depth;
    tilemap->render_data.order = -1000 + 2 * depth;
    bgmap->render_data.order = -1001 + 2 * depth;

    for(auto& ei : mg.enemies) {
        auto e = new Enemy(getParent(), ei.type);
        e->setPos(this, ei.pos);
    }
    for(auto& ii : mg.items) {
        auto i = new Item(getParent(), ii.type);
        i->setPos(this, ii.pos);
    }
}

Scene::Scene()
: sp::Scene("MAIN")
{
    sp::Scene::get("INGAME_MENU")->enable();

    auto camera = new sp::Camera(getRoot());
    camera->setOrtographic({5, 5});
    camera->setPosition({10, 10});
    setDefaultCamera(camera);

    player = new Player(getRoot());
    auto map = new Map(getRoot(), 0);
    player->setPos(map, map->popPoi());
    updateFoV();
    for(auto e : enemies)
        if (e->map->tiles[e->pos].visible)
            e.destroy();
    while(maps.size() < 8) {
        auto source = *sp::randomSelect(maps);
        if (source->depth < 4)
            new Map(source, source->popPoi());
    }
    for(auto e : enemies) // Workaround for initial sprite positions being wrong.
        e->setPos(e->map, e->pos);
    for(auto m : maps) // Workaround for initial sprite positions being wrong.
        for(auto& t : m->tiles)
            if (t.item)
                t.item->setPos(t.item->map, t.item->pos);
    updateFoV();
    LOG(Debug, maps.size(), enemies.size());

    hud = sp::gui::Loader::load("gui/hud.gui", "HUD");

    int tile_idx = itemTypeToTileIdx(ItemType::Armor);
    sp::P<sp::gui::Image> image = hud->getWidgetWithID("ARMOR")->getWidgetWithID("ICON");
    image->setUV({{(tile_idx % 8) / 8.0f, (tile_idx / 8) / 22.0f},
            {1.0f / 8.0f, 1.0f / 22.0f}});
    tile_idx = itemTypeToTileIdx(ItemType::Shield);
    image = hud->getWidgetWithID("SHIELD")->getWidgetWithID("ICON");
    image->setUV({{(tile_idx % 8) / 8.0f, (tile_idx / 8) / 22.0f},
            {1.0f / 8.0f, 1.0f / 22.0f}});
}

Scene::~Scene()
{
    sp::Scene::get("INGAME_MENU")->disable();
    hud.destroy();
}

void Scene::onUpdate(float delta)
{
    sp::P<sp::gui::Widget> hud_health = hud->getWidgetWithID("HEALTH");
    if (hud_health->getChildren().size() != player->max_health / 4)
    {
        while(hud_health->getChildren().size() > player->max_health / 4)
            (*hud_health->getChildren().begin()).destroy();
        while(hud_health->getChildren().size() < player->max_health / 4)
        {
            sp::P<sp::gui::Image> heart = new sp::gui::Image(hud_health);
            heart->setAttribute("size", "32, 32");
        }
    }
    int index = 1;
    for(auto widget : hud_health->getChildren())
    {
        sp::P<sp::gui::Widget> heart = widget;
        if (index * 4 <= player->health)
            heart->setAttribute("image", "gui/icons/heart_full.png");
        else if (index * 4 <= player->health + 3)
            heart->setAttribute("image", "gui/icons/heart_half.png");
        else
            heart->setAttribute("image", "gui/icons/heart_empty.png");
        index += 1;
    }

    int tile_idx = itemTypeToTileIdx(player->weapon);
    sp::P<sp::gui::Image> weapon_icon = hud->getWidgetWithID("WEAPON")->getWidgetWithID("ICON");
    weapon_icon->setUV({{(tile_idx % 8) / 8.0f, (tile_idx / 8) / 22.0f},
            {1.0f / 8.0f, 1.0f / 22.0f}});
    auto bar = hud->getWidgetWithID("WEAPON")->getWidgetWithID("BAR");
    bar->setVisible(player->weapon_durability_max > 0);
    if (player->weapon_durability_max) {
        bar->setAttribute("range", "0, " + sp::string(player->weapon_durability_max));
        bar->setAttribute("value", sp::string(player->weapon_durability));
    }

    hud->getWidgetWithID("ARMOR")->setVisible(player->has_armor);
    hud->getWidgetWithID("SHIELD")->setVisible(player->has_shield);

    hud->getWidgetWithID("QUEST")->setAttribute("caption", "Monsters: " + sp::string(enemies.size()));
}

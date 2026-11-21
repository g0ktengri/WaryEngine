#ifndef REGISTRY_H
#define REGISTRY_H
#include <string>
#include <vector>
#include <deque>
#include "SparseSet.h"
#include "Components.h"

struct EntitySnapshot {
    std::string name;
    bool active;
    Transform transform;
    Sprite sprite;
    Physics physics;
};

class Registry {
public:
    EntityID Create(const std::string& name);
    void Destroy(EntityID e);
    const std::vector<EntityID>& Entities() const { return liveEntities; }

    std::string& Name(EntityID e) { return names[e]; }
    bool& Active(EntityID e) { return actives[e]; }

    std::vector<EntitySnapshot> Snapshot() const;
    void Restore(const std::vector<EntitySnapshot>& snapshot);

    SparseSet<Transform> transforms;
    SparseSet<Sprite> sprites;
    SparseSet<Physics> physics;

private:
    std::vector<EntityID> liveEntities; // insertion order
    std::vector<std::string> names;     // parallel-indexed by EntityID, grown as needed
    std::deque<bool> actives;           // parallel-indexed by EntityID, grown as needed
                                         // (deque, not vector<bool>: vector<bool> is
                                         // bit-packed and operator[] returns a proxy,
                                         // not a real bool&, which Active() below needs)
    EntityID nextId = 0;
};
#endif

#include "Registry.h"
#include <algorithm>

EntityID Registry::Create(const std::string& name) {
    EntityID id = nextId++;
    if (id >= names.size()) {
        names.resize(id + 1);
        actives.resize(id + 1);
    }
    names[id] = name;
    actives[id] = true;
    liveEntities.push_back(id);
    return id;
}

void Registry::Destroy(EntityID e) {
    auto it = std::find(liveEntities.begin(), liveEntities.end(), e);
    if (it != liveEntities.end()) liveEntities.erase(it);
    transforms.Remove(e);
    sprites.Remove(e);
    physics.Remove(e);
}

std::vector<EntitySnapshot> Registry::Snapshot() const {
    std::vector<EntitySnapshot> result;
    result.reserve(liveEntities.size());
    for (EntityID id : liveEntities) {
        EntitySnapshot snap;
        snap.name = names[id];
        snap.active = actives[id];
        snap.transform = transforms.Get(id);
        snap.sprite = sprites.Get(id);
        snap.physics = physics.Get(id);
        result.push_back(std::move(snap));
    }
    return result;
}

void Registry::Restore(const std::vector<EntitySnapshot>& snapshot) {
    transforms = SparseSet<Transform>();
    sprites = SparseSet<Sprite>();
    physics = SparseSet<Physics>();
    liveEntities.clear();
    names.clear();
    actives.clear();
    // nextId is intentionally NOT reset here — see Registry.h / task brief.

    for (const EntitySnapshot& snap : snapshot) {
        EntityID id = Create(snap.name);
        Active(id) = snap.active;
        transforms.Add(id, snap.transform);
        sprites.Add(id, snap.sprite);
        physics.Add(id, snap.physics);
    }
}

#ifndef SPARSE_SET_H
#define SPARSE_SET_H
#include <vector>
#include <cstdint>
#include <utility>

using EntityID = uint32_t;

template <typename T>
class SparseSet {
public:
    void Add(EntityID e, T value) {
        if (Has(e)) { dense[(size_t)sparse[e]] = std::move(value); return; }
        if (e >= sparse.size()) sparse.resize(e + 1, -1);
        sparse[e] = (int)dense.size();
        denseIds.push_back(e);
        dense.push_back(std::move(value));
    }
    void Remove(EntityID e) {
        if (!Has(e)) return;
        size_t idx = (size_t)sparse[e];
        denseIds.erase(denseIds.begin() + idx);
        dense.erase(dense.begin() + idx);
        sparse[e] = -1;
        for (size_t i = idx; i < denseIds.size(); i++) sparse[denseIds[i]] = (int)i;
    }
    bool Has(EntityID e) const { return e < sparse.size() && sparse[e] != -1; }
    T& Get(EntityID e) { return dense[(size_t)sparse[e]]; }
    const T& Get(EntityID e) const { return dense[(size_t)sparse[e]]; }
    const std::vector<EntityID>& Ids() const { return denseIds; }
    std::vector<T>& Dense() { return dense; }
private:
    std::vector<int> sparse;
    std::vector<EntityID> denseIds;
    std::vector<T> dense;
};
#endif

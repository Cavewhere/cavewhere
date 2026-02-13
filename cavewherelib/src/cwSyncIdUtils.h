#pragma once

#include <QHash>
#include <QSet>
#include <QUuid>

#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace cwSyncIdUtils {

template<typename Container, typename IdAccessor>
std::optional<std::vector<QUuid>> collectOrderedUniqueIds(const Container& items, IdAccessor idAccessor)
{
    std::vector<QUuid> orderedIds;
    orderedIds.reserve(static_cast<size_t>(items.size()));
    QSet<QUuid> seenIds;

    for (const auto& item : items) {
        const QUuid id = idAccessor(item);
        if (id.isNull() || seenIds.contains(id)) {
            return std::nullopt;
        }

        seenIds.insert(id);
        orderedIds.push_back(id);
    }

    return orderedIds;
}

inline bool haveSameIdsIgnoringOrder(const std::vector<QUuid>& currentIds, const std::vector<QUuid>& loadedIds)
{
    if (currentIds.size() != loadedIds.size()) {
        return false;
    }

    QSet<QUuid> currentIdSet(currentIds.begin(), currentIds.end());
    QSet<QUuid> loadedIdSet(loadedIds.begin(), loadedIds.end());
    return currentIdSet == loadedIdSet;
}

template<typename Container, typename PtrAccessor, typename IdAccessor>
auto buildUniqueIdPointerMap(const Container& items, PtrAccessor ptrAccessor, IdAccessor idAccessor)
    -> std::optional<QHash<QUuid, std::decay_t<decltype(ptrAccessor(*std::begin(std::declval<const Container&>())))>>>
{
    using ItemRef = decltype(*std::begin(std::declval<const Container&>()));
    using Ptr = std::decay_t<decltype(ptrAccessor(std::declval<ItemRef>()))>;

    static_assert(std::is_pointer_v<Ptr>, "buildUniqueIdPointerMap expects ptrAccessor to return a pointer type");

    QHash<QUuid, Ptr> byId;
    byId.reserve(items.size());

    for (const auto& item : items) {
        Ptr ptr = ptrAccessor(item);
        if (ptr == nullptr) {
            return std::nullopt;
        }

        const QUuid id = idAccessor(ptr);
        if (id.isNull() || byId.contains(id)) {
            return std::nullopt;
        }

        byId.insert(id, ptr);
    }

    return byId;
}

} // namespace cwSyncIdUtils

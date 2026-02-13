#pragma once

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QUuid>

#include "Monad/Result.h"

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

template<typename Container, typename IdAccessor>
auto buildUniqueIdValueMap(const Container& items, IdAccessor idAccessor)
    -> std::optional<QHash<QUuid, std::decay_t<decltype(*std::begin(std::declval<const Container&>()))>>>
{
    using Value = std::decay_t<decltype(*std::begin(std::declval<const Container&>()))>;

    QHash<QUuid, Value> byId;
    byId.reserve(items.size());

    for (const auto& item : items) {
        const QUuid id = idAccessor(item);
        if (id.isNull() || byId.contains(id)) {
            return std::nullopt;
        }
        byId.insert(id, item);
    }

    return byId;
}

template<typename T, typename MergeSharedFn, typename PreferredOrderFn>
Monad::Result<QList<T>> buildMergedOrderedList(const QList<T>& currentValues,
                                               const QList<T>& loadedValues,
                                               const std::optional<QList<T>>& baseValues,
                                               MergeSharedFn mergeSharedValue,
                                               PreferredOrderFn preferredOrder,
                                               const QString& contextLabel)
{
    const auto currentById = buildUniqueIdValueMap(
        currentValues,
        [](const T& value) { return value.id(); });
    const auto loadedById = buildUniqueIdValueMap(
        loadedValues,
        [](const T& value) { return value.id(); });
    if (!currentById.has_value() || !loadedById.has_value()) {
        return Monad::Result<QList<T>>(QStringLiteral("Ambiguous ids in %1.").arg(contextLabel));
    }

    std::optional<QHash<QUuid, T>> baseById;
    if (baseValues.has_value()) {
        baseById = buildUniqueIdValueMap(
            *baseValues,
            [](const T& value) { return value.id(); });
        if (!baseById.has_value()) {
            return Monad::Result<QList<T>>(QStringLiteral("Ambiguous base ids in %1.").arg(contextLabel));
        }
    }

    const auto currentIds = collectOrderedUniqueIds(
        currentValues,
        [](const T& value) { return value.id(); });
    const auto loadedIds = collectOrderedUniqueIds(
        loadedValues,
        [](const T& value) { return value.id(); });
    if (!currentIds.has_value() || !loadedIds.has_value()) {
        return Monad::Result<QList<T>>(QStringLiteral("Ambiguous ordered ids in %1.").arg(contextLabel));
    }

    std::optional<std::vector<QUuid>> baseIds;
    if (baseValues.has_value()) {
        baseIds = collectOrderedUniqueIds(
            *baseValues,
            [](const T& value) { return value.id(); });
        if (!baseIds.has_value()) {
            return Monad::Result<QList<T>>(QStringLiteral("Ambiguous ordered base ids in %1.").arg(contextLabel));
        }
    }

    QHash<QUuid, T> mergedById;
    mergedById.reserve(currentById->size() + loadedById->size());

    QSet<QUuid> allIds;
    for (auto it = currentById->cbegin(); it != currentById->cend(); ++it) {
        allIds.insert(it.key());
    }
    for (auto it = loadedById->cbegin(); it != loadedById->cend(); ++it) {
        allIds.insert(it.key());
    }

    for (const QUuid& id : std::as_const(allIds)) {
        const auto currentIt = currentById->constFind(id);
        const auto loadedIt = loadedById->constFind(id);
        const bool hasCurrent = currentIt != currentById->cend();
        const bool hasLoaded = loadedIt != loadedById->cend();
        const bool hasBase = baseById.has_value() && baseById->contains(id);

        if (hasCurrent && hasLoaded) {
            mergedById.insert(id,
                              mergeSharedValue(*currentIt,
                                               *loadedIt,
                                               hasBase ? std::optional<T>(baseById->value(id)) : std::nullopt));
            continue;
        }

        if (hasCurrent) {
            mergedById.insert(id, *currentIt);
            continue;
        }

        if (hasLoaded && !hasBase) {
            mergedById.insert(id, *loadedIt);
        }
    }

    const std::vector<QUuid> preferredIds = preferredOrder(*currentIds, *loadedIds, baseIds);

    QList<QUuid> mergedOrder;
    mergedOrder.reserve(mergedById.size());
    QSet<QUuid> seenIds;
    for (const QUuid& id : preferredIds) {
        if (mergedById.contains(id) && !seenIds.contains(id)) {
            mergedOrder.append(id);
            seenIds.insert(id);
        }
    }
    for (const QUuid& id : *loadedIds) {
        if (mergedById.contains(id) && !seenIds.contains(id)) {
            mergedOrder.append(id);
            seenIds.insert(id);
        }
    }
    for (const QUuid& id : *currentIds) {
        if (mergedById.contains(id) && !seenIds.contains(id)) {
            mergedOrder.append(id);
            seenIds.insert(id);
        }
    }

    if (mergedOrder.size() != mergedById.size()) {
        return Monad::Result<QList<T>>(
            QStringLiteral("Failed to build deterministic merged order for %1.").arg(contextLabel));
    }

    QList<T> mergedValues;
    mergedValues.reserve(mergedOrder.size());
    for (const QUuid& id : std::as_const(mergedOrder)) {
        mergedValues.append(mergedById.value(id));
    }

    return Monad::Result<QList<T>>(mergedValues);
}

} // namespace cwSyncIdUtils

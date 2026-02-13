#pragma once

#include <optional>

namespace cwSyncMergeApplyUtils {

template<typename T, typename IsEqualFn>
T chooseBundleValue(const T& currentValue,
                    const T& loadedValue,
                    const std::optional<T>& baseValue,
                    IsEqualFn isEqual)
{
    if (!baseValue.has_value()) {
        return loadedValue;
    }

    const bool currentMatchesBase = isEqual(currentValue, *baseValue);
    const bool loadedMatchesBase = isEqual(loadedValue, *baseValue);

    if (currentMatchesBase && !loadedMatchesBase) {
        return loadedValue;
    }

    return currentValue;
}

} // namespace cwSyncMergeApplyUtils


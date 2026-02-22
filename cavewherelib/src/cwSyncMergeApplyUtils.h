#pragma once

#include <optional>

namespace cwSyncMergeApplyUtils {

enum class ApplyMode {
    ThreeWayMerge,
    LoadedWins
};

template<typename T, typename IsEqualFn>
T chooseBundleValue(const T& currentValue,
                    const T& loadedValue,
                    const std::optional<T>& baseValue,
                    IsEqualFn isEqual,
                    ApplyMode applyMode = ApplyMode::ThreeWayMerge)
{
    if (applyMode == ApplyMode::LoadedWins) {
        return loadedValue;
    }

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

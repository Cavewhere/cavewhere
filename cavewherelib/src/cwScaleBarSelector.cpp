/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwScaleBarSelector.h"

//Std includes
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {
    // The "nice" mantissas a scale bar rounds to (1/2/5 × 10ⁿ), and the exponent
    // sweep that covers everything from a fraction of the unit up to very large
    // scales.
    constexpr std::array<double, 3> kNiceMantissas = {1.0, 2.0, 5.0};
    constexpr int kMinExponent = -2;
    constexpr int kMaxExponent = 6;

    std::vector<cwScaleBarSelector::Candidate> buildCandidates(cwUnits::UnitSystem system)
    {
        const std::array<cwUnits::LengthUnit, 2> units = {
            cwUnits::smallLengthUnit(system),
            cwUnits::largeLengthUnit(system)
        };

        std::vector<cwScaleBarSelector::Candidate> candidates;
        for (cwUnits::LengthUnit unit : units) {
            for (int exponent = kMinExponent; exponent <= kMaxExponent; ++exponent) {
                for (double mantissa : kNiceMantissas) {
                    const double value = mantissa * std::pow(10.0, exponent);
                    const double meters = cwUnits::convert(value, unit, cwUnits::Meters);
                    // Keep a candidate only in the unit its own magnitude calls
                    // for, so each label is round in the unit it is shown in and
                    // the small/large crossover has a single owner.
                    if (cwUnits::magnitudeUnit(meters, system) != unit) {
                        continue;
                    }
                    candidates.push_back({meters, value, unit});
                }
            }
        }

        std::sort(candidates.begin(), candidates.end(),
                  [](const cwScaleBarSelector::Candidate& a, const cwScaleBarSelector::Candidate& b) {
                      return a.meters < b.meters;
                  });
        return candidates;
    }
}

const std::vector<cwScaleBarSelector::Candidate>& cwScaleBarSelector::niceCandidates(cwUnits::UnitSystem system)
{
    // The list depends only on the unit system, so build each once and share it.
    static const std::vector<Candidate> metric = buildCandidates(cwUnits::Metric);
    static const std::vector<Candidate> imperial = buildCandidates(cwUnits::Imperial);
    return system == cwUnits::Imperial ? imperial : metric;
}

cwScaleBarSelection cwScaleBarSelector::selectViewScale(double pixelsPerMeter,
                                                        double minTotalWidth,
                                                        double maxTotalWidth,
                                                        int cells,
                                                        cwUnits::UnitSystem system) const
{
    cwScaleBarSelection result;
    const std::vector<Candidate>& candidates = niceCandidates(system);
    if (candidates.empty()) {
        return result;
    }

    // Fall back to the smallest candidate so callers always get a real unit and
    // increment — even before the camera is ready (pixelsPerMeter <= 0), when the
    // bar has no width to draw anyway.
    const Candidate* chosen = &candidates.front();

    if (pixelsPerMeter > 0.0 && cells > 0 && maxTotalWidth > 0.0) {
        // Aim a little below the maximum so the bar has room to breathe.
        const double bestWidth = maxTotalWidth - minTotalWidth / 2.0;
        double nearestToBest = std::numeric_limits<double>::max();

        for (const Candidate& candidate : candidates) {
            const double totalWidth = cells * candidate.meters * pixelsPerMeter;
            const double distance = std::abs(bestWidth - totalWidth);
            if (distance < nearestToBest) {
                nearestToBest = distance;
                chosen = &candidate;
                result.valid = (totalWidth >= minTotalWidth && totalWidth <= maxTotalWidth);
            }
        }
    }

    result.value = chosen->value;
    result.unit = chosen->unit;
    return result;
}

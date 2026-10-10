// Our includes
#include "cwColorBy.h"
#include "cwColorRamp.h"
#include "cwKeywordColorBy.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
#include "cwRenderLinePlot.h"
#include "cwRenderTexturedItems.h"
#include "cwScene.h"

// Qt includes
#include <QBox3D>
#include <QSettings>

// Std includes
#include <algorithm>
#include <array>
#include <cmath>

namespace {
QString sourceKey() { return QStringLiteral("colorBy/source"); }
QString rampIdKey() { return QStringLiteral("colorBy/rampId"); }
QString pivotKey() { return QStringLiteral("colorBy/pivot"); }
QString scrapStrengthKey() { return QStringLiteral("colorBy/scrapStrength"); }
QString keywordKeyKey() { return QStringLiteral("colorBy/keywordKey"); }

// How strongly the ramp tints scrap paper; ink stays dark at any strength.
constexpr double kDefaultScrapStrength = 0.6;
constexpr float kOpaque = 1.0f;

// Diverging normalize: the pivot sits at the middle of the bar.
constexpr double kPivotPosition = 0.5;
constexpr double kHalf = 0.5;

// Legend ticks: {1,2,5} x 10^n steps, n in kMinTickExponent..kMaxTickExponent.
// The smallest step is one whole meter or foot.
constexpr int kMaxTicks = 7;
constexpr std::array<double, 3> kTickMantissas = {1.0, 2.0, 5.0};
constexpr int kMinTickExponent = 0;
constexpr int kMaxTickExponent = 5;
constexpr double kTickBase = 10.0;
// Absorbs unit-conversion round-off so a range end that is a tick stays one
constexpr double kTickEpsilon = 1e-9;
// A flat range snaps to the whole unit below it and this many units above that
constexpr double kFlatRangeSpan = 1.0;

enum RangeIndex { RangeMin = 0, RangeMax = 1, RangePivot = 2, RangeHalfExtent = 3 };
enum ParamIndex { ParamMode = 0, ParamStrength = 1, ParamSlotCount = 2, ParamValueSource = 3 };

// The stored ramp, or the depth preset when the id is unknown
cwColorRamp resolvedRamp(const QString& id)
{
    const cwColorRamp ramp = cwColorRamp::preset(id);
    if (ramp.isValid()) {
        return ramp;
    }
    return cwColorRamp::preset(cwColorRamp::defaultDepthRampId());
}

// Diverging half extent: the larger distance from the pivot to either end
double halfExtentOf(double rangeMin, double rangeMax, double pivot)
{
    return std::max(rangeMax - pivot, pivot - rangeMin);
}

double clampedStrength(double strength)
{
    return std::clamp(strength, 0.0, 1.0);
}

// A range in depth-unit values, snapped outward to multiples of interval
struct SnappedRange
{
    double lo;
    double hi;
    double interval;
};

// Smallest {1,2,5}x10^n step (n from 0, whole units, up to kMaxTickExponent)
// whose multiples bracket [lo, hi] with at most kMaxTicks ticks inclusive;
// lo floors and hi ceils to that step.
SnappedRange snapRange(double lo, double hi)
{
    if (hi - lo <= 0.0) {
        const double flatLo = std::floor(lo + kTickEpsilon);
        return {flatLo, flatLo + kFlatRangeSpan, kFlatRangeSpan};
    }

    const auto firstIndex = [&](double interval) { return std::floor(lo / interval + kTickEpsilon); };
    const auto lastIndex = [&](double interval) { return std::ceil(hi / interval - kTickEpsilon); };
    const auto snappedWith = [&](double interval) {
        return SnappedRange{firstIndex(interval) * interval, lastIndex(interval) * interval, interval};
    };

    double interval = 0.0;
    for (int exponent = kMinTickExponent; exponent <= kMaxTickExponent; ++exponent) {
        for (double mantissa : kTickMantissas) {
            interval = mantissa * std::pow(kTickBase, exponent);
            if (lastIndex(interval) - firstIndex(interval) + 1 <= kMaxTicks) {
                return snappedWith(interval);
            }
        }
    }
    // A range wider than the largest step allows: use the largest anyway
    return snappedWith(interval);
}

// Every multiple of the snapped range's interval from lo to hi inclusive, in meters
QList<double> ticksOf(const SnappedRange& range, cwUnits::LengthUnit unit)
{
    QList<double> ticks;
    const int first = static_cast<int>(std::round(range.lo / range.interval));
    const int last = static_cast<int>(std::round(range.hi / range.interval));
    for (int i = first; i <= last; ++i) {
        ticks.append(cwUnits::convert(i * range.interval, unit, cwUnits::Meters));
    }
    return ticks;
}
}

cwColorBy::cwColorBy(QObject* parent) :
    QObject(parent),
    m_keywordColorBy(new cwKeywordColorBy(this)),
    m_rampId(cwColorRamp::defaultDepthRampId()),
    m_scrapStrength(kDefaultScrapStrength)
{
    // The slot count rides in the uniform, and the values feed the legend.
    connect(m_keywordColorBy, &cwKeywordColorBy::slotsChanged, this, [this]() {
        rebuildUniform();
        emit legendChanged();
    });

    loadSettings();
    rebuildUniform();
}

void cwColorBy::setKeywordItemModel(cwKeywordItemModel* model)
{
    if (m_keywordItemModel) {
        disconnect(m_keywordItemModel, nullptr, this, nullptr);
    }
    m_keywordItemModel = model;

    // The visible model forwards only rows entering and leaving the filter, so
    // a keyword edit on an item that stays visible arrives through here.
    if (m_keywordItemModel) {
        connect(m_keywordItemModel, &QAbstractItemModel::dataChanged, this,
                [this](const QModelIndex&, const QModelIndex&, const QList<int>& roles) {
            if (roles.contains(cwKeywordItemModel::KeywordsRole)) {
                scheduleRegroup();
            }
        });
        const auto keywordRowsChanged = [this](const QModelIndex& parent) {
            if (parent.isValid()) {
                scheduleRegroup();
            }
        };
        connect(m_keywordItemModel, &QAbstractItemModel::rowsInserted, this, keywordRowsChanged);
        connect(m_keywordItemModel, &QAbstractItemModel::rowsRemoved, this, keywordRowsChanged);
    }
    updateGroupingModel();
}

void cwColorBy::setVisibleKeywordModel(QAbstractItemModel* model)
{
    m_visibleKeywordModel = model;
    updateGroupingModel();
}

QAbstractItemModel* cwColorBy::groupingModel() const
{
    // Slots follow the visible items; with no filter wired, every item is visible.
    if (m_visibleKeywordModel) {
        return m_visibleKeywordModel;
    }
    return m_keywordItemModel;
}

void cwColorBy::scheduleRegroup()
{
    // The full item model is the grouping model when no filter is wired, and
    // then it reports its own edits.
    if (m_visibleKeywordModel == nullptr || m_regroupPending) {
        return;
    }
    m_regroupPending = true;
    QMetaObject::invokeMethod(this, [this]() {
        m_regroupPending = false;
        m_keywordColorBy->regroup();
    }, Qt::QueuedConnection);
}

void cwColorBy::updateGroupingModel()
{
    m_keywordColorBy->setItemModel(groupingModel());
    updateKeywordGrouping();
    rebuildUniform();
}

void cwColorBy::setTargets(cwRenderLinePlot* linePlot, cwRenderTexturedItems* items)
{
    m_linePlot = linePlot;
    m_items = items;
    push();
}

void cwColorBy::setScene(cwScene* scene)
{
    if (m_scene == scene) {
        return;
    }

    if (m_scene) {
        disconnect(m_scene, &cwScene::visibleBoundsChanged, this, &cwColorBy::scheduleRangeUpdate);
    }

    m_scene = scene;

    if (m_scene) {
        connect(m_scene, &cwScene::visibleBoundsChanged, this, &cwColorBy::scheduleRangeUpdate);
        scheduleRangeUpdate();
    }
}

void cwColorBy::setSource(Source source)
{
    if (m_source == source) {
        return;
    }
    m_source = source;
    updateKeywordGrouping();
    rebuildUniform();
    emit sourceChanged();
    emit legendChanged();
}

QStringList cwColorBy::hiddenKeywordKeys()
{
    return {cwKeywordModel::ObjectIdKey, cwKeywordModel::FileNameKey};
}

void cwColorBy::setKeywordKey(const QString& key)
{
    if (m_keywordKey == key) {
        return;
    }
    m_keywordKey = key;
    saveSettings();
    updateKeywordGrouping();
    rebuildUniform();
    emit keywordKeyChanged();
}

QStringList cwColorBy::legendValues() const
{
    if (m_source == Source::Keyword) {
        return m_keywordColorBy->values();
    }
    return {};
}

int cwColorBy::legendSlotCount() const
{
    if (m_source == Source::Keyword) {
        return m_keywordColorBy->slotCount();
    }
    return 0;
}

QColor cwColorBy::slotColor(int slot) const
{
    // The categorical preset is fixed, so bake it once for every legend row.
    static const QVector<QColor> samples = cwColorRamp::preset(cwColorRamp::defaultCategoricalRampId())
                                               .bake(cwColorByUniform::kRampStops);
    return samples.at(std::clamp(slot, 0, static_cast<int>(samples.size()) - 1));
}

int cwColorBy::legendSlot(int row) const
{
    return row < legendSlotCount() ? row : cwKeywordColorBy::kOtherSlot;
}

void cwColorBy::updateKeywordGrouping()
{
    // Grouping walks every visible keyword item, so it runs only while it is shown.
    const bool grouping = m_source == Source::Keyword && groupingModel() != nullptr;
    m_keywordColorBy->setKey(grouping ? m_keywordKey : QString());
}

void cwColorBy::setRampId(const QString& id)
{
    if (m_rampId == id) {
        return;
    }
    m_rampId = id;
    saveSettings();
    rebuildUniform();
    emit rampIdChanged();
}

QStringList cwColorBy::rampIds() const
{
    QStringList ids;
    const QVector<cwColorRamp> ramps = cwColorRamp::presets();
    for (const cwColorRamp& ramp : ramps) {
        if (ramp.kind() != cwColorRamp::Kind::Categorical) {
            ids.append(ramp.id());
        }
    }
    return ids;
}

QString cwColorBy::rampName(const QString& id) const
{
    return cwColorRamp::preset(id).name();
}

double cwColorBy::pivot() const
{
    if (m_hasPivotManual) {
        return m_pivotManual;
    }
    return (m_rangeMin + m_rangeMax) * kHalf;
}

void cwColorBy::setPivotManual(double pivot)
{
    if (m_hasPivotManual && m_pivotManual == pivot) {
        return;
    }
    applyPivotManual(true, pivot);
}

void cwColorBy::clearPivotManual()
{
    if (!m_hasPivotManual) {
        return;
    }
    applyPivotManual(false, m_pivotManual);
}

void cwColorBy::applyPivotManual(bool hasManual, double manual)
{
    const double oldPivot = pivot();
    m_hasPivotManual = hasManual;
    m_pivotManual = manual;
    rebuildUniform();
    emit pivotManualChanged();
    if (oldPivot != pivot()) {
        emit pivotChanged();
    }
}

void cwColorBy::setUnitSystem(cwUnits::UnitSystem system)
{
    if (m_unitSystem == system) {
        return;
    }
    m_unitSystem = system;
    // Feet and meters snap to different round numbers
    applyDataRange();
    emit unitSystemChanged();
}

QString cwColorBy::formatDepth(double meters) const
{
    const cwUnits::LengthUnit unit = cwUnits::depthUnit(m_unitSystem);
    // IEEE: -0.0 + 0.0 == +0.0, so a value that rounds to zero shows no sign
    const double value = std::round(cwUnits::convert(meters, cwUnits::Meters, unit)) + 0.0;
    const QString sign = value < 0.0 ? QStringLiteral(u"\u2212") : QString();
    return QStringLiteral("%1%2 %3").arg(sign).arg(std::abs(value), 0, 'f', 0).arg(cwUnits::unitName(unit));
}

void cwColorBy::setScrapStrength(double strength)
{
    const double clamped = clampedStrength(strength);
    if (m_scrapStrength == clamped) {
        return;
    }
    m_scrapStrength = clamped;
    saveSettings();
    rebuildUniform();
    emit scrapStrengthChanged();
}

double cwColorBy::normalized(double value) const
{
    const int mode = static_cast<int>(m_uniform.params[ParamMode]);
    double t = 0.0;
    switch (mode) {
    case cwColorByUniform::Sequential:
        t = (value - m_rangeMin) / (m_rangeMax - m_rangeMin);
        break;
    case cwColorByUniform::Diverging: {
        const double resolvedPivot = pivot();
        t = kPivotPosition + kPivotPosition * (value - resolvedPivot) / halfExtentOf(m_rangeMin, m_rangeMax, resolvedPivot);
        break;
    }
    default:
        // Off, and Categorical, whose value is a slot index rather than a position
        return 0.0;
    }
    return std::clamp(t, 0.0, 1.0);
}

QColor cwColorBy::colorAt(double t) const
{
    return resolvedRamp(m_rampId).colorAt(t);
}

void cwColorBy::scheduleRangeUpdate()
{
    // A burst of visibility and geometry signals collapses into one recompute.
    if (m_rangeUpdatePending) {
        return;
    }
    m_rangeUpdatePending = true;
    QMetaObject::invokeMethod(this, [this]() {
        m_rangeUpdatePending = false;
        updateRangeFromScene();
    }, Qt::QueuedConnection);
}

void cwColorBy::updateRangeFromScene()
{
    if (m_scene == nullptr) {
        return;
    }

    const QBox3D box = m_scene->visibleFramingBounds();
    if (box.isNull()) {
        // Nothing visible: keep the last range
        return;
    }
    setRange(box.minimum().z(), box.maximum().z());
}

void cwColorBy::setRange(double dataMin, double dataMax)
{
    m_dataMin = dataMin;
    m_dataMax = dataMax;
    m_hasDataRange = true;
    applyDataRange();
}

void cwColorBy::applyDataRange()
{
    if (!m_hasDataRange) {
        return;
    }

    const cwUnits::LengthUnit unit = cwUnits::depthUnit(m_unitSystem);
    const SnappedRange snapped = snapRange(cwUnits::convert(m_dataMin, cwUnits::Meters, unit),
                                           cwUnits::convert(m_dataMax, cwUnits::Meters, unit));
    const double rangeMin = cwUnits::convert(snapped.lo, unit, cwUnits::Meters);
    const double rangeMax = cwUnits::convert(snapped.hi, unit, cwUnits::Meters);
    const QList<double> ticks = ticksOf(snapped, unit);

    const bool rangeMoved = m_rangeMin != rangeMin || m_rangeMax != rangeMax;
    const bool ticksMoved = m_legendTicks != ticks;
    const double oldPivot = pivot();

    m_rangeMin = rangeMin;
    m_rangeMax = rangeMax;
    m_legendTicks = ticks;

    if (rangeMoved) {
        // normalized() reads the mode from the uniform, so rebuild before listeners re-read it
        rebuildUniform();
    }
    if (ticksMoved) {
        emit legendTicksChanged();
    }
    if (rangeMoved) {
        emit rangeChanged();
        if (oldPivot != pivot()) {
            emit pivotChanged();
        }
    }
}

void cwColorBy::rebuildUniform()
{
    const bool keywordMode = m_source == Source::Keyword;
    const cwColorRamp ramp = keywordMode
        ? cwColorRamp::preset(cwColorRamp::defaultCategoricalRampId())
        : resolvedRamp(m_rampId);

    cwColorByUniform uniform;

    const QVector<QColor> samples = ramp.bake(cwColorByUniform::kRampStops);
    for (int i = 0; i < samples.size(); ++i) {
        const QColor& color = samples.at(i);
        uniform.ramp[i][0] = color.redF();
        uniform.ramp[i][1] = color.greenF();
        uniform.ramp[i][2] = color.blueF();
        uniform.ramp[i][3] = kOpaque;
    }

    const double resolvedPivot = pivot();
    const double halfExtent = halfExtentOf(m_rangeMin, m_rangeMax, resolvedPivot);
    uniform.range[RangeMin] = static_cast<float>(m_rangeMin);
    uniform.range[RangeMax] = static_cast<float>(m_rangeMax);
    uniform.range[RangePivot] = static_cast<float>(resolvedPivot);
    uniform.range[RangeHalfExtent] = static_cast<float>(halfExtent);

    const bool diverging = ramp.kind() == cwColorRamp::Kind::Diverging;
    // A non-empty range also gives the diverging half extent a positive size:
    // it is at least half the range wherever the pivot sits.
    const bool rangeIsUsable = m_rangeMax > m_rangeMin;

    int mode = cwColorByUniform::Off;
    int slotCount = 0;
    int valueSource = cwColorByUniform::WorldZ;
    if (keywordMode) {
        // Slots are pushed into the proxies only while a key groups them.
        if (!m_keywordColorBy->key().isEmpty()) {
            mode = cwColorByUniform::Categorical;
        }
        slotCount = m_keywordColorBy->slotCount();
        valueSource = cwColorByUniform::PerElement;
    } else if (m_source == Source::Depth && rangeIsUsable) {
        mode = diverging ? cwColorByUniform::Diverging : cwColorByUniform::Sequential;
    }

    uniform.params[ParamMode] = static_cast<float>(mode);
    uniform.params[ParamStrength] = static_cast<float>(m_scrapStrength);
    uniform.params[ParamSlotCount] = static_cast<float>(slotCount);
    uniform.params[ParamValueSource] = static_cast<float>(valueSource);

    m_uniform = uniform;
    push();
}

void cwColorBy::push()
{
    if (m_linePlot) {
        m_linePlot->setColorBy(m_uniform);
    }
    if (m_items) {
        m_items->setSharedUniformBlock(cwColorByUniform::toBytes(m_uniform));
    }
}

void cwColorBy::loadSettings()
{
    QSettings settings;
    // The source and a manual pivot are session-only; drop values older builds stored
    settings.remove(sourceKey());
    settings.remove(pivotKey());
    m_keywordKey = settings.value(keywordKeyKey()).toString();
    const QString storedRampId = settings.value(rampIdKey(), cwColorRamp::defaultDepthRampId()).toString();
    // An id from another build snaps to the default so the ramp picker shows a selection
    m_rampId = cwColorRamp::preset(storedRampId).isValid() ? storedRampId : cwColorRamp::defaultDepthRampId();
    m_scrapStrength = clampedStrength(settings.value(scrapStrengthKey(), kDefaultScrapStrength).toDouble());
}

void cwColorBy::saveSettings() const
{
    QSettings settings;
    settings.setValue(rampIdKey(), m_rampId);
    settings.setValue(scrapStrengthKey(), m_scrapStrength);
    settings.setValue(keywordKeyKey(), m_keywordKey);
}

// Our includes
#include "cwColorBy.h"
#include "cwColorRamp.h"
#include "cwKeywordColorBy.h"
#include "cwKeywordItemModel.h"
#include "cwRenderLinePlot.h"
#include "cwRenderTexturedItems.h"

// Qt includes
#include <QSettings>

// Std includes
#include <algorithm>

namespace {
QString sourceKey() { return QStringLiteral("colorBy/source"); }
QString rampIdKey() { return QStringLiteral("colorBy/rampId"); }
QString pivotKey() { return QStringLiteral("colorBy/pivot"); }
QString scrapStrengthKey() { return QStringLiteral("colorBy/scrapStrength"); }
QString keywordKeyKey() { return QStringLiteral("colorBy/keywordKey"); }

// How strongly the ramp tints scrap paper; ink stays dark at any strength.
constexpr double kDefaultScrapStrength = 0.6;
constexpr double kDefaultPivot = 0.0;
constexpr float kOpaque = 1.0f;

// Diverging normalize: the pivot sits at the middle of the bar.
constexpr double kPivotPosition = 0.5;

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

cwColorBy::Source sourceFromInt(int stored)
{
    switch (stored) {
    case static_cast<int>(cwColorBy::Source::Depth):
        return cwColorBy::Source::Depth;
    case static_cast<int>(cwColorBy::Source::Keyword):
        return cwColorBy::Source::Keyword;
    default:
        return cwColorBy::Source::None;
    }
}
}

cwColorBy::cwColorBy(QObject* parent) :
    QObject(parent),
    m_keywordColorBy(new cwKeywordColorBy(this)),
    m_rampId(cwColorRamp::defaultDepthRampId()),
    m_pivot(kDefaultPivot),
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
    m_keywordItemModel = model;
    m_keywordColorBy->setKeywordItemModel(model);
    updateKeywordGrouping();
    rebuildUniform();
}

void cwColorBy::setTargets(cwRenderLinePlot* linePlot, cwRenderTexturedItems* items)
{
    if (m_linePlot) {
        disconnect(m_linePlot, &cwRenderLinePlot::geometryChanged,
                   this, &cwColorBy::updateRangeFromLinePlot);
    }

    m_linePlot = linePlot;
    m_items = items;

    if (m_linePlot) {
        connect(m_linePlot, &cwRenderLinePlot::geometryChanged,
                this, &cwColorBy::updateRangeFromLinePlot);
    }

    updateRangeFromLinePlot();
}

void cwColorBy::setSource(Source source)
{
    if (m_source == source) {
        return;
    }
    m_source = source;
    saveSettings();
    updateKeywordGrouping();
    rebuildUniform();
    emit sourceChanged();
    emit legendChanged();
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
    // Grouping walks every keyword item, so it runs only while it is shown.
    const bool grouping = m_source == Source::Keyword && m_keywordItemModel;
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

void cwColorBy::setPivot(double pivot)
{
    if (m_pivot == pivot) {
        return;
    }
    m_pivot = pivot;
    saveSettings();
    rebuildUniform();
    emit pivotChanged();
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
    case cwColorByUniform::Diverging:
        t = kPivotPosition + kPivotPosition * (value - m_pivot) / halfExtentOf(m_rangeMin, m_rangeMax, m_pivot);
        break;
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

void cwColorBy::updateRangeFromLinePlot()
{
    bool rangeMoved = false;
    if (m_linePlot && m_linePlot->minZValue() <= m_linePlot->maxZValue()) {
        const double rangeMin = m_linePlot->minZValue();
        const double rangeMax = m_linePlot->maxZValue();
        if (m_rangeMin != rangeMin || m_rangeMax != rangeMax) {
            m_rangeMin = rangeMin;
            m_rangeMax = rangeMax;
            rangeMoved = true;
        }
    }
    // normalized() reads the mode from the uniform, so rebuild before listeners re-read it
    rebuildUniform();
    if (rangeMoved) {
        emit rangeChanged();
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

    const double halfExtent = halfExtentOf(m_rangeMin, m_rangeMax, m_pivot);
    uniform.range[RangeMin] = static_cast<float>(m_rangeMin);
    uniform.range[RangeMax] = static_cast<float>(m_rangeMax);
    uniform.range[RangePivot] = static_cast<float>(m_pivot);
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
    m_source = sourceFromInt(settings.value(sourceKey(), static_cast<int>(Source::None)).toInt());
    m_keywordKey = settings.value(keywordKeyKey()).toString();
    const QString storedRampId = settings.value(rampIdKey(), cwColorRamp::defaultDepthRampId()).toString();
    // An id from another build snaps to the default so the ramp picker shows a selection
    m_rampId = cwColorRamp::preset(storedRampId).isValid() ? storedRampId : cwColorRamp::defaultDepthRampId();
    m_pivot = settings.value(pivotKey(), kDefaultPivot).toDouble();
    m_scrapStrength = clampedStrength(settings.value(scrapStrengthKey(), kDefaultScrapStrength).toDouble());
}

void cwColorBy::saveSettings() const
{
    QSettings settings;
    settings.setValue(sourceKey(), static_cast<int>(m_source));
    settings.setValue(rampIdKey(), m_rampId);
    settings.setValue(pivotKey(), m_pivot);
    settings.setValue(scrapStrengthKey(), m_scrapStrength);
    settings.setValue(keywordKeyKey(), m_keywordKey);
}

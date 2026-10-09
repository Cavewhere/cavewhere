#ifndef CWCOLORBY_H
#define CWCOLORBY_H

// Our includes
#include "cwGlobals.h"
#include "cwColorByUniform.h"
#include "cwUnits.h"

// Qt includes
#include <QColor>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QStringList>

class cwKeywordColorBy;
class cwKeywordItemModel;
class cwRenderLinePlot;
class cwRenderTexturedItems;
class cwScene;
class QAbstractItemModel;

// The color-by view preference: which value colors the 3D view, through which
// ramp, around which pivot. Builds the cwColorByUniform block and pushes it to
// the render objects. Persisted in QSettings as an app-level preference.
class CAVEWHERE_LIB_EXPORT cwColorBy : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ColorBy)
    QML_UNCREATABLE("ColorBy is owned by RegionSceneManager")

    Q_PROPERTY(Source source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QString rampId READ rampId WRITE setRampId NOTIFY rampIdChanged)
    Q_PROPERTY(QStringList rampIds READ rampIds CONSTANT)
    Q_PROPERTY(double pivot READ pivot WRITE setPivot NOTIFY pivotChanged)
    Q_PROPERTY(double rangeMin READ rangeMin NOTIFY rangeChanged)
    Q_PROPERTY(double rangeMax READ rangeMax NOTIFY rangeChanged)
    Q_PROPERTY(double scrapStrength READ scrapStrength WRITE setScrapStrength NOTIFY scrapStrengthChanged)
    Q_PROPERTY(QString keywordKey READ keywordKey WRITE setKeywordKey NOTIFY keywordKeyChanged)
    Q_PROPERTY(QStringList hiddenKeywordKeys READ hiddenKeywordKeys CONSTANT)
    Q_PROPERTY(QStringList legendValues READ legendValues NOTIFY legendChanged)
    Q_PROPERTY(int legendSlotCount READ legendSlotCount NOTIFY legendChanged)
    Q_PROPERTY(cwUnits::UnitSystem unitSystem READ unitSystem WRITE setUnitSystem NOTIFY unitSystemChanged)
    Q_PROPERTY(QList<double> legendTicks READ legendTicks NOTIFY legendTicksChanged)

public:
    enum class Source { None, Depth, Keyword };
    Q_ENUM(Source)

    explicit cwColorBy(QObject* parent = nullptr);

    // Render objects the uniform is pushed to. Either may be null.
    void setTargets(cwRenderLinePlot* linePlot, cwRenderTexturedItems* items);

    // The depth range follows the z extent of this scene's visible bounds.
    void setScene(cwScene* scene);

    // Every keyword item. Keyword mode groups these when no visible model is set.
    void setKeywordItemModel(cwKeywordItemModel* model);
    // The keyword items that pass the keyword filter. Keyword mode groups these,
    // so slots and the legend follow what is visible.
    void setVisibleKeywordModel(QAbstractItemModel* model);

    Source source() const { return m_source; }
    void setSource(Source source);
    QString rampId() const { return m_rampId; }
    void setRampId(const QString& id);
    QStringList rampIds() const;
    Q_INVOKABLE QString rampName(const QString& id) const;
    double pivot() const { return m_pivot; }
    void setPivot(double pivot);
    double rangeMin() const { return m_rangeMin; }
    double rangeMax() const { return m_rangeMax; }
    double scrapStrength() const { return m_scrapStrength; }
    void setScrapStrength(double strength);
    QString keywordKey() const { return m_keywordKey; }
    void setKeywordKey(const QString& key);
    // Keys whose values are unique per item or meaningless as colors.
    static QStringList hiddenKeywordKeys();

    // Keyword mode's legend: the values in slot order, then "Others" when some
    // item lacks the key. Empty in the other modes.
    QStringList legendValues() const;
    // Legend rows below this own slots; a row at or past it is "Others".
    int legendSlotCount() const;
    // The categorical color of a slot, as the shader draws it.
    Q_INVOKABLE QColor slotColor(int slot) const;
    // The slot a legend row stands for.
    Q_INVOKABLE int legendSlot(int row) const;

    // The unit system the depth legend labels and rounds in.
    cwUnits::UnitSystem unitSystem() const { return m_unitSystem; }
    void setUnitSystem(cwUnits::UnitSystem system);

    // Depth legend ticks, in meters: every multiple of a {1,2,5}x10^n step
    // (picked in the depth unit) that falls inside the range, at most seven.
    QList<double> legendTicks() const { return m_legendTicks; }
    // A depth in the unit system's depth unit, whole numbers: "-500 m", "-1640 ft".
    Q_INVOKABLE QString formatDepth(double meters) const;

    // Where a value sits on the bar (0..1, the same rule as the shader), and
    // the color at that position.
    Q_INVOKABLE double normalized(double value) const;
    Q_INVOKABLE QColor colorAt(double t) const;

    // The block exactly as pushed to the renderers.
    cwColorByUniform uniform() const { return m_uniform; }

signals:
    void sourceChanged();
    void rampIdChanged();
    void pivotChanged();
    void rangeChanged();
    void scrapStrengthChanged();
    void keywordKeyChanged();
    void legendChanged();
    void unitSystemChanged();
    void legendTicksChanged();

private:
    void scheduleRangeUpdate();
    void updateRangeFromScene();
    void setRange(double rangeMin, double rangeMax);
    void rebuildUniform();
    void updateLegendTicks();
    void push();
    QAbstractItemModel* groupingModel() const;
    void updateGroupingModel();
    void updateKeywordGrouping();
    void scheduleRegroup();
    void loadSettings();
    void saveSettings() const;

    QPointer<cwRenderLinePlot> m_linePlot;
    QPointer<cwRenderTexturedItems> m_items;
    QPointer<cwScene> m_scene;
    QPointer<cwKeywordItemModel> m_keywordItemModel;
    QPointer<QAbstractItemModel> m_visibleKeywordModel;
    cwKeywordColorBy* m_keywordColorBy;
    Source m_source = Source::None;
    QString m_rampId;
    double m_pivot = 0.0;
    double m_rangeMin = 0.0;
    double m_rangeMax = 0.0;
    double m_scrapStrength = 0.0;
    QString m_keywordKey;
    cwUnits::UnitSystem m_unitSystem = cwUnits::Metric;
    QList<double> m_legendTicks;
    cwColorByUniform m_uniform;
    bool m_rangeUpdatePending = false;
    bool m_regroupPending = false;
};

#endif // CWCOLORBY_H

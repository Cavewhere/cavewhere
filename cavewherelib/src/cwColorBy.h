#ifndef CWCOLORBY_H
#define CWCOLORBY_H

// Our includes
#include "cwGlobals.h"
#include "cwColorByUniform.h"

// Qt includes
#include <QColor>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QStringList>

class cwRenderLinePlot;
class cwRenderTexturedItems;

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

public:
    enum class Source { None, Depth };
    Q_ENUM(Source)

    explicit cwColorBy(QObject* parent = nullptr);

    // Render objects the uniform is pushed to. Either may be null.
    void setTargets(cwRenderLinePlot* linePlot, cwRenderTexturedItems* items);

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

private:
    void updateRangeFromLinePlot();
    void rebuildUniform();
    void push();
    void loadSettings();
    void saveSettings() const;

    QPointer<cwRenderLinePlot> m_linePlot;
    QPointer<cwRenderTexturedItems> m_items;
    Source m_source = Source::None;
    QString m_rampId;
    double m_pivot = 0.0;
    double m_rangeMin = 0.0;
    double m_rangeMax = 0.0;
    double m_scrapStrength = 0.0;
    cwColorByUniform m_uniform;
};

#endif // CWCOLORBY_H

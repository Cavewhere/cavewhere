#ifndef CWCOLORRAMP_H
#define CWCOLORRAMP_H

// Our includes
#include "cwGlobals.h"

// Qt includes
#include <QColor>
#include <QString>
#include <QVector>

// A named color ramp: an id, a display name, a kind and two or more stops.
// Sequential and diverging ramps blend neighboring stops in OKLab, which keeps
// a two-stop gradient even instead of muddy in the middle.
class CAVEWHERE_LIB_EXPORT cwColorRamp
{
public:
    enum class Kind {
        Sequential,   // one hue or an ordered perceptual sweep, dark at t = 0
        Diverging,    // two hues with white at t = 0.5
        Categorical   // stops are slots, no interpolation
    };

    struct Stop {
        double position = 0.0; // 0..1, ascending
        QColor color;
    };

    cwColorRamp() = default;
    cwColorRamp(const QString& id, const QString& name, Kind kind, const QVector<Stop>& stops);

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    Kind kind() const { return m_kind; }
    const QVector<Stop>& stops() const { return m_stops; }
    bool isValid() const { return m_stops.size() >= 2; }

    // t is clamped to [0, 1]. Sequential/Diverging blend the two neighboring
    // stops in OKLab. Categorical returns stop[floor(t * stopCount)] unblended.
    QColor colorAt(double t) const;

    // count evenly spaced samples of colorAt, for the GPU block and the legend.
    // Categorical: sample i is stop[i % stopCount], so a slot index indexes it.
    QVector<QColor> bake(int count) const;

    static QVector<cwColorRamp> presets();
    static cwColorRamp preset(const QString& id); // invalid ramp when unknown
    static QString defaultDepthRampId();
    static QString defaultCategoricalRampId();

    struct Lab { double l = 0.0; double a = 0.0; double b = 0.0; };
    static Lab toOklab(const QColor& color);
    static QColor fromOklab(const Lab& lab);

private:
    QString m_id;
    QString m_name;
    Kind m_kind = Kind::Sequential;
    QVector<Stop> m_stops;
};

#endif // CWCOLORRAMP_H

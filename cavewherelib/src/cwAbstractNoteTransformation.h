#pragma once

// Qt
#include <QObject>
#include <QPointF>
#include <QSize>
#include <QMatrix4x4>
#include <QQmlEngine>
#include <QProperty>

// Ours
#include "cwGlobals.h"
#include "cwNoteTransformationData.h"

class cwLength;
class cwImageResolution;
class cwScale;

/**
 * @brief Base for note transformations; provides scale, northUp, utility calculators
 *        and a virtual matrix().
 */
class CAVEWHERE_LIB_EXPORT cwAbstractNoteTransformation : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double scale READ scale WRITE setScale NOTIFY scaleChanged)
    Q_PROPERTY(double northUp READ northUp WRITE setNorthUp NOTIFY northUpChanged BINDABLE bindableNorthUp)
    Q_PROPERTY(cwLength* scaleNumerator READ scaleNumerator CONSTANT)
    Q_PROPERTY(cwLength* scaleDenominator READ scaleDenominator CONSTANT)
    Q_PROPERTY(cwScale* scaleObject READ scaleObject NOTIFY scaleObjectChanged)
    Q_PROPERTY(QMatrix4x4 matrix READ matrix NOTIFY matrixChanged);

public:
    explicit cwAbstractNoteTransformation(QObject* parent = nullptr);

    // Common API (unchanged from your original)
    cwLength* scaleNumerator() const;
    cwLength* scaleDenominator() const;
    cwScale* scaleObject() const { return Scale; }

    void   setScale(double s);
    double scale() const;

    double northUp() const { return m_northUp.value(); }
    void setNorthUp(const double& northUpDegrees) { m_northUp = northUpDegrees; }
    QBindable<double> bindableNorthUp() { return &m_northUp; }

    // Persist/restore the common part
    void setData(const cwNoteTransformationData& data);
    cwNoteTransformationData data() const;

    // Virtual transform builder
    virtual QMatrix4x4 matrix() const = 0;

signals:
    void scaleChanged();
    void northUpChanged();
    void scaleObjectChanged();
    void matrixChanged();

protected:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwAbstractNoteTransformation, double, m_northUp, 0.0, &cwAbstractNoteTransformation::northUpChanged);

    cwScale* Scale = nullptr;
};

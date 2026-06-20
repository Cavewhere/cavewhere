/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWMEASUREMENTINTERACTION_H
#define CWMEASUREMENTINTERACTION_H

//Our includes
#include "cwScenePicker.h"

//Qt includes
#include <QPointF>
#include <QVector3D>

/**
 * Interaction that measures the straight-line distance and bearing between two
 * scene points. The user hovers (snapping to survey stations on the centerline),
 * clicks to place point A, hovers for a live preview, then clicks to place point
 * B and freeze the measurement. A further click restarts.
 *
 * Lives in InteractionManager like any other Interaction — toggle via
 * activate()/deactivate(); the manager restores the turn-table when deactivated.
 * The readout values (distance, azimuth, inclination, …) are exposed as
 * properties for the QML view; Esc-to-exit lives in QML.
 */
class cwMeasurementInteraction : public cwScenePicker
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MeasurementInteraction)

    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged)

    Q_PROPERTY(QVector3D hoverPoint READ hoverPoint NOTIFY hoverChanged)
    Q_PROPERTY(bool hoverSnapped READ hoverSnapped NOTIFY hoverChanged)
    Q_PROPERTY(bool hoverValid READ hoverValid NOTIFY hoverChanged)

    Q_PROPERTY(QVector3D firstPoint READ firstPoint NOTIFY measurementChanged)
    Q_PROPERTY(QVector3D secondPoint READ secondPoint NOTIFY measurementChanged)
    Q_PROPERTY(bool hasFirst READ hasFirst NOTIFY measurementChanged)
    Q_PROPERTY(bool hasMeasurement READ hasMeasurement NOTIFY measurementChanged)

    Q_PROPERTY(double distance READ distance NOTIFY measurementChanged)
    Q_PROPERTY(double azimuth READ azimuth NOTIFY measurementChanged)
    Q_PROPERTY(double inclination READ inclination NOTIFY measurementChanged)
    Q_PROPERTY(double horizontal READ horizontal NOTIFY measurementChanged)
    Q_PROPERTY(double vertical READ vertical NOTIFY measurementChanged)
    Q_PROPERTY(double deltaEast READ deltaEast NOTIFY measurementChanged)
    Q_PROPERTY(double deltaNorth READ deltaNorth NOTIFY measurementChanged)

public:
    enum class Mode {
        Free,         //!< a click places wherever the ray hits geometry
        StationOnly   //!< a click only places when snapped to a survey station
    };
    Q_ENUM(Mode)

    explicit cwMeasurementInteraction(QQuickItem* parent = nullptr);
    ~cwMeasurementInteraction() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode);

    QVector3D hoverPoint() const { return m_hoverPoint; }
    bool hoverSnapped() const { return m_hoverSnapped; }
    bool hoverValid() const { return m_hoverValid; }
    //! The raw cursor position from the last hover(), in Qt viewport pixels.
    //! Lets the overlay track the cursor even when the ray misses all geometry.
    QPointF hoverScreenPoint() const { return m_hoverScreenPoint; }

    QVector3D firstPoint() const { return m_firstPoint; }
    QVector3D secondPoint() const { return m_secondPoint; }
    bool hasFirst() const { return m_hasFirst; }
    bool hasMeasurement() const { return m_hasMeasurement; }

    double distance() const { return m_distance; }
    double azimuth() const { return m_azimuth; }
    double inclination() const { return m_inclination; }
    double horizontal() const { return m_horizontal; }
    double vertical() const { return m_vertical; }
    double deltaEast() const { return m_deltaEast; }
    double deltaNorth() const { return m_deltaNorth; }

    Q_INVOKABLE void hover(QPointF screenPoint);
    Q_INVOKABLE void place(QPointF screenPoint);
    Q_INVOKABLE void reset();
    Q_INVOKABLE void copyToClipboard() const;

signals:
    void modeChanged();
    void hoverChanged();
    void measurementChanged();

private:
    bool isPlaceable(const cwScenePick::Result& pick) const;
    void updateMeasurement(QVector3D from, QVector3D to);
    void clearMeasurementValues();

    Mode m_mode = Mode::Free;

    QVector3D m_hoverPoint;
    QPointF m_hoverScreenPoint;
    bool m_hoverSnapped = false;
    bool m_hoverValid = false;

    QVector3D m_firstPoint;
    QVector3D m_secondPoint;
    bool m_hasFirst = false;
    bool m_hasMeasurement = false;

    double m_distance = 0.0;
    double m_azimuth = 0.0;
    double m_inclination = 0.0;
    double m_horizontal = 0.0;
    double m_vertical = 0.0;
    double m_deltaEast = 0.0;
    double m_deltaNorth = 0.0;
};

#endif // CWMEASUREMENTINTERACTION_H

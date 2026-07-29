/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPROGRESSRING_H
#define CWPROGRESSRING_H

//Qt includes
#include <QCanvasPainterItem>
#include <QColor>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"

/**
 * A circular busy mark: a full track with an arc drawn over it.
 *
 * The arc is determinate when progress is in [0, 1] and indeterminate when it is
 * negative — the same sentinel cwTaskFutureCombineModel::progress() reports when
 * no job can say how far along it is, so the ring spins rather than drawing an
 * arc it would have to back up.
 *
 * Spinning is driven from QML by animating spinAngle, so the animation stops on
 * its own the moment a job reports real steps. The count that goes inside the
 * ring is a QML Label layered over it, not painted here, so it picks up the
 * Theme font like every other piece of text in the app.
 */
class CAVEWHERE_LIB_EXPORT cwProgressRing : public QCanvasPainterItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ProgressRing)

    //! In [0, 1]; negative spins instead of drawing an arc
    Q_PROPERTY(double progress READ progress WRITE setProgress NOTIFY progressChanged)

    //! Where the indeterminate arc starts, in degrees. Animated from QML.
    Q_PROPERTY(double spinAngle READ spinAngle WRITE setSpinAngle NOTIFY spinAngleChanged)

    Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor NOTIFY trackColorChanged)
    Q_PROPERTY(QColor arcColor READ arcColor WRITE setArcColor NOTIFY arcColorChanged)

public:
    explicit cwProgressRing(QQuickItem* parent = nullptr);
    ~cwProgressRing() override;

    double progress() const { return m_progress; }
    void setProgress(double progress);

    double spinAngle() const { return m_spinAngle; }
    void setSpinAngle(double spinAngle);

    QColor trackColor() const { return m_trackColor; }
    void setTrackColor(QColor color);

    QColor arcColor() const { return m_arcColor; }
    void setArcColor(QColor color);

protected:
    QCanvasPainterItemRenderer* createItemRenderer() const override;

signals:
    void progressChanged();
    void spinAngleChanged();
    void trackColorChanged();
    void arcColorChanged();

private:
    double m_progress = -1.0;
    double m_spinAngle = 0.0;

    //Opaque fallbacks so the strokes are well-defined before QML binds Theme
    //tokens (QCanvasPainter strokes become backend-defined otherwise).
    QColor m_trackColor = Qt::darkGray;
    QColor m_arcColor = Qt::white;
};

#endif // CWPROGRESSRING_H

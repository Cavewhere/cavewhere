/**************************************************************************
**
**    Copyright (C) 2025
**
**************************************************************************/

#ifndef CWTWOPOINTPROJECTOR_H
#define CWTWOPOINTPROJECTOR_H

// Qt includes
#include <QObject>
#include <QMatrix4x4>
#include <QPointer>
#include <QVector3D>
#include <QQmlEngine>

// Our includes
#include "CaveWhereLibExport.h"

class cwAbstract2PointItem;
class cwCamera;

/**
 * @brief Drives a cwAbstract2PointItem using 3D world coordinates.
 *
 * Projects two QVector3D positions through the supplied camera/model matrix
 * and forwards the resulting viewport-space QPointFs to the target item.
 */
class CAVEWHERE_LIB_EXPORT cwTwoPointProjector : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TwoPointProjector)

    Q_PROPERTY(cwAbstract2PointItem* target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)
    Q_PROPERTY(QMatrix4x4 modelMatrix READ modelMatrix WRITE setModelMatrix NOTIFY modelMatrixChanged)
    Q_PROPERTY(QVector3D p1World READ p1World WRITE setP1World NOTIFY p1WorldChanged)
    Q_PROPERTY(QVector3D p2World READ p2World WRITE setP2World NOTIFY p2WorldChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit cwTwoPointProjector(QObject* parent = nullptr);

    cwAbstract2PointItem* target() const;
    void setTarget(cwAbstract2PointItem* target);

    cwCamera* camera() const;
    void setCamera(cwCamera* camera);

    QMatrix4x4 modelMatrix() const;
    void setModelMatrix(const QMatrix4x4& matrix);

    QVector3D p1World() const;
    void setP1World(const QVector3D& point);

    QVector3D p2World() const;
    void setP2World(const QVector3D& point);

    bool enabled() const;
    void setEnabled(bool enabled);

signals:
    void targetChanged();
    void cameraChanged();
    void modelMatrixChanged();
    void p1WorldChanged();
    void p2WorldChanged();
    void enabledChanged();

private slots:
    void applyProjection();

private:
    void disconnectCamera();
    void connectCamera();
    bool hasValidConfiguration() const;

    QPointer<cwAbstract2PointItem> m_target;
    QPointer<cwCamera> m_camera;
    QMatrix4x4 m_modelMatrix;
    QVector3D m_p1World;
    QVector3D m_p2World;
    bool m_enabled = false;
};

#endif // CWTWOPOINTPROJECTOR_H

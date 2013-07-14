/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWQUATERNIONROTATION3D_H
#define CWQUATERNIONROTATION3D_H

#include <QQuickQGraphicsTransform3D>

class cwQuaternionRotation3d : public QQuickQGraphicsTransform3D
{
    Q_OBJECT
    Q_PROPERTY(QVector3D origin READ origin WRITE setOrigin NOTIFY originChanged)
    Q_PROPERTY(QQuaternion quaternion READ quaternion WRITE setQuaternion NOTIFY quaternionChanged)

public:
    explicit cwQuaternionRotation3d(QObject *parent = 0);

    QVector3D origin() const;
    void setOrigin(QVector3D origin);

    QQuaternion quaternion() const;
    void setQuaternion(QQuaternion quaternion);

    void applyTo(QMatrix4x4 *matrix) const;
    QQuickQGraphicsTransform3D *clone(QObject *parent) const;

signals:
    void originChanged();
    void quaternionChanged();

public slots:

private:
    QVector3D Origin; //!<
    QQuaternion Quaternion; //!<
};

/**
Gets origin
*/
inline QVector3D cwQuaternionRotation3d::origin() const {
    return Origin;
}

/**
  Gets quaternion
  */
inline QQuaternion cwQuaternionRotation3d::quaternion() const {
    return Quaternion;
}
#endif // CWQUATERNIONROTATION3D_H

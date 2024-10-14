/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWMATRIX4X4ANIMATION_H
#define CWMATRIX4X4ANIMATION_H

//Qt includes
#include <QVariantAnimation>
#include <QQmlEngine>

class cwMatrix4x4Animation : public QVariantAnimation
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Matrix4x4Animation)

public:
    explicit cwMatrix4x4Animation(QObject *parent = 0);
    
protected:
    virtual QVariant interpolated(const QVariant &from, const QVariant &to, qreal progress) const;

signals:
    
public slots:
    
};

#endif // CWMATRIX4X4ANIMATION_H

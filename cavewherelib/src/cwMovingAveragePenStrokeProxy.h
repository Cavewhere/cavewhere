/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWMOVINGAVERAGEPENSTROKEPROXY_H
#define CWMOVINGAVERAGEPENSTROKEPROXY_H

//Qt includes
#include <QIdentityProxyModel>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwMovingAveragePenStrokeProxy : public QIdentityProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MovingAveragePenStrokeProxy)

    Q_PROPERTY(int windowSize READ windowSize WRITE setWindowSize NOTIFY windowSizeChanged)

public:
    explicit cwMovingAveragePenStrokeProxy(QObject *parent = nullptr);

    int  windowSize() const { return m_windowSize; }
    void setWindowSize(int size);

    QVariant data(const QModelIndex &index, int role) const override;

signals:
    void windowSizeChanged();

private:
    int m_windowSize = 1;
};

#endif // CWMOVINGAVERAGEPENSTROKEPROXY_H

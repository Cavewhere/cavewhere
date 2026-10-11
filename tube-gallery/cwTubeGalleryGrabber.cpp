/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwTubeGalleryGrabber.h"

//Qt includes
#include <QDir>
#include <QImage>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>

cwTubeGalleryGrabber::cwTubeGalleryGrabber(QObject* parent) :
    QObject(parent)
{
}

QString cwTubeGalleryGrabber::grab(QQuickItem* item, const QString& directory, const QString& fileName) const
{
    if (!item || !item->window()) {
        return QString();
    }

    const QImage windowImage = item->window()->grabWindow();
    const qreal ratio = windowImage.devicePixelRatio();
    const QRectF sceneRect = item->mapRectToScene(QRectF(0.0, 0.0, item->width(), item->height()));
    const QRect pixelRect(QPointF(sceneRect.topLeft() * ratio).toPoint(),
                          QSizeF(sceneRect.size() * ratio).toSize());
    const QImage image = windowImage.copy(pixelRect);

    if (!QDir().mkpath(directory)) {
        return QString();
    }
    const QString path = QDir(directory).filePath(fileName);
    return image.save(path) ? path : QString();
}

QString cwTubeGalleryGrabber::fileName(const QString& caseName, const QString& preset) const
{
    static const QRegularExpression separators(QStringLiteral("[^A-Za-z0-9]+"));
    QString base = caseName;
    base.replace(separators, QStringLiteral("-"));
    while (base.endsWith(QLatin1Char('-'))) {
        base.chop(1);
    }
    return QStringLiteral("%1-%2.png").arg(base, preset);
}

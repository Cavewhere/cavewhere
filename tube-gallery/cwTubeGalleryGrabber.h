/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTUBEGALLERYGRABBER_H
#define CWTUBEGALLERYGRABBER_H

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QQuickItem>

// Writes an item's area of its window to a PNG.
class cwTubeGalleryGrabber : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TubeGalleryGrabber)
    QML_SINGLETON

public:
    explicit cwTubeGalleryGrabber(QObject* parent = nullptr);

    // Renders the window and saves item's rectangle to directory/fileName,
    // creating directory. Returns the written path, empty on failure.
    Q_INVOKABLE QString grab(QQuickItem* item, const QString& directory, const QString& fileName) const;

    // "<case>-<preset>.png" with every run of characters other than letters
    // and digits in the case name turned into one dash.
    Q_INVOKABLE QString fileName(const QString& caseName, const QString& preset) const;
};

#endif // CWTUBEGALLERYGRABBER_H

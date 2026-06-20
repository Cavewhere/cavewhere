/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWINTERACTION_H
#define CWINTERACTION_H

#include <QQuickItem>
#include <QQmlEngine>

/**
    This is the base class for all interaction

    The interaction hold a pointer to a interaction manager.
  */
class cwInteraction : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Interaction)

public:
    explicit cwInteraction(QQuickItem *parent = 0);

    //! Converts a physical screen distance in millimeters to logical viewport
    //! pixels using this item's current screen. Screen-space pick tolerances are
    //! specified in millimeters so they cover the same physical size regardless
    //! of display DPI or scaling; a fixed pixel radius drifts physically when the
    //! display's scaled resolution changes. Falls back to 96 DPI when the item is
    //! off-screen or the screen reports no physical size (bad EDID / headless).
    Q_INVOKABLE double pixelsForMillimeters(double millimeters) const;

    //! Logical viewport pixels per millimeter for a screen whose logical width is
    //! geometryWidthPx covering physicalWidthMm of glass. Derived from width only,
    //! which assumes square pixels — true on CaveWhere's target displays, and a
    //! pick tolerance only needs to be isotropic, not pixel-exact. Pure and
    //! screen-free so it can be unit tested; falls back to 96 DPI when either
    //! input is invalid.
    static double pixelsPerMillimeter(double geometryWidthPx, double physicalWidthMm);

public slots:
    void activate();
    void deactivate();

signals:
    void activated(cwInteraction* interaction);
    void deactivated(cwInteraction* interaction);

public slots:


private:

};



#endif // CWINTERACTION_H

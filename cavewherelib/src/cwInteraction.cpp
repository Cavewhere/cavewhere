/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwInteraction.h"

//Qt includes
#include <QQuickWindow>
#include <QScreen>

namespace {
    // Fallback display density when the real physical screen size is unknown
    // (off-screen item, headless test, or a monitor with no EDID physical size).
    // 96 DPI keeps pick tolerances close to their old fixed-pixel feel.
    constexpr double kFallbackDotsPerInch = 96.0;
    constexpr double kMillimetersPerInch = 25.4;
}

cwInteraction::cwInteraction(QQuickItem *parent) :
    QQuickItem(parent)
{

}

double cwInteraction::pixelsPerMillimeter(double geometryWidthPx, double physicalWidthMm)
{
    if (geometryWidthPx > 0.0 && physicalWidthMm > 0.0) {
        return geometryWidthPx / physicalWidthMm;
    }
    return kFallbackDotsPerInch / kMillimetersPerInch;
}

double cwInteraction::pixelsForMillimeters(double millimeters) const
{
    double geometryWidthPx = 0.0;
    double physicalWidthMm = 0.0;
    if (const QQuickWindow* itemWindow = window()) {
        if (const QScreen* itemScreen = itemWindow->screen()) {
            geometryWidthPx = itemScreen->geometry().width();
            physicalWidthMm = itemScreen->physicalSize().width();
        }
    }
    return millimeters * pixelsPerMillimeter(geometryWidthPx, physicalWidthMm);
}

void cwInteraction::activate()
{
    emit activated(this);
}

void cwInteraction::deactivate()
{
    emit deactivated(this);
}


/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWVISIBILITYPROXY_H
#define CWVISIBILITYPROXY_H

//Qt includes
#include <QObject>

//Our includes
#include "cwGlobals.h"

/**
 * \brief The shared setVisible() skeleton for a cwKeywordItem's visibility target.
 *
 * cwKeywordVisibility drives visibility by calling setVisible() on the QObject a
 * cwKeywordItem points at (see cwKeywordVisibility::resolveVisibility). Every such
 * target owns the same shape: a boolean visible property, a change-guarded setter,
 * and a visibleChanged() notification. The only per-target difference is which
 * render object to poke and with which id(s).
 *
 * This base owns everything but that difference; subclasses keep only their target
 * pointer + id(s) and override applyVisible() to push the resolved state into their
 * render object.
 *
 * The same target also carries the color-by value (cwKeywordColorBy assigns each
 * one its categorical slot). Targets that draw nothing colorable keep the default
 * applyColorValue(), which ignores it.
 */
class CAVEWHERE_LIB_EXPORT cwVisibilityProxy : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)

public:
    explicit cwVisibilityProxy(QObject* parent = nullptr);

    bool isVisible() const { return m_visible; }
    float colorValue() const { return m_colorValue; }

public slots:
    void setVisible(bool visible);
    void setColorValue(float value);

signals:
    void visibleChanged();

protected:
    // Push the resolved visibility into the render object. Called by setVisible()
    // only when the value actually changed; subclasses null-check their target.
    virtual void applyVisible(bool visible) = 0;

    // Push the per-element color-by value into the render object. Called by
    // setColorValue() only when the value actually changed.
    virtual void applyColorValue(float value) { Q_UNUSED(value); }

private:
    bool m_visible = true;
    float m_colorValue = 0.0f;
};

#endif // CWVISIBILITYPROXY_H

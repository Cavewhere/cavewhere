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
 */
class CAVEWHERE_LIB_EXPORT cwVisibilityProxy : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)

public:
    explicit cwVisibilityProxy(QObject* parent = nullptr);

    bool isVisible() const { return m_visible; }

public slots:
    void setVisible(bool visible);

signals:
    void visibleChanged();

protected:
    // Push the resolved visibility into the render object. Called by setVisible()
    // only when the value actually changed; subclasses null-check their target.
    virtual void applyVisible(bool visible) = 0;

private:
    bool m_visible = true;
};

#endif // CWVISIBILITYPROXY_H

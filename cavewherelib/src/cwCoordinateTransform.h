/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCOORDINATETRANSFORM_H
#define CWCOORDINATETRANSFORM_H

//Qt includes
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>
#include <memory>

//Our includes
#include "cwGeoPoint.h"
#include "cwGlobals.h"

/**
 * Wraps a single PROJ transform between two coordinate systems.
 *
 * Thread safety: PJ_CONTEXT is per-instance and not thread-safe. A single
 * cwCoordinateTransform is safe for use by ONE thread at a time. Construct
 * one per worker (LAZ loader, line-plot post-processor, GUI). If many
 * threads must share a single transform, switch to proj_clone_pj() per
 * worker — not v1.
 *
 * Same-CS short-circuit: if srcCS == dstCS (case-insensitive trimmed
 * compare), isIdentity() is true and transform()/transformInPlace() skip
 * the proj call entirely. Useful for LAZ loaders whose source already
 * matches globalCS.
 */
class CAVEWHERE_LIB_EXPORT cwCoordinateTransform
{
public:
    cwCoordinateTransform(const QString& srcCS, const QString& dstCS);
    ~cwCoordinateTransform();

    cwCoordinateTransform(const cwCoordinateTransform&) = delete;
    cwCoordinateTransform& operator=(const cwCoordinateTransform&) = delete;
    cwCoordinateTransform(cwCoordinateTransform&& other) noexcept;
    cwCoordinateTransform& operator=(cwCoordinateTransform&& other) noexcept;

    bool isValid() const;
    bool isIdentity() const;
    QString errorMessage() const;

    cwGeoPoint transform(const cwGeoPoint& src) const;
    void transformInPlace(cwGeoPoint* pts, qsizetype n) const;

    static QStringList commonProjectedCSList();
    static bool isValidCS(const QString& cs);

    /**
     * Set the directories PROJ searches for proj.db and grid-shift files.
     * Should be called once at application startup (before any
     * cwCoordinateTransform / isValidCS call) with the result of
     * cwGlobals::projDataPath(). Subsequent PROJ contexts created by this
     * class apply these paths via proj_context_set_search_paths().
     */
    static void setProjSearchPaths(const QStringList& paths);

private:
    // Pimpl: hides <proj.h> from the public include path so the test
    // executable (and other consumers that link cavewherelib but don't
    // see PROJ::proj's include dirs) doesn't need to find proj.h.
    struct Private;
    std::unique_ptr<Private> m_d;
};

/**
 * QML-facing facade for cwCoordinateTransform's static helpers.
 *
 * Registered as a singleton so QML can call:
 *   CoordinateSystem.isValidCS(text)
 *   CoordinateSystem.commonProjectedCSList()
 *
 * The C++ cwCoordinateTransform itself is non-QObject (it owns PROJ
 * resources and is move-only), so it can't be exposed directly.
 */
class CAVEWHERE_LIB_EXPORT cwCoordinateSystem : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CoordinateSystem)
    QML_SINGLETON

public:
    explicit cwCoordinateSystem(QObject* parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE static bool isValidCS(const QString& cs);
    Q_INVOKABLE static QStringList commonProjectedCSList();
};

#endif // CWCOORDINATETRANSFORM_H

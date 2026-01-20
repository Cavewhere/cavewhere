#pragma once

// Qt
#include <QObject>
#include <QSettings>

// Ours
#include "cwTriangulateWarping.h"
#include "CaveWhereLibExport.h"

/**
 * @brief Persists cwTriangulateWarping settings to QSettings.
 *
 * Listens for changes on the provided cwTriangulateWarping instance and saves
 * values so they survive app restarts.
 */
class CAVEWHERE_LIB_EXPORT cwTriangulateWarpingSettings : public QObject
{
    Q_OBJECT
public:
    explicit cwTriangulateWarpingSettings(cwTriangulateWarping* warping, QObject* parent = nullptr);

private:
    cwTriangulateWarping* m_warping = nullptr;
    bool m_loading = false;

    void load();
    void save() const;
    QString key(const QString& name) const { return QStringLiteral("TriangulateWarping/%1").arg(name); }
};

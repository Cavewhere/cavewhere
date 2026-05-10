#ifndef LAZFIXTUREHELPER_H
#define LAZFIXTUREHELPER_H

#include <QString>
#include <QTemporaryDir>
#include <QVector>
#include <QVector3D>

class cwLazLayer;

/**
 * Test helper: writes a small synthetic point cloud to a .laz file.
 *
 * Path must end in .laz or .las — LASlib picks compression by extension. All
 * points share LAS point format 0 (x/y/z only); no CRS VLR is written.
 *
 * Returns true on success.
 */
bool writeSyntheticLazFile(const QString& outPath,
                           const QVector<QVector3D>& points);

/**
 * Build a PID-tagged .laz path inside @a dir. Including
 * QCoreApplication::applicationPid() prevents collisions when multiple
 * cavewhere-test processes run in parallel.
 */
QString tempLazPath(QTemporaryDir& dir, const QString& tag);

/**
 * Write a small (5-point) synthetic .laz at @a path. Returns the same path
 * for chaining.
 */
QString writeMinimalLaz(const QString& path);

/**
 * Spin until @a layer reaches Loaded or Error, or @a timeoutMs elapses.
 * Returns true if a terminal state was reached.
 */
bool waitForLazLayerLoaded(cwLazLayer* layer, int timeoutMs = 5000);

#endif // LAZFIXTUREHELPER_H

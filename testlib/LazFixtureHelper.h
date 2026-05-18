#ifndef LAZFIXTUREHELPER_H
#define LAZFIXTUREHELPER_H

#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVector>
#include <QVector3D>

class cwLazLayer;
class cwRootData;

/**
 * Test helper: writes a small synthetic point cloud to a .laz file.
 *
 * Path must end in .laz or .las — LASlib picks compression by extension. All
 * points share LAS point format 0 (x/y/z only). If @a wktCS is non-empty, the
 * file gets an OGC WKT CRS VLR that cwLazLoader's extractEmbeddedCS will read
 * back; otherwise no CRS VLR is written (older GeoTIFF-only equivalent).
 *
 * Returns true on success.
 */
bool writeSyntheticLazFile(const QString& outPath,
                           const QVector<QVector3D>& points,
                           const QString& wktCS = QString());

/**
 * Build a PID-tagged .laz path inside @a dir. Including
 * QCoreApplication::applicationPid() prevents collisions when multiple
 * cavewhere-test processes run in parallel.
 */
QString tempLazPath(QTemporaryDir& dir, const QString& tag);

/**
 * Write a small (5-point) synthetic .laz at @a path. Returns the same path
 * for chaining. If @a wktCS is empty, the file has no embedded CS (older
 * GeoTIFF-only equivalent).
 */
QString writeMinimalLaz(const QString& path, const QString& wktCS = QString());

/**
 * Spin until @a layer reaches Loaded or Error, or @a timeoutMs elapses.
 * Returns true if a terminal state was reached.
 */
bool waitForLazLayerLoaded(cwLazLayer* layer, int timeoutMs = 5000);

/**
 * Hand @a externalPaths to @a root's region.lazLayers via addFromFiles and
 * spin both the project save-flush and the future manager to completion. Each
 * input is copied into the project's GIS Layers folder, then surfaced by
 * rescan; on return the model has one row per resulting file.
 */
void addLazAndWait(cwRootData* root, const QStringList& externalPaths);

#endif // LAZFIXTUREHELPER_H

#ifndef LAZFIXTUREHELPER_H
#define LAZFIXTUREHELPER_H

#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVector>
#include <QVector3D>
#include <QtTypes>

class cwLazLayer;
class cwRootData;

/**
 * One synthetic point carrying the standard LAS attributes a clip must pass
 * through. Fields left at their defaults exercise the "unset" path.
 */
struct LazAttributePoint {
    QVector3D position;
    quint16 intensity = 0;
    quint8 classification = 0;
    quint16 red = 0;
    quint16 green = 0;
    quint16 blue = 0;
    double gpsTime = 0.0;
    quint16 pointSourceId = 0;
};

/**
 * Writes @a points to a .laz at @a outPath in the given LAS point format
 * (default 3 = XYZ + GPS time + RGB), so clip tests can assert attribute
 * passthrough. Supports formats 0–3. Returns true on success.
 */
bool writeAttributedLazFile(const QString& outPath,
                            const QVector<LazAttributePoint>& points,
                            quint8 pointDataFormat = 3,
                            const QString& wktCS = QString());

/**
 * The point format and per-point attributes read back from a .laz file. Lets a
 * test inspect a clip's output without depending on LASlib's include path.
 */
struct LazFileContents {
    quint8 pointDataFormat = 0;
    QVector<LazAttributePoint> points;
    QVector3D headerBboxMin;
    QVector3D headerBboxMax;
};

/// Reads every point (and the header format) from the .laz at @a path. On open
/// failure returns an empty contents with no points.
LazFileContents readLazFile(const QString& path);

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

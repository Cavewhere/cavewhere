#ifndef WINDOWGRABBER_H
#define WINDOWGRABBER_H

// Our includes
#include "CaveWhereTestLibExport.h"

// Qt includes
#include <QObject>
#include <QSize>
#include <QString>
#include <qqmlintegration.h>
class QQuickItem;
class QImage;

/**
 * Test-only helper for the user-manual screenshot harness. Where
 * OffscreenRenderTester renders only the 3D scene, this grabs the whole live
 * QQuickWindow via QQuickWindow::grabWindow() — the QML UI chrome and the
 * embedded 3D view together — which is the only capture path that yields a real
 * screenshot of the running app. Requires a GPU-backed QPA for any window that
 * contains the RHI 3D view (the headless `offscreen` platform grabs an empty
 * image). Lives in cavewhere-testlib so production code carries no screenshot
 * scaffolding.
 */
class CAVEWHERE_TESTLIB_EXPORT WindowGrabber : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(WindowGrabber)
    QML_SINGLETON

public:
    explicit WindowGrabber(QObject* parent = nullptr);

    // Directory the generated screenshots are written to. Taken from the
    // CW_MANUAL_IMAGE_DIR environment variable when set (the gen script points it
    // at docs/manual/images/); otherwise a PID-tagged temp dir, so a plain test
    // run never writes into the source tree. Created if it does not exist.
    Q_INVOKABLE QString outputDir() const;

    // Grab the whole window that contains @a anyItem and save it as a PNG at
    // <outputDir>/<name>.png. Returns the written path, or an empty string on
    // failure. @a anyItem may be any visible item in the window — the grab always
    // captures the full window contents (UI chrome + 3D view).
    Q_INVOKABLE QString grabToFile(QQuickItem* anyItem, const QString& name);

    // As grabToFile, but crops the grab to @a item's bounding rect mapped into
    // window coordinates — for an element-focused screenshot. @a margin (logical
    // px) pads the crop on every side; the result is clamped to the window.
    Q_INVOKABLE QString grabItemToFile(QQuickItem* item, const QString& name, int margin);

    Q_INVOKABLE bool fileExists(const QString& path) const;
    Q_INVOKABLE QSize imageSize(const QString& path) const;
    Q_INVOKABLE bool removeFile(const QString& path) const;

private:
    // Grab whichever window owns @a item, warning (and returning a null image)
    // when there is no window or the grab comes back empty.
    QImage grabWindow(QQuickItem* item) const;
    // Resolve <outputDir>/<name>.png, ensuring the output directory exists.
    QString resolvePath(const QString& name) const;
};

#endif // WINDOWGRABBER_H

/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCaptureTileRun.h"
#include "cwCompressedImage.h"
#include "cwCompressedImageItem.h"
#include "cwConcurrent.h"
#include "cwGraphicsImageItem.h"

//AsyncFuture includes
#include "asyncfuture.h"

//Qt includes
#include <QDebug>
#include <QGraphicsItemGroup>

//Std includes
#include <cstring>

namespace {
// Premultiplied guarantees alpha 0 ⟹ an all-zero pixel word, so the
// overwhelmingly common case on a sparse page — a fully transparent tile —
// reduces to comparing each row against a zero row instead of per-pixel
// alpha reads.
bool tileHasInk(const QImage& img)
{
    if(img.format() == QImage::Format_ARGB32_Premultiplied) {
        const qsizetype rowBytes = qsizetype(img.width())
                                   * qsizetype(sizeof(QRgb));
        const QByteArray zeroRow(rowBytes, '\0');
        for(int y = 0; y < img.height(); y++) {
            if(memcmp(img.constScanLine(y), zeroRow.constData(),
                      size_t(rowBytes)) != 0) {
                return true;
            }
        }
        return false;
    }
    for(int y = 0; y < img.height(); y++) {
        const QRgb* row = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for(int x = 0; x < img.width(); x++) {
            if(qAlpha(row[x]) != 0) {
                return true;
            }
        }
    }
    return false;
}

// The export-tile worker step; touches only its value-type inputs. The
// normalization to a known QRgb format only fires on backends whose byte
// order a raw QRgb scan would misread (the offscreen readback is already
// ARGB32_Premultiplied on Metal). Fully transparent tiles — MOST of them on
// a sparse whole-cave page — come back null, and transparent premultiplied
// pixels are zero runs, so the surviving inked tiles zlib-shrink by orders
// of magnitude.
cwCompressedImage compressExportTile(const QImage& image)
{
    const QImage normalized =
        (image.format() == QImage::Format_ARGB32
         || image.format() == QImage::Format_ARGB32_Premultiplied)
            ? image
            : image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if(!tileHasInk(normalized)) {
        return cwCompressedImage();
    }
    return cwCompressedImage(normalized);
}
}

cwCaptureTileRun::cwCaptureTileRun(QObject* parent) :
    QObject(parent)
{
}

void cwCaptureTileRun::start(const QList<Tile>& tiles,
                             QGraphicsItemGroup* tileParent,
                             bool preview,
                             cwFutureManagerToken jobToken)
{
    Q_ASSERT(!m_job.isStarted()); //one run per object
    Q_ASSERT(!tiles.isEmpty());
    Q_ASSERT(tileParent != nullptr);

    m_tileParent = tileParent;
    m_preview = preview;
    m_total = tiles.size();

    m_job.start(QStringLiteral("Rendering map tiles"), m_total, jobToken);

    for(const Tile& tile : tiles) {
        AsyncFuture::observe(tile.future).context(this,
            [this, origin = tile.origin, future = tile.future]() mutable {
                QImage image;
                if(future.resultCount() == 1) {
                    image = future.result();
                }
                // Release this tile's future NOW: our QImage handle is enough
                // to keep the pixels alive for processing, and dropping the
                // future frees the render result without waiting on the rest
                // of the page.
                future = QFuture<QImage>();
                handleTileImage(origin, image);
            },
            [this, origin = tile.origin]() {
                qWarning() << "Capture tile at" << origin
                           << "was canceled; it will be missing from the capture";
                tileProcessed(false);
            });
    }
}

void cwCaptureTileRun::cancel()
{
    m_canceled = true;
    // Resolve the job-list row right away (the job list removes rows on the
    // future's cancel); the run itself still counts through to finished(),
    // which also finishes the promise.
    m_job.future().cancel();

    // Match the "nothing placed" contract of a canceled capture: tiles land
    // as they stream in, so take back the ones already placed.
    qDeleteAll(m_placedItems);
    m_placedItems.clear();
    m_tileParent = nullptr;
}

void cwCaptureTileRun::handleTileImage(QPointF origin, const QImage& image)
{
    if(image.isNull()) {
        qWarning() << "Capture tile at" << origin
                   << "produced no image; it will be missing from the capture";
        tileProcessed(false);
        return;
    }

    if(m_canceled) {
        // The run is ending at the tile boundary anyway; drop the pixels
        // without spending a worker on them, but still count the tile so the
        // run reaches finished().
        tileProcessed(true);
        return;
    }

    if(m_preview) {
        // Preview tiles are few and screen-sized, and repaint interactively
        // in the layout scene, so they stay raw. (They are also always kept:
        // the preview's bounding box derives from its tile items.)
        cwGraphicsImageItem* graphicsImage = new cwGraphicsImageItem(m_tileParent);
        graphicsImage->setImage(image);
        graphicsImage->setPos(origin);
        m_tileParent->addToGroup(graphicsImage);
        m_placedItems.append(graphicsImage);
        tileProcessed(true);
        return;
    }

    // Export tiles: ink-scan + zlib on a worker (see compressExportTile), so
    // the raw pixels are freed as soon as that tile compresses — not at the
    // end of the phase — and the GUI thread never runs the compression loop.
    auto compressFuture = cwConcurrent::run(compressExportTile, image);
    AsyncFuture::observe(compressFuture).context(this,
        [this, origin](const cwCompressedImage& tileImage) {
            if(!tileImage.isNull() && !m_canceled) {
                auto* tileItem = new cwCompressedImageItem(m_tileParent);
                tileItem->setCompressedImage(tileImage);
                tileItem->setPos(origin);
                m_tileParent->addToGroup(tileItem);
                m_placedItems.append(tileItem);
            }
            // A no-ink (null) tile still counts as placed: it rendered fine,
            // it just contributes no item — not a hole worth warning about.
            tileProcessed(true);
        },
        [this, origin]() {
            qWarning() << "Capture tile at" << origin
                       << "failed to compress; it will be missing from the capture";
            tileProcessed(false);
        });
}

void cwCaptureTileRun::tileProcessed(bool placed)
{
    if(placed) {
        ++m_placed;
    }
    ++m_processed;
    // No-op once the job is finished (e.g. a cancel resolved the job-list row
    // early), so late ticks are harmless.
    m_job.setProgressValue(m_processed);
    if(m_processed < m_total) {
        return;
    }

    // The run's terminal step. On a cancel, cancel() has already canceled the
    // job's future (which resolves the job-list row); finishing afterwards is
    // still legal and required.
    m_job.finish();

    // The placed items now belong to tileParent's owner, which may delete
    // them at any point after this.
    m_placedItems.clear();

    // Surface a wholly- or partly-empty result: the capture pipeline has no
    // failure channel, so without this the user only discovers a blank/holed
    // export by opening the saved file. The run still finishes below so the
    // export advances rather than stalling on this layer. A canceled run's
    // output is discarded, so its holes aren't worth warning about.
    if(!m_canceled) {
        if(m_placed == 0) {
            qWarning() << "Capture image is empty: all" << m_total
                       << "tiles failed to render";
        } else if(m_placed < m_total) {
            qWarning() << "Capture image is missing" << (m_total - m_placed)
                       << "of" << m_total << "tiles";
        }
    }

    emit finished();
}

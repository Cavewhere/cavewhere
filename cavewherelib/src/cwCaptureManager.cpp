/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwCaptureManager.h"
#include "cw3dRegionViewer.h"
#include "cwScene.h"
#include "cwGraphicsImageItem.h"
#include "cwMemoryUsage.h"
#include "cwDebug.h"
#include "cwImageResolution.h"
#include "cwCaptureItem.h"
#include "cwCaptureViewport.h"
#include "cwCaptureGroupModel.h"
#include "cwDebug.h"
#include "cwMappedQImage.h"
#include "cwErrorListModel.h"
#include "cwUnits.h"

//Qt includes
#include <QLabel>
#include <QPixmap>
#include <QScopedPointer>
#include <QGraphicsScene>
#include <QPainter>
#include <QGraphicsRectItem>
#include <QQmlEngine>
#include <QSvgGenerator>
#include <QPdfWriter>
#include <QTemporaryFile>
#include <QImageWriter>

//Std includes
#include <stdexcept>
#include <memory>
#include <functional>

namespace {
// Qt's TIFF handler treats any non-zero compression as LZW (zero writes
// uncompressed strips). Uncompressed page-sized exports are huge — a sparse
// map is mostly transparent zeros — and classic TIFF (the only flavor Qt
// writes) caps the file at 4 GB, which an uncompressed whole-cave page blows.
constexpr int TiffLzwCompression = 1;

// Same opt-in profile switch as cwCaptureViewport's placement stages, so one
// env var traces an export end to end.
constexpr const char* ProfileCaptureEnvVar = "CW_PROFILE_CAPTURE";
}

cwCaptureManager::cwCaptureManager(QObject *parent) :
    QAbstractListModel(parent),
    Resolution(300.0),
    BottomMargin(0.0),
    TopMargin(0.0),
    RightMargin(0.0),
    LeftMargin(0.0),
    Filetype(PNG),
    GroupModel(new cwCaptureGroupModel(this)),
    ErrorModel(new cwErrorListModel(this)),
    NumberOfImagesProcessed(0),
    Scene(new QGraphicsScene(this)),
    PaperRectangle(new QGraphicsRectItem()),
    BorderRectangle(new QGraphicsRectItem()),
    Scale(1.0)
{
    Scene->addItem(PaperRectangle);
    Scene->addItem(BorderRectangle);

    QPen pen;
    pen.setCosmetic(true);
    pen.setWidthF(1.0);

    QBrush brush;
    brush.setColor(Qt::transparent);
    brush.setStyle(Qt::SolidPattern);

    PaperRectangle->setPen(pen);
    PaperRectangle->setBrush(brush);

    QPen borderPen;
    borderPen.setWidthF(.05);
    borderPen.setJoinStyle(Qt::MiterJoin);
    BorderRectangle->setPen(borderPen);

    connect(this, &cwCaptureManager::paperSizeChanged,
            this, &cwCaptureManager::memoryRequiredChanged);
    connect(this, &cwCaptureManager::resolutionChanged,
            this, &cwCaptureManager::memoryRequiredChanged);
}

/**
 * @brief cwCaptureManager::~cwCaptureManager
 */
cwCaptureManager::~cwCaptureManager()
{
    //We must emit this to prevent a crash in QML state machine
    //cwCaptureItemManiputalor needs to delete all of it's qml items
    //before deleting cwCaptureItems
    emit aboutToDestoryManager();

    //We need to delete the CaptureViewports early. These need
    //to be delete before the QGraphicsScene in this object
    foreach(cwCaptureItem* item, Layers) {
        delete item;
    }
}

/**
* @brief cwCaptureManager::setView
* @param view
*/
void cwCaptureManager::setView(cw3dRegionViewer* view) {
    if(View != view) {
        View = view;
        emit viewChanged();
    }
}

/**
* @brief cwCaptureManager::setPaperSize
* @param paperSize
*
* Sets the paper size, currently inches
*/
void cwCaptureManager::setPaperSize(QSizeF paperSize) {
    if(PaperSize != paperSize) {
        PaperSize = paperSize;

        //Update the paper rectangle
        auto paperRect = QRectF(QPointF(0, 0), PaperSize);
        PaperRectangle->setRect(paperRect);
        Scene->setSceneRect(paperRect);

        //Update the border
        updateBorderRectangle();

        emit paperSizeChanged();
    }
}

/**
* @brief cwCaptureManager::setResolution
* @param resolution in pixel per-inch
*/
void cwCaptureManager::setResolution(double resolution) {
    if(Resolution != resolution) {
        Resolution = resolution;
        emit resolutionChanged();
    }
}

/**
* @brief cwCaptureManager::setBottomMargin
* @param bottomMargin
*/
void cwCaptureManager::setBottomMargin(double bottomMargin) {
    if(BottomMargin != bottomMargin) {
        BottomMargin = bottomMargin;
        updateBorderRectangle();
        emit bottomMarginChanged();
    }
}

/**
* @brief cwCaptureManager::setTopMargin
* @param topMargin
*/
void cwCaptureManager::setTopMargin(double topMargin) {
    if(TopMargin != topMargin) {
        TopMargin = topMargin;
        updateBorderRectangle();
        emit topMarginChanged();
    }
}

/**
 * @brief cwCaptureManager::setRightMargin
 * @param rightMargin
 */
void cwCaptureManager::setRightMargin(double rightMargin) {
    if(RightMargin != rightMargin) {
        RightMargin = rightMargin;
        updateBorderRectangle();
        emit rightMarginChanged();
    }
}

/**
 * @brief cwCaptureManager::setLeftMargin
 * @param leftMargin
 */
void cwCaptureManager::setLeftMargin(double leftMargin) {
    if(LeftMargin != leftMargin) {
        LeftMargin = leftMargin;
        updateBorderRectangle();
        emit leftMarginChanged();
    }
}

/**
 * @brief cwCaptureManager::setViewport
 * @param viewport
 *
 * This is the capturing viewport in opengl. This is the area that will be captured
 * and saved by the manager. The rectangle should be in pixels.
 */
void cwCaptureManager::setViewport(QRect viewport) {
    if(Viewport != viewport) {
        Viewport = viewport;
        emit viewportChanged();
    }
}

/**
* @brief cwCaptureManager::setFilename
* @param filename
*
* The filename of the output file
*/
void cwCaptureManager::setFilename(QUrl filename) {
    auto filenameWithExtention = appendExtention(filename, fileType());
    if(Filename != filenameWithExtention) {
        Filename = filenameWithExtention;
        emit filenameChanged();
    }
}

/**
* @brief cwCaptureManager::setFileType
* @param fileType
*/
void cwCaptureManager::setFileType(FileType fileType) {
    if(Filetype != fileType) {
        Filetype = fileType;
        emit fileTypeChanged();

        setFilename(Filename);
    }
}

/**
 * @brief cwCaptureManager::capture
 *
 * Executes the screen capture, running each capture viewport in sequence and
 * saving the result once all have finished.
 *
 * If a capture run is already in flight, this does nothing — starting a
 * second run would stack duplicate handlers on the same viewport.
 */
void cwCaptureManager::capture()
{
    if(m_capturing) { return; }
    m_capturing = true;

    NumberOfImagesProcessed = 0;
    m_canceled = false;

    //Lamba needs to be a shared pointer because it's recursive
    auto next = std::make_shared<std::function<void ()>>();

    //Because capturing in async, we need to call this in steps
    *next = [this, next]() {
        if(m_canceled) {
            //Aborted mid-run (see cancel()); stop without saving. The
            //in-flight viewport has fully stopped (this runs from its final
            //signal), so a new run may start.
            m_capturing = false;
            return;
        }

        if(NumberOfImagesProcessed < Captures.size()) {
            auto capture = Captures.at(NumberOfImagesProcessed);

            //A capture emits exactly one of finishedCapture / captureCanceled
            //— unless it is deleted mid-run, which emits destroyed instead.
            //All three are single-shot and clear each other so no stale
            //handler survives into the next capture() run.
            connect(capture, &cwCaptureViewport::finishedCapture, this, [this, next, capture]() {
                disconnect(capture, &cwCaptureViewport::captureCanceled, this, nullptr);
                disconnect(capture, &QObject::destroyed, this, nullptr);
                NumberOfImagesProcessed++;

                //Call the next capture, recursive (sorta)
                (*next)();
            }, Qt::SingleShotConnection);
            connect(capture, &cwCaptureViewport::captureCanceled, this, [this, capture]() {
                disconnect(capture, &cwCaptureViewport::finishedCapture, this, nullptr);
                disconnect(capture, &QObject::destroyed, this, nullptr);
                //Reached either via cancel() (m_canceled already set) or a
                //direct future-manager-UI cancel of the placement job. Only
                //announce the abort once. The viewport has fully stopped, so
                //a new run may start.
                m_capturing = false;
                if(!m_canceled) {
                    m_canceled = true;
                    emit canceledCapture();
                }
            }, Qt::SingleShotConnection);
            connect(capture, &QObject::destroyed, this, [this]() {
                //The in-flight capture was deleted mid-run (e.g. the user
                //removed the layer while exporting; removeCaptureViewport
                //doesn't block that). It can never emit a terminal signal now
                //— unwind the run here or m_capturing would latch true
                //forever, silently disabling every later capture().
                m_capturing = false;
                if(!m_canceled) {
                    m_canceled = true;
                    emit canceledCapture();
                }
            }, Qt::SingleShotConnection);

            capture->setPreviewCapture(false);
            capture->capture();
        } else if(NumberOfImagesProcessed == Captures.size()) {
            //Finished
            saveCaptures();
        }
    };

    //Queue the first capture
    (*next)();
}

void cwCaptureManager::cancel()
{
    if(!m_capturing || m_canceled) {
        //Nothing in flight (or already canceled) — stay silent rather than
        //announcing a canceled capture that never existed.
        return;
    }
    m_canceled = true;

    //Cancel the in-flight capture's worker-thread placement (if any). Canceling
    //an idle/finished capture is a no-op, so canceling them all is safe.
    for(cwCaptureViewport* capture : std::as_const(Captures)) {
        capture->cancelCapture();
    }

    emit canceledCapture();
}

cwFutureManagerToken cwCaptureManager::futureManagerToken() const
{
    return m_futureManagerToken;
}

void cwCaptureManager::setFutureManagerToken(cwFutureManagerToken token)
{
    if(m_futureManagerToken == token) {
        return;
    }
    m_futureManagerToken = token;

    //Propagate to every capture so each registers its own placement job.
    for(cwCaptureViewport* capture : std::as_const(Captures)) {
        capture->setFutureManagerToken(token);
    }

    emit futureManagerTokenChanged();
}

/**
 * @brief cwCaptureManager::addCaptureViewport
 * @param capture
 *
 * This adds the viewport capture to the capture manager
 */
void cwCaptureManager::addCaptureViewport(cwCaptureViewport *capture)
{
    if(Captures.contains(capture)) {
        qWarning() << "Capture already added:" << capture << LOCATION;
        return;
    }

    if(capture == nullptr) { return; }
    addPreviewCaptureItemHelper(capture);
    addFullResultionCaptureItemHelper(capture);

    connect(capture, &cwCaptureViewport::previewItemChanged, this, &cwCaptureManager::addPreviewCaptureItem);

    capture->setName(QString("Capture %1").arg(Captures.size() + 1));
    capture->setParent(this);
    capture->setFutureManagerToken(m_futureManagerToken);
    QQmlEngine::setObjectOwnership(capture, QQmlEngine::CppOwnership);

    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    Captures.append(capture);
    Layers.append(capture);
    endInsertRows();
    emit numberOfCapturesChanged();

    //Add the capture to the first group in the groupModel
    if(groupModel()->rowCount() == 0) {
        //Create the first group
        groupModel()->addGroup();
    }

    QModelIndex firstGroupIndex = groupModel()->index(0, 0, QModelIndex());
    groupModel()->addCapture(firstGroupIndex, capture);

}

/**
 * @brief cwCaptureManager::removeCaptureViewport
 * @param capture
 *
 * This will remove the capture from the catpure manager. This will also delete the capture.
 */
void cwCaptureManager::removeCaptureViewport(cwCaptureViewport *capture)
{
    if(!Captures.contains(capture)) {
        qWarning() << "Can't remove. Capture manager doesn't contain" << capture << LOCATION;
        return;
    }

    if(capture->scaleBarItem() != nullptr && capture->scaleBarItem()->scene() == Scene) {
        Scene->removeItem(capture->scaleBarItem());
    }

    int rowIndex = Layers.indexOf(capture);
    beginRemoveRows(QModelIndex(), rowIndex, rowIndex);
    Captures.removeAt(rowIndex);
    Layers.removeAt(rowIndex);
    endRemoveRows();

    groupModel()->removeCapture(capture);

    //Start unwinding any in-flight run now so the viewport's destructor —
    //which blocks on the placement worker — waits as briefly as possible.
    //If this was the manager's in-flight capture, its destroyed signal (hooked
    //single-shot in capture()) unwinds the manager run.
    capture->cancelCapture();
    capture->deleteLater();
}

/**
 * @brief cwCaptureLayerModel::rowCount
 * @param parent
 * @return The number of rows in the model
 */
int cwCaptureManager::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return Captures.size();
}

/**
 * @brief cwCaptureManager::data
 * @param index
 * @param role
 * @return
 */
QVariant cwCaptureManager::data(const QModelIndex &index, int role) const
{
    //Make sure the index is valid
    if(!index.isValid()) {
        return QVariant();
    }

    switch(role) {
    case LayerObjectRole:
        return QVariant::fromValue(Layers.at(index.row()));
    case LayerNameRole:
        return Layers.at(index.row())->name();
    default:
        return QVariant();
    }

    return QVariant();
}

/**
 * @brief cwCaptureManager::index
 * @param row
 * @param column
 * @param parent
 * @return The model index, see Qt documentation QAbstractItemModel::index() for details
 */
QModelIndex cwCaptureManager::index(int row, int column, const QModelIndex &parent) const
{
    return QAbstractListModel::index(row, column, parent);
}

int cwCaptureManager::indexOf(cwCaptureViewport *capture) const
{
    return Captures.indexOf(capture);
}

/**
 * @brief cwCaptureManager::roleNames
 * @return
 */
QHash<int, QByteArray> cwCaptureManager::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[LayerObjectRole] = "layerObjectRole";
    roles[LayerNameRole] = "layerNameRole";
    return roles;
}

/**
 * @brief cwCaptureManager::addPreviewCaptureItem
 *
 * This will add the preview item to the graphics scene of the preview item
 *
 * This function should only be called through a signal and slot
 */
void cwCaptureManager::addPreviewCaptureItem()
{
    Q_ASSERT(dynamic_cast<cwCaptureViewport*>(sender()) != nullptr);
    cwCaptureViewport* captureViewport = static_cast<cwCaptureViewport*>(sender());
    addPreviewCaptureItemHelper(captureViewport);
}

/**
 * @brief cwCaptureManager::addPreviewCaptureItem
 *
 * This will add the full item to the graphics scene of the preview item
 *
 * This function should only be called through a signal and slot
 */
void cwCaptureManager::addFullResultionCaptureItem()
{
    Q_ASSERT(dynamic_cast<cwCaptureViewport*>(sender()) != nullptr);
    cwCaptureViewport* captureViewport = static_cast<cwCaptureViewport*>(sender());
    addFullResultionCaptureItemHelper(captureViewport);
    disconnect(captureViewport, &cwCaptureViewport::fullResolutionItemChanged, this, &cwCaptureManager::addFullResultionCaptureItem);
    NumberOfImagesProcessed--;

    if(NumberOfImagesProcessed == 0) {
        saveScene();
    }
}

/**
 * @brief cwCaptureManager::saveCaptures
 */
void cwCaptureManager::saveCaptures()
{
    // NumberOfImagesProcessed--;

    if(NumberOfImagesProcessed == Captures.size()) {
        //All the images have been captured, go through all the captures add the fullImages
        //to the scene.

        foreach(cwCaptureViewport* capture, Captures) {
            scene()->addItem(capture->fullResolutionItem());
            capture->setResolution(resolution());
            capture->previewItem()->setVisible(false);
            capture->setPreviewCapture(false);
        }

//        PaperRectangle->setVisible(false);

        saveScene();

        foreach(cwCaptureViewport* capture, Captures) {
            capture->previewItem()->setVisible(true);
            capture->fullResolutionItem()->setVisible(false);
            disconnect(capture, &cwCaptureViewport::finishedCapture, this, &cwCaptureManager::saveCaptures);
        }
    }
}

/**
 * @brief cwCaptureManager::exportScene
 */
void cwCaptureManager::saveScene()
{
    //The capture queue is done; whatever the save outcome (including the
    //error paths below), the run is over and a new capture() may start.
    m_capturing = false;

    QSizeF imageSize = paperSize() * resolution();
    QRectF imageRect = QRectF(QPointF(), imageSize);
    QRectF sceneRect = QRectF(QPointF(), paperSize()); //Scene->itemsBoundingRect();

    // Memory trace for whole-page export debugging (same env var as
    // cwCaptureViewport's placement profile). The save is the last phase that
    // touches page-sized data, so stamp the footprint at each stage.
    const bool profile = qEnvironmentVariableIsSet(ProfileCaptureEnvVar);
    auto logStage = [profile, &imageSize](const char* stage) {
        if(profile) {
            qDebug() << "[cwCaptureManager profile] saveScene" << stage
                     << "— imageSize" << imageSize
                     << "footprintMB" << cwMemoryUsage::physFootprintMB();
        }
    };
    logStage("start");

    auto saveToImage = [&](FileType type, const QColor& backgroundFill) {
        auto size = imageSize.toSize();

        auto outputImage = cwMappedQImage::createDiskImageWithTempFile(QLatin1String("capture-manager"), size);

        qint64 imageSizeBytes = requiredSizeInBytes();
        Q_ASSERT(outputImage.sizeInBytes() == imageSizeBytes);
        outputImage.fill(backgroundFill);
        logStage("mapped image filled");

        cwImageResolution resolutionDPI(resolution(), cwUnits::DotsPerInch);
        cwImageResolution::Data resolutionDPM = resolutionDPI.convertTo(cwUnits::DotsPerMeter);

        outputImage.setDotsPerMeterX(resolutionDPM.value);
        outputImage.setDotsPerMeterY(resolutionDPM.value);

        QPainter painter(&outputImage);
        Scene->render(&painter, imageRect, sceneRect);
        logStage("scene rendered");

        //QImageWriter streams from the mapped image — PNG passes ARGB32 scan
        //lines straight to libpng, TIFF/JPEG convert in bounded chunks — so
        //the page is never duplicated in memory (verified against the Qt 6.11
        //handler sources and measured with a 2 GiB disk-backed image).
        QImageWriter imageWriter;
        imageWriter.setFileName(filename().toLocalFile());
        imageWriter.setFormat(fileTypeToExtention(type).toLocal8Bit());
        if(type == TIF) {
            imageWriter.setCompression(TiffLzwCompression);
        }
        bool success = imageWriter.write(outputImage);
        logStage("image written");
        if(!success) {
            throw std::runtime_error(QString("%1 driver had an issue saving the final image and had the following error:\"%2\"").arg(type).arg(imageWriter.errorString()).toStdString());
        }
    };

    auto saveToSVG = [&]() {
        QSvgGenerator generator;
        generator.setFileName(filename().toLocalFile());
        generator.setSize(imageSize.toSize());
        generator.setResolution(resolution());
        generator.setViewBox(imageRect);

        QPainter painter;
        painter.begin(&generator);
        Scene->render(&painter, imageRect, sceneRect);
        painter.end();
        logStage("svg written");
    };

    auto saveToPDF = [&]() {
        QPdfWriter pdfWriter(filename().toLocalFile());
        pdfWriter.setPageSize(QPageSize(paperSize(), QPageSize::Inch));
        pdfWriter.setResolution(resolution());

        QPainter painter;
        painter.begin(&pdfWriter);
        Scene->render(&painter, imageRect, sceneRect);
        painter.end();
        logStage("pdf written");
    };

    try {
        switch (fileType()) {
        case PNG:
        case TIF:
            saveToImage(fileType(), Qt::transparent);
            break;
        case JPG:
            saveToImage(fileType(), Qt::white);
            break;
        case SVG:
            saveToSVG();
            break;
        case PDF:
            saveToPDF();
            break;
        default:
            qDebug() << "Can't export to an unsupported type, this is a bug" << LOCATION;
            break;
        }

        emit finishedCapture();

    } catch(std::runtime_error e) {
        errorModel()->append(cwError(QString::fromStdString(e.what()), cwError::Fatal));
    } catch(...) {
        errorModel()->append(cwError("There was an unknown error, this is a bug!", cwError::Fatal));
    }
}

/**
 * @brief cwCaptureManager::tileProjection
 * @param viewport
 * @param originalProjection
 * @return
 *
 * This will take original viewport and original projection and find the
 * tile projection matrix using the tileViewport. The tileViewport
 * should be a sub rectangle of the original viewport. This function
 * will work with orthognal and perspective projections
 */
cwProjection cwCaptureManager::tileProjection(QRectF tileViewport,
                                                    QSizeF imageSize,
                                                    const cwProjection &originalProjection) const
{
    double originalProjectionWidth = originalProjection.right() - originalProjection.left();
    double originalProjectionHeight = originalProjection.top() - originalProjection.bottom();

    double left = originalProjection.left() + originalProjectionWidth
            * (tileViewport.left() / imageSize.width());
    double right = left + originalProjectionWidth * tileViewport.width() / imageSize.width();
    double bottom = originalProjection.bottom() + originalProjectionHeight
            * (tileViewport.top() / imageSize.height());
    double top = bottom + originalProjectionHeight * tileViewport.height() / imageSize.height();

    cwProjection projection;
    switch(originalProjection.type()) {
    case cwProjection::Perspective:
    case cwProjection::PerspectiveFrustum:
        projection.setFrustum(left, right,
                              bottom, top,
                              originalProjection.near(), originalProjection.far());
        return projection;
    case cwProjection::Ortho:
        projection.setOrtho(left, right,
                            bottom, top,
                            originalProjection.near(), originalProjection.far());
        return projection;
    default:
        qWarning() << "Can't return tile matrix because original matrix isn't a orth or perspectiveMatrix" << LOCATION;
        break;
    }
    return projection;
}

/**
 * @brief cwCaptureManager::addBorderItem
 *
 * This function will add a new border item to the scene
 */
void cwCaptureManager::addBorderItem()
{
    QPen pen;
    pen.setWidthF(4.0);
    pen.setColor(Qt::black);

    QGraphicsRectItem* rectItem = new QGraphicsRectItem();
    rectItem->setPen(QPen());
    rectItem->setBrush(Qt::NoBrush);
    rectItem->setRect(viewport());
    rectItem->setScale(Scale);
    rectItem->setZValue(1.0);

    Scene->addItem(rectItem);
}

/**
 * @brief cwCaptureManager::croppedTile
 * @param tileSize
 * @param imageSize
 * @param row
 * @param column
 * @return Return the tile size of row and column
 *
 * This may crop the tile, if the it goes beyond the imageSize
 */
QSize cwCaptureManager::calcCroppedTileSize(QSize tileSize, QSize imageSize, int row, int column) const
{
    QSize croppedTileSize = tileSize;

    bool exactEdgeX = imageSize.width() % tileSize.width() == 0;
    bool exactEdgeY = imageSize.height() % tileSize.height() == 0;
    int lastColumnIndex = imageSize.width() / tileSize.width();
    int lastRowIndex = imageSize.height() / tileSize.height();


    Q_ASSERT(column <= lastColumnIndex);
    Q_ASSERT(row <= lastRowIndex);

    if(column == lastColumnIndex && !exactEdgeX)
    {
        //Edge tile, crop it
        double tilesInImageX = imageSize.width() / (double)tileSize.width();
        double extraX = tilesInImageX - floor(tilesInImageX);
        double imageWidthCrop = ceil(tileSize.width() * extraX);
        croppedTileSize.setWidth(imageWidthCrop);
    }

    if(row == lastRowIndex && !exactEdgeY)
    {
        //Edge tile, crop it
        double tilesInImageY = imageSize.height() / (double)tileSize.height();
        double extraY = tilesInImageY - floor(tilesInImageY);
        double imageHeightCrop = ceil(tileSize.height() * extraY);
        croppedTileSize.setHeight(imageHeightCrop);
    }

    return croppedTileSize;
}

/**
 * @brief cwCaptureManager::scaleCaptureToFitPage
 * @param item
 *
 * This will try to fit the item to the page. By default it'll be added to the (0, 0) and
 * by stretched to fit the page.
 */
void cwCaptureManager::scaleCaptureToFitPage(cwCaptureViewport *item)
{
    Q_ASSERT(item->previewItem() != nullptr);

    QRectF borderRectangle = BorderRectangle->rect();
    QPen pen = BorderRectangle->pen();
    double buffer = pen.widthF();

    QSizeF paperSize = borderRectangle.size() - QSizeF(buffer, buffer);
    double itemAspect = item->viewport().height() /
            (double)item->viewport().width();
    double paperAspect = paperSize.height() / paperSize.width();

    if(itemAspect > paperAspect) {
        //Item's height is greater than it's width
        item->setPaperHeightOfItem(paperSize.height());
    } else {
        //Item's Width is greater than it's height
        item->setPaperWidthOfItem(paperSize.width());
    }

    item->setPositionOnPaper(borderRectangle.topLeft() + QPointF(buffer, buffer) * 0.5);

}

/**
 * @brief cwCaptureManager::addPreviewCaptureItemHelper
 * @param capture
 */
void cwCaptureManager::addPreviewCaptureItemHelper(cwCaptureViewport *capture)
{
    if(capture->previewItem() != nullptr) {
        Scene->addItem(capture->previewItem());
        if(capture->scaleBarItem() != nullptr && capture->scaleBarItem()->scene() != Scene) {
            Scene->addItem(capture->scaleBarItem());
        }
        scaleCaptureToFitPage(capture);
    }
}

/**
 * @brief cwCaptureManager::addFullResultionCaptureItemHelper
 * @param catpure
 */
void cwCaptureManager::addFullResultionCaptureItemHelper(cwCaptureViewport *capture)
{
    if(capture->fullResolutionItem() != nullptr) {
        Scene->addItem(capture->fullResolutionItem());
    }
}

/**
 * @brief cwCaptureManager::updateBorderRectangle
 */
void cwCaptureManager::updateBorderRectangle()
{
    QRectF borderRectangle = QRectF(QPointF(), paperSize());
    borderRectangle.setLeft(leftMargin());
    borderRectangle.setRight(paperSize().width() - rightMargin());
    borderRectangle.setTop(topMargin());
    borderRectangle.setBottom(paperSize().height() - bottomMargin());

    BorderRectangle->setRect(borderRectangle);
}

/**
* @brief cwCaptureManeger::fileTypes
* @return
*/
QStringList cwCaptureManager::fileTypes() const {
    return FileTypes.keys();
}

/**
* Returns the memory required by the capture manager
*/
double cwCaptureManager::memoryRequired() const {
    return requiredSizeInBytes() / (1024.0 * 1024.0);
}

/**
 * Returns the require image size in bytes
 */
qint64 cwCaptureManager::requiredSizeInBytes() const
{
    QSizeF imageSize = paperSize() * resolution();
    return cwMappedQImage::requiredSizeInBytes(imageSize.toSize(), QImage::Format_ARGB32);
}

QUrl cwCaptureManager::appendExtention(const QUrl &fileUrl, FileType fileType) const
{
    QString extention = fileTypeToExtention(fileType);
    QString filename = fileUrl.toLocalFile();
    QFileInfo info(filename);
    if(typeNameToFileType(info.suffix().toUpper()) != UnknownType) {
        if(!info.suffix().isEmpty()) {
            filename.chop(info.suffix().size() + 1); //also chop off the .jpg
        }
    }
    if(!filename.endsWith(QLatin1String("."))) {
        filename += QString(".");
    }

    filename += extention;
    return QUrl::fromLocalFile(filename);
}

QString cwCaptureManager::fileTypeToExtention(cwCaptureManager::FileType type) const
{
    switch(type) {
    case UnknownType:
        return QString();
    case PNG:
        return QLatin1String("png");
    case TIF:
        return QLatin1String("tif");
    case JPG:
        return QLatin1String("jpg");
    case SVG:
        return QLatin1String("svg");
    case PDF:
        return QLatin1String("pdf");
    }
    return QString("");
}

/**
* Return the memory limit in MB
*
* For 32Bit systems this is 2.0GB
* For 64bit systems this returns -1, no limit
*/
double cwCaptureManager::memoryLimit() const {

    auto isBuild32Bit = []() {
        static bool b = !QSysInfo::buildCpuArchitecture().contains(QLatin1String("64"));
        return b;
    };

    if(isBuild32Bit()) {
        return 1.0 * 1024.0;
    }
    return -1.0;
}

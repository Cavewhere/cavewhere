//Our includes
#include "cwNoteItem.h"

//Qt includes
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPixmapItem>
#include <QLabel>
#include <QUrl>
#include <QtConcurrentRun>
#include <QFuture>

cwNoteItem::cwNoteItem(QDeclarativeItem *parent) :
    QDeclarativeItem(parent),
    ImageItem(NULL),
    PixmapItem(new QGraphicsPixmapItem(this)),
    SmoothPixmapItem(new QGraphicsPixmapItem(this)),
    PixmapFutureWatcher(new QFutureWatcher<QImage>(this)),
    LODTimer(new QTimer(this))
{
    // setFlag(QGraphicsItem::ItemHasNoContents, false);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    //    PixmapItem->setTransformationMode(Qt::SmoothTransformation);
    //setSmooth(true);

    LODTimer->setInterval(100); //100ms

    connect(PixmapFutureWatcher, SIGNAL(finished()), SLOT(SetSmoothPixmap()));
    connect(LODTimer, SIGNAL(timeout()), SLOT(RenderSmooth()));
}

//void cwNoteItem::paint(QPainter* painter, const QStyleOptionGraphicsItem *, QWidget *) {
//    qDebug() << "Painting";

//    //painter->setTransform(Transform);
//    QPointF imagePosition = Transform.map(QPointF(0.0, 0.0));
//    painter->drawPixmap(imagePosition, Note);

//    painter->drawRect(boundingRect());

//}

void cwNoteItem::mouseMoveEvent ( QGraphicsSceneMouseEvent * event ) {
    if(!LastPanPoint.isNull()) { // && ImageItem != NULL) {
        //For panning
        //QPointF delta = ((QGraphicsItem*)ImageItem)->mapFromItem(this, event->pos()) - ((QGraphicsItem*)ImageItem)->mapFromItem(this, LastPanPoint); //mapToScene(event->pos());
        QPointF pixmapItemDelta = PixmapItem->mapFromItem(this, event->pos()) - PixmapItem->mapFromItem(this, LastPanPoint); //mapToScene(event->pos());
        QPointF smoothItemDelta = SmoothPixmapItem->mapFromItem(this, event->pos()) - SmoothPixmapItem->mapFromItem(this, LastPanPoint);
        LastPanPoint = event->pos();

        //ImageItem->translate(delta.x(), delta.y());
        PixmapItem->translate(pixmapItemDelta.x(), pixmapItemDelta.y());
        SmoothPixmapItem->translate(smoothItemDelta.x(), smoothItemDelta.y());
        update();
    }
}

void cwNoteItem::mousePressEvent ( QGraphicsSceneMouseEvent * event ) {
    qDebug() << "Mouse pressed: " << event;

    //For panning the view
    //if(event->button() == (Qt::LeftButton | Qt::RightButton) ) {
        LastPanPoint = event->pos();
    //}

    event->accept();
}

void cwNoteItem::mouseReleaseEvent ( QGraphicsSceneMouseEvent * event ) {
    LastPanPoint = QPointF();
}

void cwNoteItem::wheelEvent(QGraphicsSceneWheelEvent *event) {
    LODTimer->start();
    PixmapItem->show();
    SmoothPixmapItem->hide();

    //if(ImageItem == NULL) { return; }
    //Get the mouse coordianate in scene coords
    //ScaleCenter = ((QGraphicsItem*)ImageItem)->mapFromItem(this, event->pos());
    ScaleCenter = PixmapItem->mapFromItem(this, event->pos());

    //Calc the scaleFactor
    float scaleFactor = 1.1;
    if(event->delta() < 0) {
        //Zoom out
        scaleFactor = 1.0 / scaleFactor;
    }

    //    else {
    //        //Zoom in
    //    }

    //    //Update our scale
    //    float startScale = 1.0;
    //    float endingScale = scaleFactor;

    SetScale(scaleFactor);

//    QSizeF area = PixmapItem->mapRectToScene(PixmapItem->boundingRect()).size();

//    //if(!PixmapFutureWatcher->isRunning()) {
//        QFuture<QImage> scaledFuture = QtConcurrent::run(OriginalNote, &QImage::scaled, area.toSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
//        PixmapFutureWatcher->setFuture(scaledFuture);
//    //}
}

void cwNoteItem::RenderSmooth() {
    LODTimer->stop();

    QSize area = PixmapItem->mapRectToScene(PixmapItem->boundingRect()).size().toSize();

    if(area.width() >= OriginalNote.size().width() &&
            area.height() >= OriginalNote.height()) { return; }

    //if(!PixmapFutureWatcher->isRunning()) {
    QFuture<QImage> scaledFuture = QtConcurrent::run(OriginalNote, &QImage::scaled, area, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
    PixmapFutureWatcher->setFuture(scaledFuture);
}

void cwNoteItem::SetSmoothPixmap() {
    QSize area = PixmapItem->mapRectToScene(PixmapItem->boundingRect()).size().toSize();
    if(area.width() >= OriginalNote.size().width() &&
            area.height() >= OriginalNote.height()) { return; }

    PixmapItem->hide();
    SmoothPixmapItem->show();
    QImage scaledImage = PixmapFutureWatcher->future().result();
    SmoothPixmapItem->setPixmap(QPixmap::fromImage(scaledImage));
}

//void cwNoteItem::mousePressEvent(QMouseEvent* event) {
//    //For panning the view
//    if(event->button() == Qt::RightButton) {
//        LastPanPoint = event->pos();
//    }

//}

//void cwNoteItem::mouseReleaseEvent(QMouseEvent* event) {
//    LastPanPoint = QPoint();
//}

//void cwNoteItem::mouseMoveEvent(QMouseEvent* event) {

//}

//void cwNoteItem::wheelEvent(QWheelEvent* event) {

//}

/**
  \brief Sets the image source for the the note item
  */
void cwNoteItem::setImageSource(QString imageSource) {
    if(imageSource != NoteSource) {
        //Transform.reset();

        NoteSource = imageSource;
        OriginalNote = QImage(QUrl(imageSource).toLocalFile());
        QPixmap note = QPixmap::fromImage(OriginalNote);
        PixmapItem->setPixmap(note);

        RenderSmooth();

        //PixmapItem->setTransform(QTransform());

        //        emit noteChanged();
        emit imageSourceChanged();
    }
}

void cwNoteItem::setImageItem(QDeclarativeItem* item) {
    if(item != ImageItem) {
        ImageItem = item;
        emit imageChanged();
    }
}

/**
  * Sets the current zoom factor for the view
  * This will reset the transform to the identity
  \see SetScaleCenter()
  */
void cwNoteItem::SetScale(float newScale) {
    //if(ImageItem == NULL) { return; }

    QTransform matrix;
    matrix.translate(ScaleCenter.x(), ScaleCenter.y());
    matrix.scale(newScale, newScale);
    matrix.translate(-ScaleCenter.x(), -ScaleCenter.y());
    PixmapItem->setTransform(matrix, true);

    QPointF pixmapItemPosition = PixmapItem->mapToItem(this, QPointF(0.0, 0.0));
    QPointF smoothItemPosition = SmoothPixmapItem->mapToItem(this, QPointF(0.0, 0.0));
    QPointF differance = pixmapItemPosition - smoothItemPosition;
    SmoothPixmapItem->translate(differance.x(), differance.y());

    // ImageItem->setTransform(matrix, true);
}

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
    //ImageItem(NULL),
    PixmapItem(new QGraphicsPixmapItem(this)),
    //SmoothPixmapItem(new QGraphicsPixmapItem(this)),
    PixmapFutureWatcher(new QFutureWatcher<QImage>(this)),
    LODTimer(new QTimer(this))
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);

    LODTimer->setInterval(250); //100ms

    connect(PixmapFutureWatcher, SIGNAL(finished()), SLOT(SetSmoothPixmap()));
    connect(LODTimer, SIGNAL(timeout()), SLOT(RenderSmooth()));
}

/**
  \brief Fits the note item to the view
  */
void cwNoteItem::fitToView() {
    ScaleCenter = QPointF();
    FastPixmapTransform.reset();
    SmoothTransform.reset();

    //Find the scale of the item to
    float scaleX = width() / OriginalNote.width();
    float scaleY = height() / OriginalNote.height();

    float scale = qMin(scaleX, scaleY);
    SetScale(scale);
    RenderSmooth();
}


void cwNoteItem::mouseMoveEvent ( QGraphicsSceneMouseEvent * event ) {
    if(!LastPanPoint.isNull()) { // && ImageItem != NULL) {
        //For panning
        QTransform inverseFastPixmapTransform = FastPixmapTransform.inverted();
        QTransform inverseSmoothTransform =SmoothTransform.inverted();

        QPointF pixmapItemDelta = inverseFastPixmapTransform.map(event->pos()) - inverseFastPixmapTransform.map(LastPanPoint); //mapToScene(event->pos());
        QPointF smoothItemDelta = inverseSmoothTransform.inverted().map(event->pos()) - inverseSmoothTransform.inverted().map(LastPanPoint);
        LastPanPoint = event->pos();

        //ImageItem->translate(delta.x(), delta.y());
        FastPixmapTransform.translate(pixmapItemDelta.x(), pixmapItemDelta.y());
        SmoothTransform.translate(smoothItemDelta.x(), smoothItemDelta.y());

        if(PixmapItem->pixmap().cacheKey() == FastPixmap.cacheKey()) {
            PixmapItem->setTransform(FastPixmapTransform);
        } else {
            PixmapItem->setTransform(SmoothTransform);
        }
        update();
    }
}

void cwNoteItem::mousePressEvent ( QGraphicsSceneMouseEvent * event ) {
    LastPanPoint = event->pos();

    event->accept();
}

void cwNoteItem::mouseReleaseEvent ( QGraphicsSceneMouseEvent * event ) {
    LastPanPoint = QPointF();
}

void cwNoteItem::wheelEvent(QGraphicsSceneWheelEvent *event) {
    LODTimer->start();
    PixmapItem->setPixmap(FastPixmap);
    PixmapItem->setTransform(FastPixmapTransform);

    ScaleCenter = PixmapItem->mapFromItem(this, event->pos());

    //Calc the scaleFactor
    float scaleFactor = 1.1;
    if(event->delta() < 0) {
        //Zoom out
        scaleFactor = 1.0 / scaleFactor;
    }

    SetScale(scaleFactor);
}

void cwNoteItem::RenderSmooth() {
    LODTimer->stop();

    QSize area = PixmapItem->mapRectToScene(PixmapItem->boundingRect()).size().toSize();

    if(area.width() >= OriginalNote.size().width() &&
            area.height() >= OriginalNote.height()) {
        return;
    }

    QFuture<QImage> scaledFuture = QtConcurrent::run(OriginalNote, &QImage::scaled, area, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
    PixmapFutureWatcher->setFuture(scaledFuture);

}

void cwNoteItem::SetSmoothPixmap() {

    QImage scaledImage = PixmapFutureWatcher->future().result();
    SmoothPixmap = QPixmap::fromImage(scaledImage);

    if(!LODTimer->isActive()) {
        PixmapItem->setTransform(SmoothTransform);
        PixmapItem->setPixmap(SmoothPixmap);
    }

}

/**
  \brief Sets the image source for the the note item
  */
void cwNoteItem::setImageSource(QString imageSource) {
    if(imageSource != NoteSource) {
        //Transform.reset();

        NoteSource = imageSource;
        OriginalNote = QImage(QUrl(imageSource).toLocalFile());
        FastPixmap = QPixmap::fromImage(OriginalNote);
        PixmapItem->setPixmap(FastPixmap);

        RenderSmooth();

        emit imageSourceChanged();
    }
}


/**
  * Sets the current zoom factor for the view
  * This will reset the transform to the identity
  \see SetScaleCenter()
  */
void cwNoteItem::SetScale(float newScale) {
    QTransform matrix;
    matrix.translate(ScaleCenter.x(), ScaleCenter.y());
    matrix.scale(newScale, newScale);
    matrix.translate(-ScaleCenter.x(), -ScaleCenter.y());
    FastPixmapTransform = matrix * FastPixmapTransform;
    PixmapItem->setTransform(FastPixmapTransform, false);

    QPointF pixmapItemPosition = FastPixmapTransform.map(QPointF(0.0, 0.0));
    QPointF smoothItemPosition = SmoothTransform.map(QPointF(0.0, 0.0));
    QPointF differance = pixmapItemPosition - smoothItemPosition;
    SmoothTransform.translate(differance.x(), differance.y());
}

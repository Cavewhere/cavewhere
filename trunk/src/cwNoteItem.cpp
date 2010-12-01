//Our includes
#include "cwNoteItem.h"

//Qt includes
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPixmapItem>
#include <QLabel>
#include <QUrl>

cwNoteItem::cwNoteItem(QDeclarativeItem *parent) :
    QDeclarativeItem(parent),
    ImageItem(NULL)
{
   // setFlag(QGraphicsItem::ItemHasNoContents, false);
    setAcceptedMouseButtons(Qt::LeftButton);
//    PixmapItem = new QGraphicsPixmapItem(this);
//    PixmapItem->setTransformationMode(Qt::SmoothTransformation);
    //setSmooth(true);
}

//void cwNoteItem::paint(QPainter* painter, const QStyleOptionGraphicsItem *, QWidget *) {
//    qDebug() << "Painting";

//    //painter->setTransform(Transform);
//    QPointF imagePosition = Transform.map(QPointF(0.0, 0.0));
//    painter->drawPixmap(imagePosition, Note);

//    painter->drawRect(boundingRect());

//}

void cwNoteItem::mouseMoveEvent ( QGraphicsSceneMouseEvent * event ) {
    if(!LastPanPoint.isNull() && ImageItem != NULL) {
        //For panning
        QPointF delta = ((QGraphicsItem*)ImageItem)->mapFromItem(this, event->pos()) - ((QGraphicsItem*)ImageItem)->mapFromItem(this, LastPanPoint); //mapToScene(event->pos());
        //        QPointF delta = PixmapItem->mapFromItem(this, event->pos()) - PixmapItem->mapFromItem(this, LastPanPoint); //mapToScene(event->pos());
        LastPanPoint = event->pos();

        ImageItem->translate(delta.x(), delta.y());
        // PixmapItem->translate(delta.x(), delta.y());
        update();
    }
}

void cwNoteItem::mousePressEvent ( QGraphicsSceneMouseEvent * event ) {
    qDebug() << "Mouse pressed: " << event;

    //For panning the view
    if(event->button() == Qt::LeftButton) {
        LastPanPoint = event->pos();
    }

    event->accept();
}

void cwNoteItem::mouseReleaseEvent ( QGraphicsSceneMouseEvent * event ) {
    LastPanPoint = QPointF();
}

void cwNoteItem::wheelEvent(QGraphicsSceneWheelEvent *event) {
    if(ImageItem == NULL) { return; }
    //Get the mouse coordianate in scene coords
    ScaleCenter = ((QGraphicsItem*)ImageItem)->mapFromItem(this, event->pos());
    //  ScaleCenter = PixmapItem->mapFromItem(this, event->pos());

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
//    if(imageSource != NoteSource) {
//        //Transform.reset();

//        NoteSource = imageSource;
//        QPixmap note = QPixmap::fromImage(QImage(QUrl(imageSource).toLocalFile()));
//        PixmapItem->setPixmap(note);

//        PixmapItem->setTransform(QTransform());

////        emit noteChanged();
//        emit imageSourceChanged();
//    }
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
    if(ImageItem == NULL) { return; }

    QTransform matrix;
    matrix.translate(ScaleCenter.x(), ScaleCenter.y());
    matrix.scale(newScale, newScale);
    matrix.translate(-ScaleCenter.x(), -ScaleCenter.y());
   // PixmapItem->setTransform(matrix, true);
    ImageItem->setTransform(matrix, true);
}

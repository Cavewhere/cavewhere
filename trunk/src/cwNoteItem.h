#ifndef CWNOTEITEM_H
#define CWNOTEITEM_H

#include <QDeclarativeItem>
#include <QTransform>
#include <QFutureWatcher>
#include <QTimer>

class cwNoteItem : public QDeclarativeItem
{
    Q_OBJECT

    Q_PROPERTY(QString imageSource READ imageSource WRITE setImageSource NOTIFY imageSourceChanged());

public:
    explicit cwNoteItem(QDeclarativeItem *parent = 0);

    QString imageSource() const;
    void setImageSource(QString imageSource);

signals:
    void imageSourceChanged();
    void imageChanged();

public slots:

protected:

    QGraphicsPixmapItem* PixmapItem;

    QTransform FastPixmapTransform;
    QTransform SmoothTransform;

    QPixmap FastPixmap;
    QPixmap SmoothPixmap;

    QFutureWatcher<QImage>* PixmapFutureWatcher;

    //LOD timer
    QTimer* LODTimer;

    QImage OriginalNote;
    QString NoteSource;

    //For interaction
    QPointF LastPanPoint;
    QPointF ScaleCenter;

    virtual void mouseMoveEvent ( QGraphicsSceneMouseEvent * event );
    virtual void mousePressEvent ( QGraphicsSceneMouseEvent * event );
    virtual void mouseReleaseEvent ( QGraphicsSceneMouseEvent * event );
    virtual void wheelEvent(QGraphicsSceneWheelEvent *event);

    void SetScale(float scaleFactor);

protected slots:
    void RenderSmooth();
    void SetSmoothPixmap();

};

/**
  \brief Get's the image's sourec director
  */
inline QString cwNoteItem::imageSource() const {
    return NoteSource;
}


#endif // CWNOTEITEM_H

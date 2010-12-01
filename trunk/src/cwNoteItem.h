#ifndef CWNOTEITEM_H
#define CWNOTEITEM_H

#include <QDeclarativeItem>
#include <QTransform>

class cwNoteItem : public QDeclarativeItem
{
    Q_OBJECT

//    Q_PROPERTY(QImage note READ note NOTIFY noteChanged());
    Q_PROPERTY(QDeclarativeItem* image READ image WRITE setImageItem NOTIFY imageChanged());

    Q_PROPERTY(QString imageSource READ imageSource WRITE setImageSource NOTIFY imageSourceChanged());

public:
    explicit cwNoteItem(QDeclarativeItem *parent = 0);

//    QImage note() const;
    //void setNote(QImage note);

    QString imageSource() const;
    void setImageSource(QString imageSource);

    QDeclarativeItem* image() const;
    void setImageItem(QDeclarativeItem* item);

//    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *);

signals:
//    void noteChanged();
    void imageSourceChanged();
    void imageChanged();

public slots:

protected:

    //virtual void wheelEvent(QWheelEvent* event);

    QDeclarativeItem* ImageItem;
    //QGraphicsPixmapItem* PixmapItem;

//    QPixmap Note;
    QString NoteSource;

    //For interaction
    QPointF LastPanPoint;
    QPointF ScaleCenter;

    virtual void mouseMoveEvent ( QGraphicsSceneMouseEvent * event );
    virtual void mousePressEvent ( QGraphicsSceneMouseEvent * event );
    virtual void mouseReleaseEvent ( QGraphicsSceneMouseEvent * event );
    virtual void wheelEvent(QGraphicsSceneWheelEvent *event);

    void SetScale(float scaleFactor);
};

///**
//  \brief Get's the current note
//  */
//inline QImage cwNoteItem::note() const {
//    return Note;
//}

/**
  \brief Get's the image's sourec director
  */
inline QString cwNoteItem::imageSource() const {
    return NoteSource;
}

inline QDeclarativeItem* cwNoteItem::image() const {
    return ImageItem;
}

#endif // CWNOTEITEM_H

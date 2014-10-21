/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWLAYERCAPTURE_H
#define CWLAYERCAPTURE_H

//Qt includes
#include <QObject>
#include <QSizeF>
#include <QPointF>
#include <QRectF>

class cwCaptureItem : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QSizeF paperSizeOfItem READ paperSizeOfItem NOTIFY paperSizeOfItemChanged)
    Q_PROPERTY(QPointF positionOnPaper READ positionOnPaper WRITE setPositionOnPaper NOTIFY positionOnPaperChanged)
    Q_PROPERTY(double rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(QRectF boundingBox READ boundingBox NOTIFY boundingBoxChanged)

public:
    explicit cwCaptureItem(QObject *parent = 0);

    QString name() const;
    void setName(QString name);

    QSizeF paperSizeOfItem() const;

    QPointF positionOnPaper() const;
    void setPositionOnPaper(QPointF positionOnPaper);

    double rotation() const;
    void setRotation(double rotation);

    QRectF boundingBox() const;

signals:
    void nameChanged();
    void paperSizeOfItemChanged();
    void positionOnPaperChanged();
    void rotationChanged();
    void boundingBoxChanged();

public slots:

private:
    QString Name; //!<
    QSizeF PaperSizeOfItem; //!<
    QPointF PositionOnPaper; //!<
    double Rotation; //!<
    QRectF BoundingBox; //!<

protected:
    void setPaperSizeOfItem(QSizeF paperSize);
    void setBoundingBox(QRectF boundingbox);

};

/**
* @brief cwLayerCapture::name
* @return
*/
inline QString cwCaptureItem::name() const {
    return Name;
}

/**
* @brief class::itemOnPaperSize
* @return
*/
inline QSizeF cwCaptureItem::paperSizeOfItem() const {
    return PaperSizeOfItem;
}

/**
* @brief cwCaptureViewport::postitionOnPaper
* @return
*/
inline QPointF cwCaptureItem::positionOnPaper() const {
    return PositionOnPaper;
}

/**
* @brief cwCaptureItem::rotation
* @return
*/
inline double cwCaptureItem::rotation() const {
    return Rotation;
}

/**
* @brief cwcapt::boundingBox
* @return
*/
inline QRectF cwCaptureItem::boundingBox() const {
    return BoundingBox;
}
#endif // CWLAYERCAPTURE_H

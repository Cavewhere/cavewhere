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

class cwCaptureItem : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QSizeF paperSizeOfItem READ paperSizeOfItem NOTIFY paperSizeOfItemChanged)
    Q_PROPERTY(QPointF positionOnPaper READ positionOnPaper WRITE setPositionOnPaper NOTIFY positionOnPaperChanged)

public:
    explicit cwCaptureItem(QObject *parent = 0);

    QString name() const;
    void setName(QString name);

    QSizeF paperSizeOfItem() const;

    QPointF positionOnPaper() const;
    void setPositionOnPaper(QPointF positionOnPaper);

signals:
    void nameChanged();
    void paperSizeOfItemChanged();
    void positionOnPaperChanged();

public slots:

private:
    QString Name; //!<
    QSizeF PaperSizeOfItem; //!<
    QPointF PositionOnPaper; //!<

protected:
    void setPaperSizeOfItem(QSizeF paperSize);

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
* @brief cwViewportCapture::postitionOnPaper
* @return
*/
inline QPointF cwCaptureItem::positionOnPaper() const {
    return PositionOnPaper;
}


#endif // CWLAYERCAPTURE_H

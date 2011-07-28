#ifndef CWNOTE_H
#define CWNOTE_H

//Qt includes
#include <QObject>

//Our includes
#include "cwImage.h"

class cwNote : public QObject
{
    Q_OBJECT

//    Q_PROPERTY(QString imagePath READ imagePath WRITE setImagePath NOTIFY imagePathChanged)

    Q_PROPERTY(int original READ original NOTIFY originalChanged)
    Q_PROPERTY(int icon READ icon NOTIFY iconChanged)


public:
    explicit cwNote(QObject *parent = 0);

    void setImage(cwImage image);
    cwImage image() const;

//    void setImagePath(const QString& imagePath);
//    QString imagePath() const;

    int original() const;
    int icon() const;

signals:
//    void imagePathChanged();
    void originalChanged(int id);
    void iconChanged(int id);

public slots:

private:
    cwImage ImageIds;
};


/**
  \brief Get's the path for the image
  */
inline cwImage cwNote::image() const {
    return ImageIds;
}

inline int cwNote::original() const {
    return ImageIds.original();
}

inline int cwNote::icon() const {
    return ImageIds.icon();
}

#endif // CWNOTE_H

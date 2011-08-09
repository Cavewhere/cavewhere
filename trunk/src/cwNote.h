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
    Q_PROPERTY(cwImage image READ image WRITE setImage NOTIFY imageChanged)
    Q_PROPERTY(float rotate READ rotate WRITE setRotate NOTIFY rotateChanged)

public:
    explicit cwNote(QObject *parent = 0);
    cwNote(const cwNote& object);
    cwNote& operator=(const cwNote& object);

    void setImage(cwImage image);
    cwImage image() const;

    int original() const;
    int icon() const;

    void setRotate(float degrees);
    float rotate() const;

signals:
//    void imagePathChanged();
    void originalChanged(int id);
    void iconChanged(int id);
    void imageChanged(cwImage image);
    void rotateChanged(float rotate);

public slots:

private:
    cwImage ImageIds;
    float Rotation;

    void copy(const cwNote& object);
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

/**
  \brief Sets the rotation for the notes
  */
inline float cwNote::rotate() const {
    return Rotation;
}


#endif // CWNOTE_H

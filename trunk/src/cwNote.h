#ifndef CWNOTE_H
#define CWNOTE_H

//Qt includes
#include <QObject>
#include <QImage>

class cwNote : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString imagePath READ imagePath WRITE setImagePath NOTIFY imagePathChanged)

public:
    explicit cwNote(QObject *parent = 0);

    void setImagePath(const QString& imagePath);
    QString imagePath() const;

signals:
    void imagePathChanged();

public slots:

private:
    QString ImagePath;
};


/**
  \brief Get's the path for the image
  */
inline QString cwNote::imagePath() const {
    return ImagePath;
}

#endif // CWNOTE_H

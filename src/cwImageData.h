#ifndef CWIMAGEDATA_H
#define CWIMAGEDATA_H

//Qt includes
#include <QSize>
#include <QString>
#include <QByteArray>
#include <QSharedData>

/**
  This class stores

  */
class cwImageData
{
public:
    cwImageData();
    cwImageData(QSize size, int dotsPerMeter, const QByteArray& format, const QByteArray& image);

    QSize size() const;
    int dotsPerMeter() const;
    QByteArray format() const;
    QByteArray data() const;

private:
    class PrivateData : public QSharedData {
    public:
        QSize Size;
        int DotsPerMeter;
        QByteArray Format;
        QByteArray Data;
    };

    QSharedDataPointer<PrivateData> Data;
};

/**
  \brief This gets the image data size
  */
inline QSize cwImageData::size() const {
    return Data->Size;
}

/**
  \brief Gets the resolution of the image
  */
inline int cwImageData::dotsPerMeter() const {
    return Data->DotsPerMeter;
}

/**
  \brief This gets the image data format

  This is usually a jpg, or a compress opengl format
  */
inline QByteArray cwImageData::format() const {
    return Data->Format;
}

/**
  \brief This gets the image data
  */
inline QByteArray cwImageData::data() const {
    return Data->Data;
}

#endif // CWIMAGEDATA_H

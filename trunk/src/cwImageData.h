#ifndef CWIMAGEDATA_H
#define CWIMAGEDATA_H

//Qt includes
#include <QSize>
#include <QString>
#include <QByteArray>

/**
  This class stores

  */
class cwImageData
{
public:
   // cwImageData(int id);
    cwImageData();
    cwImageData(QSize size, int dotsPerMeter, const QByteArray& format, const QByteArray& image);

    QSize size() const;
    int dotsPerMeter() const;
    QByteArray format() const;
    QByteArray data() const;
   // int DatabaseID() const;

private:
    QSize Size;
    int DotsPerMeter;
    QByteArray Format;
    QByteArray Data;
    //int databaseId;

};

/**
  \brief This gets the image data size
  */
inline QSize cwImageData::size() const {
    return Size;
}

/**
  \brief Gets the resolution of the image
  */
inline int cwImageData::dotsPerMeter() const {
    return DotsPerMeter;
}

/**
  \brief This gets the image data format

  This is usually a jpg, or a compress opengl format
  */
inline QByteArray cwImageData::format() const {
    return Format;
}

/**
  \brief This gets the image data
  */
inline QByteArray cwImageData::data() const {
    return Data;
}

/**
  \brief Gets the database id.  This is the unique id that the database has
  stored for the image
  */
//inline int cwImageData::DatabaseID() const {
//    return databaseId;
//}

#endif // CWIMAGEDATA_H

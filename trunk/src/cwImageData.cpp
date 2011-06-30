#include "cwImageData.h"

//cwImageData::cwImageData(int id) {
//    Id = id;
//}

cwImageData::cwImageData(QSize size, const QByteArray& format, const QByteArray& image) {
    Size = size;
    Format = format;
    Data = image;
//    Id = -1;
}

//cwImageData::cwImageData(QSize size, QString type, const QByteArray& image, int id) {
//    Size = size;
//    Type = type;
//    Image = image;
////    Id = id;
//}

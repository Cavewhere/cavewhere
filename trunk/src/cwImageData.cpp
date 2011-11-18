#include "cwImageData.h"

//cwImageData::cwImageData(int id) {
//    Id = id;
//}

cwImageData::cwImageData() :
    DotsPerMeter(0)
{
}

cwImageData::cwImageData(QSize size, int dotsPerMeter, const QByteArray& format, const QByteArray& image) {
    Size = size;
    DotsPerMeter = dotsPerMeter;
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

#include "cwImageData.h"

//cwImageData::cwImageData(int id) {
//    Id = id;
//}

cwImageData::cwImageData() :
    Data(new PrivateData())
{
    Data->DotsPerMeter = 0;
}

cwImageData::cwImageData(QSize size, int dotsPerMeter, const QByteArray& format, const QByteArray& image) :
    Data(new PrivateData())
{
    Data->Size = size;
    Data->DotsPerMeter = dotsPerMeter;
    Data->Format = format;
    Data->Data = image;
}


//Catch includes
#include <catch.hpp>

//Our includes
#include "TestHelper.h"
#include "DXT1BlockCompare.h"
#include "cwImageData.h"
#include "cwOpenGLUtils.h"

//squish decompression
#include "squish.h"
//#include "s3tc.h"

void DXT1BlockCompare::compare(const DXT1BlockCompare::TestImage &size, const cwImageData &mipmap)
{
    CHECK(mipmap.size() == size.dim);
    CHECK(mipmap.data().size() == size.bytes);

    if(!size.blockColors.isEmpty()) {
        //DXT1 decompression makes this assumption

        QVector<unsigned int> image(size.dim.width() * size.dim.height(), 0);

        squish::DecompressImage(reinterpret_cast<squish::u8*>(image.data()),
                                size.dim.width(), size.dim.height(),
                                mipmap.data().constData(),
                                squish::kDxt1);

        //Checks that the add images dxt1 compression is working okay, if this breaks, then
        //compression isn't working
        int blockSize = 4;
        for(int c = 0; c < size.dim.width(); c += 4) {
            for(int r = 0; r < size.dim.height(); r += 4) {

                int blockIndex = r * size.dim.width() / (blockSize * blockSize) + c / blockSize;
                QColor color = size.blockColors.at(blockIndex);

                int imageIndex = (size.dim.height() - 1 - r) * size.dim.width() + c;
                QRgb decompressedColor = cwOpenGLUtils::toQRgba(image.at(imageIndex));

                INFO("BlockIndex:" << blockIndex << " ImageIndex:" << imageIndex << " Image block index:" << imageIndex / 16);
                INFO("Color:" << color.name().toStdString()
                     << " DecompressedColor:" << QColor(decompressedColor).name().toStdString());
                CHECK(cwOpenGLUtils::fuzzyCompareColors(color, decompressedColor) <= 16);
            }
        }
    }
}

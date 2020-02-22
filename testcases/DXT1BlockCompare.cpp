//Catch includes
#include <catch.hpp>

//Our includes
#include "TestHelper.h"
#include "DXT1BlockCompare.h"
#include "cwImageData.h"

//Stc3 decompression
#include "s3tc-dxt-decompression/s3tc.h"

void DXT1BlockCompare::compare(const DXT1BlockCompare::TestImage &size, const cwImageData &mipmap)
{
    CHECK(mipmap.size() == size.dim);
    CHECK(mipmap.data().size() == size.bytes);

    if(!size.blockColors.isEmpty()) {
        //DXT1 decompression makes this assumption
        REQUIRE(sizeof(long) == sizeof(int));

        QVector<unsigned long> image(size.dim.width() * size.dim.height(), 0);

        BlockDecompressImageDXT1(size.dim.width(), size.dim.height(),
                                 reinterpret_cast<const unsigned char*>(mipmap.data().constData()), image.data());

        auto toQRgba = [](unsigned long color) {
            //Get shifty
            int r = (color & 0xFF000000) >> 24;
            int g = (color & 0x00FF0000) >> 16;
            int b = (color & 0x0000FF00) >> 8;
            int a = (color & 0x000000FF);
            return qRgba(r, g, b, a);
        };

        //Does sum of the square differenc between color changes
        auto fuzzyCompareColors = [](QColor c1, QColor c2) {
            QVector<int> channelDiff = {
                c1.red() - c2.red(),
                c1.green() - c2.green(),
                c1.blue() - c2.blue(),
                c1.alpha() - c2.alpha()
            };
            std::transform(channelDiff.begin(), channelDiff.end(), channelDiff.begin(), [](int value) { return value * value; });
            int sum = std::accumulate(channelDiff.begin(), channelDiff.end(), 0);
            CHECK(sum <= 10); //This might have to be increased for GPU compression?
        };

        //Checks that the add images dxt1 compression is working okay, if this breaks, then
        //compression isn't working
        int blockSize = 4;
        for(int c = 0; c < size.dim.width(); c += 4) {
            for(int r = 0; r < size.dim.height(); r += 4) {

                int blockIndex = r * size.dim.width() / (blockSize * blockSize) + c / blockSize;
                QColor color = size.blockColors.at(blockIndex);

                int imageIndex = (size.dim.height() - 1 - r) * size.dim.width() + c;
                QRgb decompressedColor = toQRgba(image.at(imageIndex));

                INFO("BlockIndex:" << blockIndex << " ImageIndex:" << imageIndex);
                INFO("Color:" << color.name().toStdString() << " DecompressedColor:" << QColor(decompressedColor).name().toStdString());
                fuzzyCompareColors(color, decompressedColor);
            }
        }
    }
}

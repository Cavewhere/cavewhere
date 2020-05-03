#ifndef DXT1BLOCKCOMPARE_H
#define DXT1BLOCKCOMPARE_H

//Qt includes
#include <QSize>
#include <QColor>
#include <QVector>

//Our includes
class cwImageData;

class DXT1BlockCompare
{
public:
    class TestImage {
    public:
        TestImage() {}
        TestImage(QSize dim, int bytes, QVector<QColor> colors) :
            dim(dim),
            bytes(bytes),
            blockColors(colors)
        {

        }

        QSize dim;
        int bytes;
        QVector<QColor> blockColors;

    };

    DXT1BlockCompare() = delete;

    static void compare(const TestImage& size, const cwImageData& mipmap);

};

#endif // DXT1BLOCKCOMPARE_H

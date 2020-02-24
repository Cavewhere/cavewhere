//Our includes
#include "cwDXT1Compresser.h"
#include "cwDebug.h"
#include "cwAsyncFuture.h"

//Qt includes
#include <QPoint>
#include <QGLWidget>
#include <QtConcurrent>
#include <QOffscreenSurface>
#include <QOpenGLFunctions_2_1>

//For creating compressed DXT texture maps
#include <squish.h>

//Async future
#include <asyncfuture.h>

cwDXT1Compresser::cwDXT1Compresser(QObject *parent) :
    QObject(parent)
{
}

cwDXT1Compresser::~cwDXT1Compresser()
{

}

QFuture<cwDXT1Compresser::CompressedImage> cwDXT1Compresser::compress(const QList<QImage> &images)
{
    //FIXME: This should be replace with the current computer setting for compression
    return squishCompression(images, true);
//        return openglCompression(images, true);
}


QFuture<cwDXT1Compresser::CompressedImage> cwDXT1Compresser::openglCompression(const QList<QImage> &images, bool threaded)
{
    auto compressImages = [images]() {
        QOffscreenSurface* surface = nullptr;
        auto createSurface = [&surface]() {
            surface = new QOffscreenSurface();
            surface->create();
        };

        if(QThread::currentThread() == QCoreApplication::instance()->thread()) {
            createSurface();
        } else {
            QMetaObject::invokeMethod(QCoreApplication::instance(), createSurface, Qt::BlockingQueuedConnection);
        }

        auto compressionContext = std::make_unique<QOpenGLContext>();
        compressionContext->create();
        bool contextOkay = compressionContext->makeCurrent(surface);

        Q_ASSERT(contextOkay);

        OpenGLCompresser compresser;
        compresser.initializeOpenGLFunctions();

        QList<cwDXT1Compresser::CompressedImage> compressedImages;
        for(auto image : images) {
            compressedImages.append(compresser.openglDxt1Compression(image));
        }

        compressionContext->doneCurrent();
        QMetaObject::invokeMethod(surface, [surface](){surface->deleteLater();});

        return compressedImages;
    };

    QFuture<CompressedImage> future;
    if(QOpenGLContext::supportsThreadedOpenGL() && threaded) {
        auto concurrentFuture = QtConcurrent::run(compressImages);

        //This converts QList<CompressImage> to a future with the results in it, that can be iterated
        future = AsyncFuture::observe(concurrentFuture).context(this, [concurrentFuture](){
            return AsyncFuture::completed(concurrentFuture.result());
        }).future();
    } else if(QOpenGLContext::supportsThreadedOpenGL() || thread() == QCoreApplication::instance()->thread()){
        future = AsyncFuture::completed(compressImages());
    } else {
        QList<cwDXT1Compresser::CompressedImage> images;
        QMetaObject::invokeMethod(QCoreApplication::instance(), compressImages, Qt::BlockingQueuedConnection, &images);
        future = AsyncFuture::completed(images);
    }

    return future;
}

QFuture<cwDXT1Compresser::CompressedImage> cwDXT1Compresser::squishCompression(const QList<QImage> &images, bool threaded)
{
    std::function<cwDXT1Compresser::CompressedImage (const QImage&)> compress = [](const QImage& image) {
          return squishCompressImageThreaded(image, squish::kDxt1 | squish::kColourIterativeClusterFit);
    };

    if(threaded) {
        return QtConcurrent::mapped(images, compress);
    }

    QList<cwDXT1Compresser::CompressedImage> compressedImages;
    std::transform(images.begin(), images.end(), std::back_inserter(compressedImages), compress);
    return AsyncFuture::completed(compressedImages);
}

QList<cwDXT1Compresser::CompressedImage> cwDXT1Compresser::results(QFuture<cwDXT1Compresser::CompressedImage> future) const
{
    cwAsyncFuture::waitForFinished(future);
    return future.results();
}

using namespace squish;

/**
  This block is create to be compressed by squish in a thread way.

  See sqiush::CompressMask for details
  */
class Block {
public:

    /**
      \param x - The x parameter
      \param y - The y parameter
      \param rgba - The source rgba
      \param blockData - The output blockdata
      */
    Block(int x, int y, u8 const* rgba, void* blockData) {
        Position = QPoint(x, y);
        RGBA = rgba;
        BlockData = blockData;
    }

    QPoint Position;
    u8 const* RGBA;
    void* BlockData;
};

/**
  \brief This class compresses a signle block.  This allow squish library to be
  threaded.
  */
class CompressImageKernal {
public:
    CompressImageKernal(QSize imageSize, int flags, float* metric) {
        ImageSize = imageSize;
        Flags = flags;
        Metric = metric;
    }

    QSize ImageSize;
    int Flags;
    float* Metric;


    void operator()(Block block) {
        // build the 4x4 block of pixels
        u8 sourceRgba[16*4];
        u8* targetPixel = sourceRgba;
        int mask = 0;
        for( int py = 0; py < 4; ++py )
        {
            for( int px = 0; px < 4; ++px )
            {
                // get the source pixel in the image
                int sx = block.Position.x() + px;
                int sy = block.Position.y() + py;

                // enable if we're in the image
                if( sx < ImageSize.width() && sy < ImageSize.height() )
                {
                    // copy the rgba value
                    u8 const* sourcePixel = block.RGBA + 4*( ImageSize.width() * sy + sx );
                    for( int i = 0; i < 4; ++i ) {
                        *targetPixel++ = *sourcePixel++;
                    }

                    // enable this pixel
                    mask |= ( 1 << ( 4*py + px ) );
                }
                else
                {
                    // skip this pixel as its outside the image
                    targetPixel += 4;
                }
            }
        }

        CompressMasked(sourceRgba, mask, block.BlockData, Flags);
    }

};



/**
  Copied directly from squish library
  */

static int FixFlags( int flags )
{
    // grab the flag bits
    int method = flags & ( kDxt1 | kDxt3 | kDxt5 );
    int fit = flags & ( kColourIterativeClusterFit | kColourClusterFit | kColourRangeFit );
    int extra = flags & kWeightColourByAlpha;

    // set defaults
    if( method != kDxt3 && method != kDxt5 )
        method = kDxt1;
    if( fit != kColourRangeFit && fit != kColourIterativeClusterFit )
        fit = kColourClusterFit;

    // done
    return method | fit | extra;
}

/**
  \brief This is a drop in replacement for sqiush::CompressImage

  The only differance is this is threaded.
  */
cwDXT1Compresser::CompressedImage cwDXT1Compresser::squishCompressImageThreaded( QImage image, int flags, float* metric ) {
    int outputFileSize = squish::GetStorageRequirements(image.width(), image.height(), squish::kDxt1);

    //Allocate the compress data
    QByteArray outputData;
    outputData.resize(outputFileSize);

    //Convert the image to a real format
    QImage convertedFormat = QGLWidget::convertToGLFormat(image);

    // fix any bad flags
    flags = FixFlags( flags );

    // initialise the block output
    u8* targetBlock = reinterpret_cast< u8* >( outputData.data() );
    int bytesPerBlock = ( ( flags & kDxt1 ) != 0 ) ? 8 : 16;

    // loop over pixels and create blocks
    QList<Block> computeBlocks;
    for( int y = 0; y < image.height(); y += 4 )
    {
        for( int x = 0; x < image.width(); x += 4 )
        {
            computeBlocks.append(Block(x, y, convertedFormat.bits(), targetBlock));

            // advance
            targetBlock += bytesPerBlock;
        }
    }

    //This takes all the compute blocks and compresses them using squish
    QtConcurrent::blockingMap(computeBlocks, CompressImageKernal(image.size(), flags, metric));
//    std::for_each(computeBlocks.begin(), computeBlocks.end(), CompressImageKernal(image.size(), flags, metric));

    return CompressedImage(outputData, image.size());
}

/**
 * @brief cwAddImageTask::graphicsDriverDx1Compression
 * @param image - The image to compress
 * @return Returns the compress image in DXT1 compression
 *
 * This assumes that the opengl context is bound
 */
//#ifndef Q_OS_WIN
cwDXT1Compresser::CompressedImage cwDXT1Compresser::OpenGLCompresser::openglDxt1Compression(QImage image)
{
    GLuint texture;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    QImage convertedImage = QGLWidget::convertToGLFormat(image);
    CompressedImage outputImage;

    glTexImage2D(GL_TEXTURE_2D,
                 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                 image.width(), image.height(),
                 0, GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 convertedImage.bits());


    GLint compressed;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_ARB, &compressed);
    // if the compression has been successful
    if (compressed == GL_TRUE)
    {
        //        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT,
        //                                 &internalformat);
        GLint compressed_size;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB,
                                 &compressed_size);
        QByteArray compressedByteArray(compressed_size, 0);
        glGetCompressedTexImage(GL_TEXTURE_2D, 0, compressedByteArray.data());

        outputImage = CompressedImage(compressedByteArray, image.size());
    }

    glDeleteTextures(1, &texture);

    return outputImage;
}
//#endif

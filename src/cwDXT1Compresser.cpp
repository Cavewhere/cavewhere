//Our includes
#include "cwDXT1Compresser.h"
#include "cwDebug.h"
#include "cwAsyncFuture.h"
#include "cwOpenGLUtils.h"
#include "cwOpenGLSettings.h"

//Qt includes
#include <QPoint>
#include <QGLWidget>
#include <QtConcurrent>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
#include <QVector>

//For creating compressed DXT texture maps
#include <squish.h>

//Async future
#include <asyncfuture.h>

cwDXT1Compresser::cwDXT1Compresser()
{
}

cwDXT1Compresser::~cwDXT1Compresser()
{

}

QFuture<cwDXT1Compresser::CompressedImage> cwDXT1Compresser::compress(const QList<QImage> &images)
{
    switch(cwOpenGLSettings::instance()->dxt1Algorithm()) {
    case cwOpenGLSettings::DXT1_GPU:
        return openglCompression(images, true);
    case cwOpenGLSettings::DXT1_Squish:
        return squishCompression(images, true);
    }

    return squishCompression(images, true);
}


QFuture<cwDXT1Compresser::CompressedImage> cwDXT1Compresser::openglCompression(const QList<QImage> &images, bool threaded)
{
    Q_ASSERT(context()->thread() == QThread::currentThread());

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
        future = AsyncFuture::observe(concurrentFuture).context(context(), [concurrentFuture](){
            return AsyncFuture::completed(concurrentFuture.result());
        }).future();
    } else if(QOpenGLContext::supportsThreadedOpenGL() || QThread::currentThread() == QCoreApplication::instance()->thread()){
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
    Q_ASSERT(context()->thread() == QThread::currentThread());

    auto context = this->context();

    std::function<QFuture<cwDXT1Compresser::CompressedImage> (const QImage&)> compress =
            [context](const QImage& image)
    {
          return squishCompressImageThreaded(image,
                                             context,
                                             squish::kDxt1 | squish::kColourIterativeClusterFit);
    };

    auto futureResults = [context](const QList<QFuture<cwDXT1Compresser::CompressedImage>>& imageFutures) {
        auto compressFutures = AsyncFuture::combine() << imageFutures;

        return AsyncFuture::observe(compressFutures.future())
                .context(context, [imageFutures]()
        {
            QList<cwDXT1Compresser::CompressedImage> compressedImages;
            compressedImages.reserve(imageFutures.size());
            for(auto imageResultFuture : imageFutures) {
                Q_ASSERT(imageResultFuture.isFinished());
                compressedImages.append(imageResultFuture.result());
            }
            return AsyncFuture::completed(compressedImages);
        }).future();
    };

    if(threaded) {
        auto compressFuture = QtConcurrent::mapped(images, compress);

        return AsyncFuture::observe(compressFuture)
                .context(context, [compressFuture, futureResults]()
        {
            return futureResults(compressFuture.results());
        }
        ,
        //Cancel all futures
        [compressFuture]() {
            for(auto future : compressFuture.results()) {
                future.cancel();
            }
        }
        ).future();
    }

    QList<QFuture<cwDXT1Compresser::CompressedImage>> compressedImages;
    std::transform(images.begin(), images.end(), std::back_inserter(compressedImages), compress);
    return futureResults(compressedImages);
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

    Block() {}
    /**
      \param x - The x parameter
      \param y - The y parameter
      \param rgba - The source rgba
      \param blockData - The output blockdata
      */
    Block(int x, int y, const QImage& rgbaImage, void* blockData) :
        image(rgbaImage)
    {
        Position = QPoint(x, y);
        RGBA = image.constBits();
        BlockData = blockData;
    }

    QPoint Position;
    const QImage image;
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
QFuture<cwDXT1Compresser::CompressedImage> cwDXT1Compresser::squishCompressImageThreaded( QImage image,
                                                                                 QObject* context,
                                                                                 int flags,
                                                                                 float* metric) {
    int outputFileSize = squish::GetStorageRequirements(image.width(), image.height(), squish::kDxt1);

    //Allocate the compress data
    QByteArray outputData;
    outputData.resize(outputFileSize);

    //Convert the image to a real format
    QImage convertedFormat = cwOpenGLUtils::toGLTexture(image);

    // fix any bad flags
    flags = FixFlags( flags );

    // initialise the block output
    u8* targetBlock = reinterpret_cast< u8* >( outputData.data() );
    int bytesPerBlock = ( ( flags & kDxt1 ) != 0 ) ? 8 : 16;

    // loop over pixels and create blocks
    QVector<Block> computeBlocks;
    computeBlocks.reserve((image.height() / 4 + 1) * (image.width() / 4 + 1) );
    for( int y = 0; y < image.height(); y += 4 )
    {
        for( int x = 0; x < image.width(); x += 4 )
        {
            computeBlocks.append(Block(x, y, convertedFormat, targetBlock));

            // advance
            targetBlock += bytesPerBlock;
        }
    }

    //This takes all the compute blocks and compresses them using squish
    auto blockFuture = QtConcurrent::map(computeBlocks, CompressImageKernal(image.size(), flags, metric));

    return AsyncFuture::observe(blockFuture)
            .context(context, [outputData, image, computeBlocks]() {
        return CompressedImage(outputData, image.size());
    }).future();
}

/**
 * @brief cwAddImageTask::graphicsDriverDx1Compression
 * @param image - The image to compress
 * @return Returns the compress image in DXT1 compression
 *
 * This assumes that the opengl context is bound
 */
cwDXT1Compresser::CompressedImage cwDXT1Compresser::OpenGLCompresser::openglDxt1Compression(QImage image)
{
    auto gl = std::make_unique<QOpenGLFunctions_2_1>();
    bool couldInit = gl->initializeOpenGLFunctions();

    if(!couldInit) {
        return CompressedImage();
    }

    GLuint texture;

    gl->glGenTextures(1, &texture);
    gl->glBindTexture(GL_TEXTURE_2D, texture);

    QImage convertedImage = cwOpenGLUtils::toGLTexture(image);
    CompressedImage outputImage;

    gl->glTexImage2D(GL_TEXTURE_2D,
                 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                 image.width(), image.height(),
                 0, GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 convertedImage.bits());


    GLint compressed;
    gl->glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_ARB, &compressed);
    // if the compression has been successful
    if (compressed == GL_TRUE)
    {
        //        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT,
        //                                 &internalformat);
        GLint compressed_size;
        gl->glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB,
                                 &compressed_size);
        QByteArray compressedByteArray(compressed_size, 0);
        gl->glGetCompressedTexImage(GL_TEXTURE_2D, 0, compressedByteArray.data());

        outputImage = CompressedImage(compressedByteArray, image.size());
    }

    gl->glDeleteTextures(1, &texture);

    return outputImage;
}

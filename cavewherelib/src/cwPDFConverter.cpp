//Our includes
#include "cwPDFConverter.h"
#include "cwDebug.h"
#include "cwUnits.h"
#include "cwConcurrent.h"

//AsyncFuture
#include "asyncfuture.h"

//Qt includes
#ifdef CW_WITH_PDF_SUPPORT
#include <QPdfDocument>
#endif

cwPDFConverter::cwPDFConverter(QObject *parent) : QObject(parent)
{

}

/**
* The path to the pdf
*/
void cwPDFConverter::setSource(QString source) {
    if(Source != source) {
        Source = source;
        emit sourceChanged();
    }
}

/**
* Sets the resolution of the export pdf. This is in ppi (like in adobe)
* To get 1 to 1 resolution set this to 72. The default is 300ppi (high resolution)
*/
void cwPDFConverter::setResolution(int resolution) {
    if(Resolution != resolution) {
        Resolution = resolution;
        emit resolutionChanged();
    }
}

/**
 * Converts each page of the pdf using QtConcurrent::mapped()
 */
QFuture<QImage> cwPDFConverter::convert()
{
#ifdef CW_WITH_PDF_SUPPORT
    QPdfDocument document;
    auto error = document.load(source());

    if(error != QPdfDocument::Error::None) {
        auto toString = [](QPdfDocument::Error error) {
            switch(error) {
            case QPdfDocument::Error::None:
                return QStringLiteral("No Error");
            case QPdfDocument::Error::Unknown:
                return QStringLiteral("Unknown Error :(");
            case QPdfDocument::Error::DataNotYetAvailable:
                return QStringLiteral("Data Not yet available error");
            case QPdfDocument::Error::FileNotFound:
                return QStringLiteral("File not found");
            case QPdfDocument::Error::InvalidFileFormat:
                return QStringLiteral("Invalid file format");
            case QPdfDocument::Error::IncorrectPassword:
                return QStringLiteral("Incorrect password");
            case QPdfDocument::Error::UnsupportedSecurityScheme:
                return QStringLiteral("Unsupported Security scheme");
            default:
                return QStringLiteral("Unhandled error, this is a bug :(");
            }
        };

        setError("PDF Renderer error: " + toString(error));
        return QFuture<QImage>();
    }

    QVector<int> pages(document.pageCount());
    std::iota(pages.begin(), pages.end(), 0);

    QString path = source();
    int resolution = this->resolution();

    std::function<QImage (int page)> convertPage = [path, resolution](int page)->QImage {
        QPdfDocument doc;
        doc.load(path);
        auto size = doc.pagePointSize(page);

        auto scaledSize = size * (1.0 / cwUnits::PointsPerInch) * resolution;
        QImage image = doc.render(page, QSize(std::round(scaledSize.width()),
                                              std::round(scaledSize.height())));
        return image;
    };

    return cwConcurrent::mapped(pages, convertPage);
#else
    setError("PDF Renderer not enabled");
    return QFuture<QImage>();
#endif
}

void cwPDFConverter::setError(const QString &errorMessage)
{
    if(ErrorMessage != errorMessage) {
        ErrorMessage = errorMessage;
        emit errorChanged();
    }
}

/**
* Returns true if the converter will work, and falso if it doesn't
*/
bool cwPDFConverter::isSupported()  {
#ifdef CW_WITH_PDF_SUPPORT
    return true;
#else
    return false;
#endif
}

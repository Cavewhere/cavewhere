//Qt includes
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QSurfaceFormat>
#include <rhi/qrhi.h>

//CaveWhere
#include "cwTask.h"

QList<int> supportedSampleCounts() {
#if QT_CONFIG(vulkan)
    QVulkanInstance inst;
#endif
    std::unique_ptr<QRhi> rhi;
#if defined(Q_OS_WIN)
    QRhiD3D12InitParams params;
    rhi.reset(QRhi::create(QRhi::D3D12, &params));
#elif QT_CONFIG(metal)
    QRhiMetalInitParams params;
    rhi.reset(QRhi::create(QRhi::Metal, &params));
#elif QT_CONFIG(vulkan)
    inst.setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
    if (inst.create()) {
        QRhiVulkanInitParams params;
        params.inst = &inst;
        rhi.reset(QRhi::create(QRhi::Vulkan, &params));
    } else {
        qFatal("Failed to create Vulkan instance");
    }
#endif
    if (rhi) {
        qDebug() << rhi->backendName() << rhi->driverInfo();
    } else {
        qFatal("Failed to initialize RHI");
    }

    return rhi->supportedSampleCounts();
};

int sampleCount() {
    auto supported = supportedSampleCounts();
    int wantedSampleCount = 4; //MSAA 4x4
    if(supported.contains(wantedSampleCount)) {
        return wantedSampleCount;
    } else if (!supported.isEmpty()) {
        //Assume that it's sorted
        return supported.last();
    } else {
        return 1;
    }
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    cwTask::initilizeThreadPool();

    QSurfaceFormat format;
    format.setSamples(sampleCount()); // Adjust the sample count as needed
    QSurfaceFormat::setDefaultFormat(format);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    // QString resourcePath = QCoreApplication::applicationDirPath() + "/../../../";
    // qDebug() << "resourcePath:" << resourcePath;
    // engine.addImportPath(resourcePath);

    engine.loadFromModule("CaveWhereSketch", "Main");

    return app.exec();
}

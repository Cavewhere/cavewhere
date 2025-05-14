#include "cwMetaTypeSystem.h"

//Qt includes
#include <QMetaType>

//Our includes
#include "cwCompassReading.h"
#include "cwClinoReading.h"
#include "cwDistanceReading.h"

void cwMetaTypeSystem::registerTypes()
{

    auto readingToString = [](const cwReading& reading) {
        return reading.value();
    };

    int success;
    success = QMetaType::registerConverter<cwCompassReading, QString>(readingToString);
    Q_ASSERT(success);
    success = QMetaType::registerConverter<cwDistanceReading, QString>(readingToString);
    Q_ASSERT(success);
    success = QMetaType::registerConverter<cwClinoReading, QString>(readingToString);
    Q_ASSERT(success);
    success = QMetaType::registerConverter<cwReading, QString>(readingToString);
    Q_ASSERT(success);

    //Duck typed conversion
    auto readingToDouble = [](const auto& reading)->std::optional<double> {
        //ReadingType should be cwCompassReading, cwDistanceReading, or cwClinoReading
        using ReadingType = std::remove_cv_t<std::remove_reference_t<decltype(reading)>>;
        if(reading.state() == ReadingType::State::Valid) {
            return reading.toDouble();
        }
        return std::nullopt;
    };

    success = QMetaType::registerConverter<cwCompassReading, double>(readingToDouble);
    Q_ASSERT(success);

    success = QMetaType::registerConverter<cwDistanceReading, double>(readingToDouble);
    Q_ASSERT(success);

    success = QMetaType::registerConverter<cwClinoReading, double>(readingToDouble);
    Q_ASSERT(success);

    success = QMetaType::registerConverter<cwReading, double>(&cwReading::toDouble);
    Q_ASSERT(success);

}

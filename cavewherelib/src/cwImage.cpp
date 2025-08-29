/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwImage.h"
#include "cwProject.h"

//Qt includes
#include <QDir>
#include <QDebug>
#include <QVector2D>
#include <QVector>

bool cwImage::isMipmapsValid() const {
    if (!std::holds_alternative<IdData>(m_data->modeData)) {
        return false;
    }
    const auto& mipmapsList = std::get<IdData>(m_data->modeData).mipmapIds;
    if (mipmapsList.isEmpty()) {
        return false;
    }
    return std::all_of(mipmapsList.begin(), mipmapsList.end(), [](int id) {
        return isIdValid(id);
    });
}

QDebug operator<<(QDebug debug, const cwImage &image)
{
    QDebugStateSaver saver(debug);
    if(image.mode() == cwImage::Mode::Ids) {
        debug.nospace() << "original:" << image.original() << " icon:" << image.icon() << " mipmaps:" << image.mipmaps() << " size:" << image.originalSize() << " dotPerMeter:" << image.originalDotsPerMeter();;
    } else if(image.mode() == cwImage::Mode::Path) {
        debug.nospace() << "path:" << image.path() << " size:" << image.originalSize() << " dotPerMeter:" << image.originalDotsPerMeter();
    } else {
        debug.nospace() << "cwImage isn't valid";
    }
    return debug;
}


bool cwImage::operator==(const cwImage &other) const {
    if (m_data == other.m_data) return true;
    if (!m_data || !other.m_data) return false;

    if (m_data->originalSize != other.m_data->originalSize ||
        m_data->originalDotsPerMeter != other.m_data->originalDotsPerMeter) {
        return false;
    }

    if (m_data->modeData.index() != other.m_data->modeData.index()) return false;

    return m_data->modeData == other.m_data->modeData;
}


QList<int> cwImage::ids() const {
    if (!std::holds_alternative<IdData>(m_data->modeData)) return {};
    const auto& data = std::get<IdData>(m_data->modeData);
    QList<int> out;
    out.reserve(2 + data.mipmapIds.size());
    if (isIdValid(data.originalId)) { out.append(data.originalId); }
    if (isIdValid(data.iconId)) { out.append(data.iconId); }
    out.append(data.mipmapIds);
    return out;
}

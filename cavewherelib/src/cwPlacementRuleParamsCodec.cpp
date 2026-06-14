/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPlacementRuleParamsCodec.h"
#include "cwPlacementRuleParams.h"
#include "cavewhere_symbology.pb.h"

QVariant cwPlacementRuleParamsCodec::decode(const QString &name, const QByteArray &params)
{
    if (params.isEmpty()) {
        return QVariant();
    }

    if (name == cwUniformSpacingRuleName()) {
        CavewhereSymbologyProto::UniformSpacingParams proto;
        if (proto.ParseFromArray(params.constData(), static_cast<int>(params.size()))) {
            cwUniformSpacingParams out;
            if (proto.has_spacingmm()) {
                out.spacingMm = proto.spacingmm();
            }
            return QVariant::fromValue(out);
        }
        // Malformed bytes for a known rule: preserve verbatim so a re-save
        // doesn't destroy data we couldn't parse.
        return QVariant::fromValue(params);
    }

    // Unknown rule: keep the opaque bytes so it round-trips intact.
    return QVariant::fromValue(params);
}

QByteArray cwPlacementRuleParamsCodec::encode(const QString &name, const QVariant &params)
{
    if (!params.isValid()) {
        return QByteArray();
    }

    // Passthrough for opaque bytes (unknown rules, malformed-but-preserved blobs).
    if (params.typeId() == QMetaType::QByteArray) {
        return params.toByteArray();
    }

    if (name == cwUniformSpacingRuleName()
        && params.typeId() == qMetaTypeId<cwUniformSpacingParams>()) {
        const cwUniformSpacingParams in = params.value<cwUniformSpacingParams>();
        CavewhereSymbologyProto::UniformSpacingParams proto;
        proto.set_spacingmm(in.spacingMm);
        const std::string bytes = proto.SerializeAsString();
        return QByteArray(bytes.data(), static_cast<int>(bytes.size()));
    }

    return QByteArray();
}

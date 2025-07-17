// #include "cwDiff.h"

// cwDiff::cwDiff() {}

#include <google/protobuf/message.h>
#include <google/protobuf/util/message_differencer.h>
#include "cwDiff.h"

namespace cwDiff {


// Helper to pull out the 'id' key from a submessage.
static std::string extractId(
    const google::protobuf::Message& msg,
    const google::protobuf::FieldDescriptor* idField)
{
    auto const* refl = msg.GetReflection();
    return refl->GetString(msg, idField);
}


struct RepeatParameters {
    const google::protobuf::Message& base;
    const google::protobuf::Message& ours;
    const google::protobuf::Message& theirs;
    MergeStrategy strategy;
    const google::protobuf::FieldDescriptor* field;
    google::protobuf::Message* result;
};

template<typename EqualsFunc>
struct Compare {
    EqualsFunc equals;

    //Merge equal Messages with equal ids. If the equals function is true where the
    //ids are the same, the data in the Message may be different, this cause
    //mergeRepeated to step into the Message and merge them. The ids must be
    //independantly generated so UUID is a good choice.
    bool deepMerge = false;
};

template<typename EqualsFunc>
Compare<EqualsFunc> makeCompare(EqualsFunc equals, bool deepMerge) {
    return Compare<EqualsFunc>{equals, deepMerge};
}

template<typename RepeatedToVectorFunc, typename AddFunc, typename CompareType>
void mergeRepeated(
    const RepeatParameters& p,
    RepeatedToVectorFunc repeatedToVector,
    AddFunc add,
    const CompareType& compare)
{
    auto const* desc        = p.base.GetDescriptor();
    auto const* reflBase    = p.base.GetReflection();
    auto const* reflOurs    = p.ours.GetReflection();
    auto const* reflTheirs  = p.theirs.GetReflection();
    auto      * reflResult  = p.result->GetReflection();
    auto const* subMsgDesc  = p.field->message_type();
    // auto const* idFieldDesc = subMsgDesc->FindFieldByName("id");

    qDebug() << "------";

    auto baseSubs   = repeatedToVector(p.base);
    auto oursSubs   = repeatedToVector(p.ours);
    auto theirsSubs = repeatedToVector(p.theirs);

    using Type = typename decltype(baseSubs)::value_type;

    // diff scripts
    auto oursEdits   = cwDiff::diff<Type>(baseSubs, oursSubs, compare.equals);
    auto theirsEdits = cwDiff::diff<Type>(baseSubs, theirsSubs, compare.equals);

    //replay exactly like mergeList, but Copy/Merge messages
    size_t baseIdx = 0, oursIdx = 0, theirsIdx = 0;
    size_t oursE = 0, theirsE = 0;
    while (baseIdx < baseSubs.size()
           || oursE < oursEdits.size()
           || theirsE < theirsEdits.size())
    {
        bool oursDel = (oursE < oursEdits.size()
                        && oursEdits[oursE].operation == cwDiff::EditOperation::Delete
                        && oursEdits[oursE].positionOld == baseIdx);
        bool theirsDel = (theirsE < theirsEdits.size()
                          && theirsEdits[theirsE].operation == cwDiff::EditOperation::Delete
                          && theirsEdits[theirsE].positionOld == baseIdx);
        bool oursInserted = (oursE < oursEdits.size()
                        && oursEdits[oursE].operation == cwDiff::EditOperation::Insert
                        && oursEdits[oursE].positionOld == baseIdx);
        bool theirsInserted = (theirsE < theirsEdits.size()
                          && theirsEdits[theirsE].operation == cwDiff::EditOperation::Insert
                          && theirsEdits[theirsE].positionOld == baseIdx);

        qDebug() << "BaseIdx:" << baseIdx << " oursE:" << oursE << " thiersE:" << theirsE;
        qDebug() << "OursIdx:" << oursIdx << " thiersIdx:" << theirsIdx;
        qDebug() << "OursDel:" << oursDel << " ThiersDel:" << theirsDel << " OursInserted:" << oursInserted << " theirsInserted:" << theirsInserted;

        // both inserted → conflict or identical
        if (oursInserted && theirsInserted) {
            auto const& oursSub = oursSubs[oursEdits[oursE].positionNew];
            auto const& thiersSub = theirsSubs[theirsEdits[theirsE].positionNew];
            if (compare.equals(oursSub, thiersSub)) {

                //They are the same, just add ours
                if constexpr (std::is_same_v<std::remove_cv_t<std::remove_pointer_t<Type>>, google::protobuf::Message>) {
                    //Merge the message
                    qDebug() << "ours:" << oursSub->DebugString();
                    qDebug() << "theirs: " << thiersSub->DebugString();
                    qDebug() << "Equals: " << compare.equals(oursSub, thiersSub);

                    if(compare.deepMerge) {
                        auto merged = mergeMessageByReflection(*baseSubs[baseIdx],
                                                               *oursSub,
                                                               *thiersSub,
                                                               p.strategy);
                        add(*merged);
                    } else {
                        add(*oursSub);
                    }
                } else {
                    //Merge the scalar
                    if(compare.deepMerge) {
                        auto merged = merge(baseSubs[baseIdx],
                                            oursSub,
                                            thiersSub,
                                            p.strategy,
                                            compare.equals);
                        add(merged);
                    } else {
                        add(oursSub);
                    }
                }
            } else {
                // different data → append both
                if constexpr (std::is_pointer<Type>()) {
                    add(*oursSub);
                    add(*thiersSub);
                } else {
                    add(oursSub);
                    add(thiersSub);
                }
            }
            ++oursE; ++theirsE;
            continue;
        }
        if (oursInserted) {
            if constexpr(std::is_pointer<Type>()) {
                add(*oursSubs.at(oursEdits.at(oursE).positionNew));
            } else {
                add(oursSubs.at(oursEdits.at(oursE).positionNew));
            }

            ++oursE;
            continue;
        }
        if (theirsInserted) {
            if constexpr(std::is_pointer<Type>()) {
                add(*theirsSubs.at(theirsEdits.at(theirsE).positionNew));
            } else {
                add(theirsSubs.at(theirsEdits.at(theirsE).positionNew));
            }

            ++theirsE;
            continue;
        }

        // deletions
        if (oursDel && theirsDel) {
            ++baseIdx; ++oursE; ++theirsE;
            ++oursIdx; ++theirsIdx; //I'm not sure if this is correct, might need to do more testing
            qDebug() << "Both deleted!";
            continue;
        }
        if (oursDel && !theirsDel) {
            // theirs kept?
            // if (extractId(*baseSubs[baseIdx], idFieldDesc)
            //     != extractId(*theirsSubs[theirsIdx], idFieldDesc)) {
            if(!compare.equals(baseSubs.at(baseIdx), theirsSubs.at(theirsIdx))) {
                if (p.strategy != MergeStrategy::UseTheirsOnConflict) {
                    if constexpr(std::is_pointer<Type>()) {
                        add(*theirsSubs.at(theirsIdx));
                    } else {
                        add(theirsSubs.at(theirsIdx));
                    }

                    // auto* dst = reflResult->AddMessage(result, field);
                    // dst->CopyFrom(*theirsSubs[theirsIdx]);
                }
            }
            ++baseIdx; ++oursE; ++theirsIdx;
            continue;
        }
        if (!oursDel && theirsDel) {
            if(!compare.equals(baseSubs.at(baseIdx), oursSubs.at(oursIdx))) {
                // if (extractId(*baseSubs[baseIdx], idFieldDesc)
                //     != extractId(*oursSubs[oursIdx], idFieldDesc)) {
                if (p.strategy != MergeStrategy::UseOursOnConflict) {
                    if constexpr(std::is_pointer<Type>()) {
                        add(*oursSubs.at(oursIdx));
                    } else {
                        add(oursSubs.at(oursIdx));
                    }
                    // auto* dst = reflResult->AddMessage(result, field);
                    // dst->CopyFrom(*oursSubs[oursIdx]);
                }
            }
            ++baseIdx; ++theirsE; ++oursIdx;
            continue;
        }

        // unchanged in both
        if (baseIdx < baseSubs.size()) {

            if constexpr(std::is_pointer<Type>()) {
                if(compare.deepMerge) {
                    auto merged = mergeMessageByReflection(
                        *baseSubs[baseIdx],
                        *oursSubs[oursIdx],
                        *theirsSubs[theirsIdx],
                        p.strategy);

                    qDebug() << "Deep merge:" << baseIdx << oursIdx << theirsIdx;

                    add(*merged);
                } else {
                    qDebug() << "Adding ours!";
                    add(*oursSubs[oursIdx]);
                }
            } else {
                if(compare.deepMerge) {
                    auto merged = merge(
                        baseSubs[baseIdx],
                        oursSubs[oursIdx],
                        theirsSubs[theirsIdx],
                        p.strategy);

                    add(merged);
                } else {
                    add(oursSubs[oursIdx]);
                }
            }
        }

        ++baseIdx; ++oursIdx; ++theirsIdx;
        if (oursDel) ++oursE;
        if (theirsDel) ++theirsE;
    }
}

template<typename Compare>
void mergeRepeatedMessage(const RepeatParameters& p,
                          const Compare& fieldEqualsFunc)
{
    // collect sub-messages in vectors
    auto collectSubs = [&](const google::protobuf::Message& message){
        const auto& reflBase = p.base.GetReflection();

        std::vector<const google::protobuf::Message*> v;
        int n = reflBase->FieldSize(message, p.field);
        v.reserve(n);
        for (int i = 0; i < n; ++i) {
            v.push_back(&reflBase->GetRepeatedMessage(message, p.field, i));
        }
        return v;
    };

    auto addMessage = [&](const google::protobuf::Message& message) {
        auto      * reflResult  = p.result->GetReflection();
        auto* dst = reflResult->AddMessage(p.result, p.field);
        dst->CopyFrom(message);
    };

    mergeRepeated(
        p,
        collectSubs,
        addMessage,
        fieldEqualsFunc
        );
}

template<typename T, typename GetRepeatedFunc, typename AddRepeatedFunc>
void mergeRepeatedTypedScaler(RepeatParameters& p,
                              GetRepeatedFunc getRepeated,
                              AddRepeatedFunc add)
{
    // collect sub-messages in vectors
    auto collectSubs = [&](const google::protobuf::Message& message){
        const auto& reflBase = p.base.GetReflection();

        std::vector<T> v;
        int n = reflBase->FieldSize(message, p.field);
        v.reserve(n);
        for (int i = 0; i < n; ++i) {
            v.push_back(getRepeated(reflBase, message, p.field, i));
            // v.push_back(reflBase->GetRepeatedInt32(message, p.field, i));
        }
        return v;
    };

    auto addValue = [&](T value) {
        auto* reflResult  = p.result->GetReflection();
        add(reflResult, p.result, p.field, value);
        // reflResult->AddInt32(result, field, value);
    };

    mergeRepeated(p,
                  collectSubs,
                  addValue,
                  makeCompare(std::equal_to<T>(), false));
}


void mergeRepeatedScalar(RepeatParameters& p)
{
    switch (p.field->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32: {

        auto get = [](const google::protobuf::Reflection* reflection,
                      const google::protobuf::Message& message,
                      const google::protobuf::FieldDescriptor* field,
                      int index)
        {
            return reflection->GetRepeatedInt32(message, field, index);
        };

        auto add = [](const google::protobuf::Reflection* reflection,
                      google::protobuf::Message* message,
                      const google::protobuf::FieldDescriptor* field,
                      int32_t value)
        {
            reflection->AddInt32(message, field, value);
        };

        mergeRepeatedTypedScaler<int32_t>(p, get, add);
        break;
    }

    case google::protobuf::FieldDescriptor::CPPTYPE_INT64: {

        auto get = [](const google::protobuf::Reflection* reflection,
                      const google::protobuf::Message& message,
                      const google::protobuf::FieldDescriptor* field,
                      int index)
        {
            return reflection->GetRepeatedInt64(message, field, index);
        };

        auto add = [](const google::protobuf::Reflection* reflection,
                      google::protobuf::Message* message,
                      const google::protobuf::FieldDescriptor* field,
                      int64_t value)
        {
            reflection->AddInt64(message, field, value);
        };

        mergeRepeatedTypedScaler<int64_t>(p, get, add);
        break;
    }

    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32: {
        auto get = [](const google::protobuf::Reflection* reflection,
                      const google::protobuf::Message& message,
                      const google::protobuf::FieldDescriptor* field,
                      int index)
        {
            return reflection->GetRepeatedUInt32(message, field, index);
        };

        auto add = [](const google::protobuf::Reflection* reflection,
                      google::protobuf::Message* message,
                      const google::protobuf::FieldDescriptor* field,
                      uint32_t value)
        {
            reflection->AddUInt32(message, field, value);
        };

        mergeRepeatedTypedScaler<uint32_t>(p, get, add);
        break;
    }

    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64: {
        auto get = [](const google::protobuf::Reflection* reflection,
                      const google::protobuf::Message& message,
                      const google::protobuf::FieldDescriptor* field,
                      int index)
        {
            return reflection->GetRepeatedUInt64(message, field, index);
        };

        auto add = [](const google::protobuf::Reflection* reflection,
                      google::protobuf::Message* message,
                      const google::protobuf::FieldDescriptor* field,
                      uint64_t value)
        {
            reflection->AddUInt64(message, field, value);
        };

        mergeRepeatedTypedScaler<uint64_t>(p, get, add);
        break;
    }

    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE: {
        auto get = [](const google::protobuf::Reflection* reflection,
                      const google::protobuf::Message& message,
                      const google::protobuf::FieldDescriptor* field,
                      int index)
        {
            return reflection->GetRepeatedDouble(message, field, index);
        };

        auto add = [](const google::protobuf::Reflection* reflection,
                      google::protobuf::Message* message,
                      const google::protobuf::FieldDescriptor* field,
                      double value)
        {
            reflection->AddDouble(message, field, value);
        };

        mergeRepeatedTypedScaler<double>(p, get, add);
        break;
    }

    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT: {
        auto get = [](const google::protobuf::Reflection* reflection,
                      const google::protobuf::Message& message,
                      const google::protobuf::FieldDescriptor* field,
                      int index)
        {
            return reflection->GetRepeatedFloat(message, field, index);
        };

        auto add = [](const google::protobuf::Reflection* reflection,
                      google::protobuf::Message* message,
                      const google::protobuf::FieldDescriptor* field,
                      float value)
        {
            reflection->AddFloat(message, field, value);
        };

        mergeRepeatedTypedScaler<float>(p, get, add);
        break;
    }

    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL: {
        auto get = [](const google::protobuf::Reflection* reflection,
                      const google::protobuf::Message& message,
                      const google::protobuf::FieldDescriptor* field,
                      int index)
        {
            return reflection->GetRepeatedBool(message, field, index);
        };

        auto add = [](const google::protobuf::Reflection* reflection,
                      google::protobuf::Message* message,
                      const google::protobuf::FieldDescriptor* field,
                      bool value)
        {
            reflection->AddBool(message, field, value);
        };

        mergeRepeatedTypedScaler<bool>(p, get, add);
        break;
    }

    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
        auto get = [](const google::protobuf::Reflection* reflection,
                      const google::protobuf::Message& message,
                      const google::protobuf::FieldDescriptor* field,
                      int index)
        {
            return reflection->GetRepeatedEnumValue(message, field, index);
        };

        auto add = [](const google::protobuf::Reflection* reflection,
                      google::protobuf::Message* message,
                      const google::protobuf::FieldDescriptor* field,
                      int value)
        {
            reflection->AddEnumValue(message, field, value);
        };

        mergeRepeatedTypedScaler<int>(p, get, add);
        break;
    }

    case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
        auto get = [](const google::protobuf::Reflection* reflection,
                      const google::protobuf::Message& message,
                      const google::protobuf::FieldDescriptor* field,
                      int index)
        {
            return reflection->GetRepeatedString(message, field, index);
        };

        auto add = [](const google::protobuf::Reflection* reflection,
                      google::protobuf::Message* message,
                      const google::protobuf::FieldDescriptor* field,
                      const std::string& value)
        {
            reflection->AddString(message, field, value);
        };

        mergeRepeatedTypedScaler<std::string>(p, get, add);
        break;
    }
    default:
        //Unhandled case, probably the message,
        Q_ASSERT(false);
    }
}

std::unique_ptr<google::protobuf::Message> mergeMessageByReflection(const google::protobuf::Message &base,
                                                                    const google::protobuf::Message &ours,
                                                                    const google::protobuf::Message &theirs,
                                                                    cwDiff::MergeStrategy strategy)
{
    // Create an empty result of the same type
    std::unique_ptr<google::protobuf::Message> result(base.New());
    auto const* descriptor   = base.GetDescriptor();
    auto const* reflection   = base.GetReflection();
    auto* resReflection = result->GetReflection();

    for (int i = 0; i < descriptor->field_count(); ++i) {
        auto const* field = descriptor->field(i);

        // --- repeated message? dispatch to helper
        if (field->is_repeated())
        {
            RepeatParameters p {
                base, ours, theirs,
                strategy,
                field,
                result.get()
            };

            if(field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
                // if sub‐message has an “id” field → key‐merge on id
                if (field->message_type()->FindFieldByName("id")) {
                    auto idEquals = [](const google::protobuf::Message* left, const google::protobuf::Message* right) {
                        Q_ASSERT(left);
                        Q_ASSERT(right);

                        auto leftIdField = left->GetDescriptor()->FindFieldByName("id");
                        auto rightIdField = right->GetDescriptor()->FindFieldByName("id");

                        std::string leftId = left->GetReflection()->GetString(*left, leftIdField);
                        std::string rightId = right->GetReflection()->GetString(*right, rightIdField);

                        return leftId == rightId;
                    };

                    mergeRepeatedMessage(p, makeCompare(idEquals, true));
                }
                else {
                    // otherwise deep merge by all the fields
                    auto fieldEquals = [](const google::protobuf::Message* left, const google::protobuf::Message* right) {
                        Q_ASSERT(left);
                        Q_ASSERT(right);
                        return google::protobuf::util::MessageDifferencer::Equals(*left, *right);
                    };
                    mergeRepeatedMessage(p, makeCompare(fieldEquals, false));
                }
            } else {
                //Repeated basic type
                mergeRepeatedScalar(p);



            }
        }
        // --- singular message → recurse
        else if (!field->is_repeated()
                 && field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE)
        {
            if (reflection->HasField(base,   field)
                || reflection->HasField(ours,   field)
                || reflection->HasField(theirs, field))
            {
                auto const& bSub = reflection->GetMessage(base,    field);
                auto const& oSub = reflection->GetMessage(ours,    field);
                auto const& tSub = reflection->GetMessage(theirs,  field);
                auto merged = mergeMessageByReflection(bSub, oSub, tSub, strategy);
                auto* newSub = resReflection->MutableMessage(result.get(), field);
                newSub->CopyFrom(*merged);
            }
        }
        // --- scalars / enums / strings
        else {
            const bool hasBase   = reflection->HasField(base,   field);
            const bool hasOurs   = reflection->HasField(ours,   field);
            const bool hasTheirs = reflection->HasField(theirs, field);
            const bool fieldExistChanged = hasBase != hasOurs || hasBase != hasTheirs || hasBase;

            if(fieldExistChanged) {
                switch (field->cpp_type()) {
                case google::protobuf::FieldDescriptor::CPPTYPE_INT32: {

                    int32_t b = hasBase   ? reflection->GetInt32(base,   field) : 0;
                    int32_t o = hasOurs   ? reflection->GetInt32(ours,   field) : b;
                    int32_t t = hasTheirs ? reflection->GetInt32(theirs, field) : b;
                    int32_t m = merge(b, o, t, strategy);
                    resReflection->SetInt32(result.get(), field, m);
                    break;
                }

                case google::protobuf::FieldDescriptor::CPPTYPE_INT64: {
                    int64_t b = hasBase   ? reflection->GetInt64(base,   field) : 0;
                    int64_t o = hasOurs   ? reflection->GetInt64(ours,   field) : b;
                    int64_t t = hasTheirs ? reflection->GetInt64(theirs, field) : b;
                    int64_t m = merge(b, o, t, strategy);
                    resReflection->SetInt64(result.get(), field, m);
                    break;
                }

                case google::protobuf::FieldDescriptor::CPPTYPE_UINT32: {
                    uint32_t b = hasBase   ? reflection->GetUInt32(base,   field) : 0u;
                    uint32_t o = hasOurs   ? reflection->GetUInt32(ours,   field) : b;
                    uint32_t t = hasTheirs ? reflection->GetUInt32(theirs, field) : b;
                    uint32_t m = merge(b, o, t, strategy);
                    resReflection->SetUInt32(result.get(), field, m);
                    break;
                }

                case google::protobuf::FieldDescriptor::CPPTYPE_UINT64: {
                    uint64_t b = hasBase   ? reflection->GetUInt64(base,   field) : 0ull;
                    uint64_t o = hasOurs   ? reflection->GetUInt64(ours,   field) : b;
                    uint64_t t = hasTheirs ? reflection->GetUInt64(theirs, field) : b;
                    uint64_t m = merge(b, o, t, strategy);
                    resReflection->SetUInt64(result.get(), field, m);
                    break;
                }

                case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE: {
                    double b = hasBase   ? reflection->GetDouble(base,   field) : 0.0;
                    double o = hasOurs   ? reflection->GetDouble(ours,   field) : b;
                    double t = hasTheirs ? reflection->GetDouble(theirs, field) : b;
                    double m = merge(b, o, t, strategy);
                    resReflection->SetDouble(result.get(), field, m);
                    break;
                }

                case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT: {
                    float b = hasBase   ? reflection->GetFloat(base,   field) : 0.0f;
                    float o = hasOurs   ? reflection->GetFloat(ours,   field) : b;
                    float t = hasTheirs ? reflection->GetFloat(theirs, field) : b;
                    float m = merge(b, o, t, strategy);
                    resReflection->SetFloat(result.get(), field, m);
                    break;
                }

                case google::protobuf::FieldDescriptor::CPPTYPE_BOOL: {
                    bool b = hasBase   ? reflection->GetBool(base,   field) : false;
                    bool o = hasOurs   ? reflection->GetBool(ours,   field) : b;
                    bool t = hasTheirs ? reflection->GetBool(theirs, field) : b;
                    bool m = merge(b, o, t, strategy);
                    resReflection->SetBool(result.get(), field, m);
                    break;
                }

                case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
                    int    b = hasBase   ? reflection->GetEnumValue(base,   field) : 0;
                    int    o = hasOurs   ? reflection->GetEnumValue(ours,   field) : b;
                    int    t = hasTheirs ? reflection->GetEnumValue(theirs, field) : b;
                    int    m = merge(b, o, t, strategy);
                    resReflection->SetEnumValue(result.get(), field, m);
                    break;
                }

                case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
                    std::string b = hasBase   ? reflection->GetString(base,   field) : std::string{};
                    std::string o = hasOurs   ? reflection->GetString(ours,   field) : b;
                    std::string t = hasTheirs ? reflection->GetString(theirs, field) : b;
                    std::string m = merge(b, o, t, strategy);
                    resReflection->SetString(result.get(), field, m);
                    break;
                }
                // … handle CPPTYPE_DOUBLE, BOOL, ENUM similarly …
                default: {
                    // // fallback: prefer ours, then theirs, then base
                    // if (hasOurs)   resReflection->CopyField(result.get(), ours,   field);
                    // else if (hasTheirs) resReflection->CopyField(result.get(), theirs, field);
                    // else if (hasBase)   resReflection->CopyField(result.get(), base,   field);
                    // break;
                }
                }
            }
        }
    }

    return result;
}
}

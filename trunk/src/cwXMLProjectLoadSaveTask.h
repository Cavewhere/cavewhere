#ifndef CWXMLPROJECTLOADSAVETASK_H
#define CWXMLPROJECTLOADSAVETASK_H

//Our includes
#include <cwTask.h>
#include <cwCavingRegion.h>
#include <cwCave.h>

//Qt includes
#include <QList>

//Boost xml includes
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/collection_traits.hpp>
#include <boost/serialization/collections_load_imp.hpp>
#include <boost/serialization/collections_save_imp.hpp>

class cwXMLProjectIOTask : public cwTask
{
public:
    cwXMLProjectIOTask();


};


BOOST_CLASS_VERSION(cwCavingRegion, 1)
BOOST_CLASS_VERSION(cwCave, 1)

BOOST_SERIALIZATION_COLLECTION_TRAITS(QList)

BOOST_SERIALIZATION_SPLIT_FREE(QString)
BOOST_SERIALIZATION_SPLIT_FREE(cwCave)
BOOST_SERIALIZATION_SPLIT_FREE(cwCavingRegion)

/**
  All the saving and loading for all the objects
  */
namespace boost {
namespace  serialization {

/**
  Save and load a region
  */
template<class Archive>
void save(Archive & archive, const cwCavingRegion & region, const unsigned int /*version*/) {
    //Do the saving
    QList<cwCave*> caves = region.caves();
    archive << BOOST_SERIALIZATION_NVP(caves);
}

/**
      Load a region
      */
template<class Archive>
void load(Archive & archive, cwCavingRegion & region, const unsigned int /*version*/) {
    QList<cwCave*> caves;

    //Do the loading or the saving
    archive >> BOOST_SERIALIZATION_NVP(caves);

    region.clearCaves();
    region.addCaves(caves);
}


//---------------------------------------------------------------------------
/// Saves a QList object to a collection
template<class Archive, class U >
inline void save(Archive &ar, const QList< U > &t, const uint /* file_version */ )
{
    boost::serialization::stl::save_collection< Archive, QList<U> >(ar, t);
}

//---------------------------------------------------------------------------
/// Loads a QList object from a collection
template<class Archive, class U>
inline void load(Archive &ar, QList<U > &t, const uint /* file_version */ )
{
        boost::serialization::stl::load_collection<
            Archive,
            QList<U>,
            boost::serialization::stl::archive_input_seq<Archive, QList<U> >,
            boost::serialization::stl::no_reserve_imp< QList<U> > >(ar, t);
}

//---------------------------------------------------------------------------
/// split non-intrusive serialization function member into separate
/// non intrusive save/load member functions
template<class Archive, class U >
inline void serialize(Archive &ar, QList<U> &t, const uint file_version )
{
    boost::serialization::split_free( ar, t, file_version);
}

/**
  Save a cave
  */
template<class Archive>
void save(Archive & archive, const cwCave& cave, const unsigned int /*version*/) {
    QString name = cave.name();
    archive << BOOST_SERIALIZATION_NVP(name);
}

/**
  load a cave
  */
template<class Archive>
void load(Archive & archive, cwCave& cave, const unsigned int /*version*/) {
    QString name;
    archive >> BOOST_SERIALIZATION_NVP(name);
    cave.setName(name);
}

/**
  Save QString
  */
template<class Archive>
void save(Archive &archive, const QString &string, const unsigned int) {
    std::wstring QString = string.toStdWString();
    archive << BOOST_SERIALIZATION_NVP(stdString);
}

/**
 Load a QString
  */
template<class Archive>
void load(Archive &archive, QString &string, const unsigned int) {
    std::wstring stdString;
    archive >> BOOST_SERIALIZATION_NVP(stdString);
    string = QString::fromStdWString(stdString);
}

}
}



#endif // CWXMLPROJECTLOADSAVETASK_H

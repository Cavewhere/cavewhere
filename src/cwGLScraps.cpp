//Our includes
#include "cwImageTexture.h"
#include "cwGLScraps.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwCamera.h"
#include "cwSurveyNoteModel.h"
#include "cwGLShader.h"
#include "cwShaderDebugger.h"
#include "cwGlobalDirectory.h"
#include "cwProject.h"


cwGLScraps::cwGLScraps(QObject *parent) :
    cwGLObject(parent),
    Project(NULL),
    Region(NULL)
{
}

/**
  \brief This goes through all the scraps in the region and uploads the data to the graphics card.

  This will clear all previously stored data
  */
void cwGLScraps::updateGeometry() {
    if(Region == NULL) { return; }

    setDirty(true);
}

void cwGLScraps::initialize() {
    initializeShaders();
}

void cwGLScraps::draw() {
    if(Scraps.isEmpty()) { return; }

    Program->bind();

    Program->setUniformValue(UniformModelViewProjectionMatrix, camera()->viewProjectionMatrix());
    Program->enableAttributeArray(vVertex);
    Program->enableAttributeArray(vScrapTexCoords);

    glEnable(GL_TEXTURE_2D);

    foreach(GLScrap scrap, Scraps) {
        scrap.Texture->updateData();

        scrap.Texture->bind();

        scrap.IndexBuffer.bind();

        scrap.PointBuffer.bind();
        Program->setAttributeBuffer(vVertex, GL_FLOAT, 0, 3);

        scrap.TexCoords.bind();
        Program->setAttributeBuffer(vScrapTexCoords, GL_FLOAT, 0, 2);

        glDrawElements(GL_TRIANGLES, scrap.NumberOfIndices, GL_UNSIGNED_INT, NULL);

        scrap.IndexBuffer.release();
        scrap.PointBuffer.release();
        scrap.TexCoords.release();

        scrap.Texture->release();
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    Program->disableAttributeArray(vVertex);
    Program->disableAttributeArray(vScrapTexCoords);
    Program->release();
}

/**
 * @brief cwGLScraps::updateData
 *
 * Updates the scrap geometry.  Make sure geometryItersecter isn't NULL
 */
void cwGLScraps::updateData()
{
    if(geometryItersecter() == NULL) { return; }

    //Clear the rendering GL stuff
    Scraps.clear();

    //Clear the itersecter of it's data for this object
    geometryItersecter()->clear(this);

    QList<cwTriangulatedData> allData = updatedTriangulatedData();
    Scraps.reserve(allData.size());

    foreach(cwTriangulatedData data, allData) {
        Q_ASSERT(data.points().size() == data.texCoords().size());

        Scraps.append(GLScrap(data, project()));

        //For geometry intersection, mouse z depth
        cwGeometryItersecter::Object geometryObject(
                    this, //This object's pointer
                    Scraps.size(), //Id
                    data.points(),
                    data.indices(),
                    cwGeometryItersecter::Triangles);

        //FIXME: This could potentially slow down the rendering.
        geometryItersecter()->addObject(geometryObject);
    }

    setDirty(false);
}

/**
  \brief This initilizes the shaders for the scraps
  */
void cwGLScraps::initializeShaders() {
    cwGLShader* scrapVertexShader = new cwGLShader(QOpenGLShader::Vertex);
    scrapVertexShader->setSourceFile(cwGlobalDirectory::baseDirectory() + "shaders/scrap.vert");

    cwGLShader* scrapFragmentShader = new cwGLShader(QOpenGLShader::Fragment);
    scrapFragmentShader->setSourceFile(cwGlobalDirectory::baseDirectory() + "shaders/scrap.frag");

//    cwGLShader* scrapGeometryShader = new cwGLShader(QOpenGLShader::Geometry);
//    scrapGeometryShader->setSourceFile(cwGlobalDirectory::baseDirectory() + "shaders/scrap.geom");

    Program = new QOpenGLShaderProgram();
    Program->addShader(scrapVertexShader);
    Program->addShader(scrapFragmentShader);
//    Program->addShader(scrapGeometryShader);

//    Program->setGeometryInputType(GL_TRIANGLES);
//    Program->setGeometryOutputType(GL_TRIANGLE_STRIP);

    bool linkingErrors = Program->link();
    if(!linkingErrors) {
        qDebug() << "Linking errors:" << Program->log();
    }

    shaderDebugger()->addShaderProgram(Program);

//    Program->bind();

    UniformModelViewProjectionMatrix = Program->uniformLocation("ModelViewProjectionMatrix");
    vVertex = Program->attributeLocation("vVertex");
    vScrapTexCoords = Program->attributeLocation("vScrapTexCoords");
//    Program->setUniformValue("colorBG", Qt::green);
    Program->setUniformValue("Texture", 0);

//    UniformModelMatrix = Program->uniformLocation("ModelMatrix");
}

cwGLScraps::GLScrap::GLScrap() :
    NumberOfIndices(0),
    Texture(NULL)
{

}

cwGLScraps::GLScrap::GLScrap(const cwTriangulatedData& data, cwProject *project) :
    Texture(new cwImageTexture())
{
    PointBuffer = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    PointBuffer.create();
    PointBuffer.bind();
    int pointBufferSize = data.points().size() * sizeof(QVector3D);
    PointBuffer.allocate(data.points().constData(), pointBufferSize);
    PointBuffer.release();

    IndexBuffer = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
    IndexBuffer.create();
    IndexBuffer.bind();
    int indexBufferSize = data.indices().size() * sizeof(uint);
    IndexBuffer.allocate(data.indices().constData(), indexBufferSize);
    IndexBuffer.release();
    NumberOfIndices = data.indices().size();

    TexCoords = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    TexCoords.create();
    TexCoords.bind();
    int texCoordSize = data.texCoords().size() * sizeof(QVector2D);
    TexCoords.allocate(data.texCoords().constData(), texCoordSize);
    TexCoords.release();

    //Upload the texture to the graphics card
    Texture->initialize();
    Texture->setProject(project->filename());
    Texture->setImage(data.croppedImage());
}


/**
Sets project
*/
void cwGLScraps::setProject(cwProject* project) {
    if(Project != project) {
        if(Project != NULL) {
            disconnect(Project, &cwProject::filenameChanged, this, &cwGLScraps::updateGeometry);
        }

        Project = project;

        if(Project != NULL) {
            connect(Project, &cwProject::filenameChanged, this, &cwGLScraps::updateGeometry);
        }

        emit projectChanged();
    }
}

/**
 * @brief cwGLScraps::updatedTriangulatedData
 * @return Get's all the triangulated data that needs to be updated
 */
QList<cwTriangulatedData> cwGLScraps::updatedTriangulatedData() const
{
    //Goes through all the caves, trips, notes and scraps and get triangulationData

    QList<cwTriangulatedData> allData;

    if(Region != NULL) {
        foreach(cwCave* cave, Region->caves()) {
            foreach(cwTrip* trip, cave->trips()) {
                foreach(cwNote* note, trip->notes()->notes()) {
                    foreach(cwScrap* scrap, note->scraps()) {
                        allData.append(scrap->triangulationData());
                    }
                }
            }
        }
    }

    return allData;
}





/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

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
    MaxScrapId(0),
    Visible(true)
{
}

void cwGLScraps::initialize() {
    initializeShaders();
}

void cwGLScraps::draw() {
    if(Scraps.isEmpty()) { return; }
    if(!visible()) { return; }

    Program->bind();

    Program->setUniformValue(UniformModelViewProjectionMatrix, camera()->viewProjectionMatrix());
    Program->enableAttributeArray(vVertex);
    Program->enableAttributeArray(vScrapTexCoords);

    glEnable(GL_TEXTURE_2D);

    foreach(GLScrap scrap, Scraps) {
        Program->setUniformValue(UniformScaleTexCoords, scrap.Texture->scaleTexCoords());

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

    foreach(PendingScrapCommand command, PendingChanges.values()) {
        switch(command.type()) {
        case PendingScrapCommand::AddScrap:
        {
            //For geometry intersection, mouse z depth
            int scrapId = -1;

            if(Scraps.contains(command.scrap())) {
                GLScrap& glScrap = Scraps[command.scrap()];
                glScrap.update(command.triangulatedData());
                scrapId = glScrap.ScrapId;
            } else {
                GLScrap glScrap(command.triangulatedData(), project());
                glScrap.ScrapId = MaxScrapId++;
                scrapId = glScrap.ScrapId;
                Scraps.insert(command.scrap(), glScrap);
            }

            cwGeometryItersecter::Object geometryObject(
                        this, //This object's pointer
                        scrapId, //Id
                        command.triangulatedData().points(),
                        command.triangulatedData().indices(),
                        cwGeometryItersecter::Triangles);

            //Update the geometry intersector
            geometryItersecter()->addObject(geometryObject);
            break;
        }
        case PendingScrapCommand::RemoveScrap:
        {
            if(Scraps.contains(command.scrap())) {
                 GLScrap& glScrap = Scraps[command.scrap()];
                 geometryItersecter()->removeObject(this, glScrap.ScrapId);
                 glScrap.releaseResources();
                 Scraps.remove(command.scrap());
            }
            break;
        }
        default:
            break;
        }
    }

    PendingChanges.clear();
    setDirty(false);
}

/**
 * @brief cwGLScraps::addScrapToUpdate
 * @param scrap - The scrap.  This isn't used, just for book keeping
 * @param data - The trianglated data for this scrap
 *
 * This will add the scrap and the data to pending
 */
void cwGLScraps::addScrapToUpdate(cwScrap *scrap)
{
    PendingScrapCommand command = PendingScrapCommand(PendingScrapCommand::AddScrap,
                                                      scrap,
                                                      scrap->triangulationData());

    if(PendingChanges.contains(scrap)) {
        PendingScrapCommand command = PendingChanges.value(scrap);
        if(command.type() == PendingScrapCommand::RemoveScrap) {
            //Replace with a add command
            PendingChanges.insert(scrap, command);
            setDirty(true);
        }
    } else {
        PendingChanges.insert(scrap, command);
        setDirty(true);
    }
}

/**
 * @brief cwGLScraps::removeScrap
 * @param scrap
 */
void cwGLScraps::removeScrap(cwScrap *scrap)
{
    PendingScrapCommand command = PendingScrapCommand(PendingScrapCommand::RemoveScrap,
                                                      scrap,
                                                      cwTriangulatedData());

    if(PendingChanges.contains(scrap)) {
        PendingScrapCommand command = PendingChanges.value(scrap);
        if(command.type() == PendingScrapCommand::AddScrap) {
            //Scrap has been removed
            PendingChanges.insert(scrap, command);
            setDirty(true);
        }
    } else {
        PendingChanges.insert(scrap, command);
        setDirty(true);
    }
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
    UniformScaleTexCoords = Program->uniformLocation("vTexCoordsScale");
    UniformModelViewProjectionMatrix = Program->uniformLocation("ModelViewProjectionMatrix");
    vVertex = Program->attributeLocation("vVertex");
    vScrapTexCoords = Program->attributeLocation("vScrapTexCoords");

//    Program->setUniformValue("colorBG", Qt::green);
    Program->setUniformValue("Texture", 0);

//    UniformModelMatrix = Program->uniformLocation("ModelMatrix");
}

cwGLScraps::GLScrap::GLScrap() :
    NumberOfIndices(0),
    ScrapId(-1),
    Texture(NULL)

{

}

cwGLScraps::GLScrap::GLScrap(const cwTriangulatedData& data, cwProject *project) :
    ScrapId(-1),
    Texture(new cwImageTexture())
{
    PointBuffer = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    PointBuffer.create();

    IndexBuffer = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
    IndexBuffer.create();

    TexCoords = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    TexCoords.create();

    //Upload the texture to the graphics card
    Texture->initialize();
    Texture->setProject(project->filename());

    update(data);
}

/**
 * @brief cwGLScraps::GLScrap::update
 * @param data.  This update the data in the glSCrap
 */
void cwGLScraps::GLScrap::update(const cwTriangulatedData &data)
{
    PointBuffer.bind();
    int pointBufferSize = data.points().size() * sizeof(QVector3D);
    PointBuffer.allocate(data.points().constData(), pointBufferSize);
    PointBuffer.release();

    IndexBuffer.bind();
    int indexBufferSize = data.indices().size() * sizeof(uint);
    IndexBuffer.allocate(data.indices().constData(), indexBufferSize);
    IndexBuffer.release();
    NumberOfIndices = data.indices().size();

    TexCoords.bind();
    int texCoordSize = data.texCoords().size() * sizeof(QVector2D);
    TexCoords.allocate(data.texCoords().constData(), texCoordSize);
    TexCoords.release();

    Texture->setImage(data.croppedImage());
}

void cwGLScraps::GLScrap::releaseResources()
{
    PointBuffer.release();
    IndexBuffer.release();
    TexCoords.release();
    delete Texture;
}


/**
Sets project
*/
void cwGLScraps::setProject(cwProject* project) {
    if(Project != project) {
        Project = project;
        emit projectChanged();
    }
}

/**
    Sets visible make scraps visible / invisible
*/
void cwGLScraps::setVisible(bool visible) {
    if(Visible != visible) {
        Visible = visible;
        emit visibleChanged();
    }
}



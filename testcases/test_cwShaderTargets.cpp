// Every baked shader must carry a desktop GLSL variant the RHI OpenGL backend
// can pick on the lowest desktop context CaveWhere meets. The backend only
// accepts a GLSL version at or below the context's, and virtualized drivers
// (virgl under Parallels) stop at an OpenGL 4.0 context: a shader baked for
// 410 alone fails to load there with "No GLSL shader code found", and the pass
// that needed it draws nothing.

#include <catch2/catch_test_macros.hpp>

#include "cwRHIObject.h"

#include <QDirIterator>
#include <QString>
#include <rhi/qshader.h>

namespace {
constexpr int kLowestDesktopGlslVersion = 400;
}

TEST_CASE("Every baked shader has a desktop GLSL variant an OpenGL 4.0 context can load",
          "[Shaders]")
{
    QStringList shaderPaths;
    QDirIterator it(QStringLiteral(":/shaders"), {QStringLiteral("*.qsb")},
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        shaderPaths.append(it.next());
    }
    REQUIRE(!shaderPaths.isEmpty());
    REQUIRE(shaderPaths.contains(QStringLiteral(":/shaders/EDL_MSAA.frag.qsb")));

    for (const QString& path : std::as_const(shaderPaths)) {
        INFO(path.toStdString());
        const QShader shader = cwRHIObject::loadShader(path);
        REQUIRE(shader.isValid());

        int lowestDesktopVersion = std::numeric_limits<int>::max();
        for (const QShaderKey& key : shader.availableShaders()) {
            if (key.source() != QShader::GlslShader
                || key.sourceVersion().flags().testFlag(QShaderVersion::GlslEs)) {
                continue;
            }
            lowestDesktopVersion = std::min(lowestDesktopVersion, key.sourceVersion().version());
        }
        CHECK(lowestDesktopVersion <= kLowestDesktopGlslVersion);
    }
}

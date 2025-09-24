//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwRenderGLTF.h"
#include "cwGltfLoader.h"


TEST_CASE("GLTF file should be loaded correctly", "[cwRenderGLTF]") {

    // cwRenderGLTF render;
    // render.setGLTFFilePath("/Users/cave/Downloads/9_8_2025.glb");

    auto scene = cw::gltf::Loader::loadGltf("/Users/cave/Downloads/9_8_2025.glb");
    scene.dump();


}

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/version.h>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try {
        Assimp::Importer importer;
        if (!importer.IsExtensionSupported(".obj") || !importer.IsExtensionSupported(".gltf"))
            throw std::runtime_error("Required importer missing");
        unsigned files = 0, meshes = 0;
        for (const auto& file : std::filesystem::recursive_directory_iterator(argv[1])) {
            if (!file.is_regular_file() || file.path().extension() != ".obj") continue;
            const auto* scene = importer.ReadFile(file.path().string(),
                aiProcess_Triangulate | aiProcess_FlipWindingOrder | aiProcess_FlipUVs);
            if (!scene || !scene->mRootNode || !scene->HasMeshes())
                throw std::runtime_error(file.path().string() + ": " + importer.GetErrorString());
            for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
                const auto* mesh = scene->mMeshes[i];
                if (!mesh || !mesh->HasPositions() || !mesh->HasNormals() || !mesh->HasTextureCoords(0))
                    throw std::runtime_error("Missing vertex attributes: " + file.path().string());
                for (unsigned v = 0; v < mesh->mNumVertices; ++v) {
                    for (const auto& p : {mesh->mVertices[v], mesh->mNormals[v], mesh->mTextureCoords[0][v]})
                        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
                            throw std::runtime_error("Non-finite vertex");
                }
                for (unsigned f = 0; f < mesh->mNumFaces; ++f) {
                    const auto& face = mesh->mFaces[f];
                    if (face.mNumIndices != 3 || !face.mIndices) throw std::runtime_error("Invalid triangle");
                    for (unsigned k = 0; k < 3; ++k)
                        if (face.mIndices[k] >= mesh->mNumVertices) throw std::runtime_error("Index OOB");
                }
                ++meshes;
            }
            ++files;
        }
        if (files == 0) throw std::runtime_error("No OBJ tested");
        std::cout << "PASS Assimp " << aiGetVersionMajor() << '.' << aiGetVersionMinor()
                  << '.' << aiGetVersionPatch() << " flags=" << aiGetCompileFlags()
                  << " files=" << files << " meshes=" << meshes << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

#include "loaders/ModelLoader.h"
#include "loaders/GLTFLoader.h"
#include "loaders/FBXLoader.h"
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

namespace cl
{
    Model* LoadModel(const char* filePath)
    {
        std::filesystem::path path = filePath;

        if (path.extension() == ".gltf" || path.extension() == ".glb")
            return LoadGLTF(filePath);
        else if (path.extension() == ".fbx")
            return LoadFBX(filePath);

        return nullptr;
    }

    Model* LoadInstance(const Model* model)
    {
        if (!model)
            return nullptr;

        Model* instance = new Model();
        instance->m_meshes = model->GetMeshes();
        instance->m_skeleton = model->GetSkeleton();
        instance->m_animations = model->m_animations;
        instance->Reset();

        if (instance->m_skeleton)
            instance->m_animator.SetSkeleton(instance->m_skeleton);

        return instance;
    }
}
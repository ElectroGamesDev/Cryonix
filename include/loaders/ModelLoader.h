#pragma once

#include "Model.h"
#include "Animation.h"
#include <vector>
#include <string>

namespace cl
{
    Model* LoadModel(std::string_view filePath, bool mergeMeshes = true);
    Model* CloneModel(const Model* model);

    AnimationClip* LoadAnimation(std::string_view filePath, size_t animationIndex = 0);
    AnimationClip* LoadAnimation(std::string_view filePath, std::string_view animationName);
    std::vector<AnimationClip*> LoadAnimations(std::string_view filePath);
}
#pragma once

#include "Model.h"
#include "Animation.h"
#include <vector>
#include <string>

namespace cl
{
    Model* LoadModel(const char* filePath);
    Model* LoadInstance(const Model* model);

    AnimationClip* LoadAnimation(const char* filePath, size_t animationIndex = 0);
    AnimationClip* LoadAnimation(const char* filePath, const std::string& animationName);
    std::vector<AnimationClip*> LoadAnimations(const char* filePath);
}
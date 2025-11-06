#pragma once

#include "Model.h"
#include "Animation.h"
#include <vector>
#include <string>

namespace cl
{
    Model* LoadFBX(const char* filePath);

    AnimationClip* LoadAnimationFromFBX(const char* filePath, size_t animationIndex = 0);
    AnimationClip* LoadAnimationFromFBX(const char* filePath, const std::string& animationName);
    std::vector<AnimationClip*> LoadAnimationsFromFBX(const char* filePath);
}
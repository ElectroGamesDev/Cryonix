#pragma once

#include "Model.h"
#include "Animation.h"
#include <vector>
#include <string>

namespace cl
{
    Model* LoadGLTF(const char* filePath);

    AnimationClip* LoadAnimationFromGLTF(const char* filePath, size_t animationIndex = 0);
    AnimationClip* LoadAnimationFromGLTF(const char* filePath, const std::string& animationName);
    std::vector<AnimationClip*> LoadAnimationsFromGLTF(const char* filePath);
}
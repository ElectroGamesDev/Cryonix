#pragma once

#include "Model.h"

namespace cx
{
    Model* LoadOBJ(std::string_view filePath, bool mergeMeshes = true);
}
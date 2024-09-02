#pragma once

#include "rc.h"
#include "backend.h"

#include <glm/vec2.hpp>

class CVulkanTexture;

namespace gamescope
{
    class ISceneGraphNode
    {
    public:
        struct Layer
        {
            glm::vec2 vOffset{ 0.0f, 0.0f };
            glm::vec2 vScale{ 1.0f, 1.0f };

            float flOpacity = 1.0f;
        };

        virtual Layer Flatten() const;
    };
}
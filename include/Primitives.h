#pragma once

#include "Mesh.h"
#include "Model.h"
#include "Maths.h"

namespace cl
{
    void InitPrimitives();
    Material* GetPrimitiveMaterial();

    // Generate meshes

    Mesh GenCubeMesh(
        float width = 1.0f,
        float height = 1.0f,
        float length = 1.0f,
        bool smoothNormals = true,
        bool generateUVs = true,
        bool inward = false,
        bool centered = true);

    Mesh GenSphereMesh(
        float radius = 1.0f,
        int rings = 16,
        int slices = 16,
        bool smoothNormals = true,
        bool generateUVs = true,
        bool inward = false,
        bool centered = true,
        float startAngle = 0.0f,
        float endAngle = 360.0f,
        bool hemiTop = false,
        bool hemiBottom = false);

    Mesh GenPlaneMesh(
        float width = 1.0f,
        float length = 1.0f,
        int resX = 1,
        int resZ = 1,
        bool smoothNormals = true,
        bool generateUVs = true,
        bool inward = false,
        bool centered = true,
        Vector2 texRepeat = { 1.0f, 1.0f },
        bool doubleSided = false);

    Mesh GenCylinderMesh(
        float radius = 0.5f,
        float height = 1.0f,
        int slices = 16,
        bool smoothNormals = true,
        bool generateUVs = true,
        bool inward = false,
        bool centered = true,
        bool cappedTop = true,
        bool cappedBottom = true);

    Mesh GenConeMesh(
        float radius = 0.5f,
        float height = 1.0f,
        int slices = 16,
        bool smoothNormals = true,
        bool generateUVs = true,
        bool inward = false,
        bool centered = true,
        bool capped = true,
        float topRadius = 0.0f);

    Mesh GenQuadMesh(
        float width = 1.0f,
        float height = 1.0f,
        int resX = 1,
        int resZ = 1,
        bool smoothNormals = true,
        bool generateUVs = true,
        bool inward = false,
        bool centered = true,
        Vector2 texRepeat = { 1.0f, 1.0f },
        bool doubleSided = false);

    Mesh GenCapsuleMesh(
        float radius = 0.5f,
        float height = 1.0f,
        int slices = 16,
        int stacks = 8,
        bool smoothNormals = true,
        bool generateUVs = true,
        bool inward = false,
        bool centered = true,
        float capRatio = 0.5f);


    // Generate model from mesh
    Model GenModelFromMesh(const Mesh& mesh);

    // Generate models

    Model GenCubeModel(
        float width = 1.0f,
        float height = 1.0f,
        float length = 1.0f,
        bool smoothNormals = true,
        bool generateUVs = true,
        bool inward = false,
        bool centered = true);

    Model GenSphereModel(
        float radius = 1.0f,
        int rings = 16,
        int slices = 16,
        bool smoothNormals = true,
        bool generateUVs = true,
        bool inward = false,
        bool centered = true,
        float startAngle = 0.0f,
        float endAngle = 360.0f,
        bool hemiTop = false,
        bool hemiBottom = false);

    Model GenPlaneModel(
        float width = 1.0f,
        float length = 1.0f,
        int resX = 1,
        int resZ = 1,
        bool smoothNormals = true,
        bool generateUVs = true,
        bool inward = false,
        bool centered = true,
        Vector2 texRepeat = { 1.0f, 1.0f },
        bool doubleSided = false);

    Model GenCylinderModel(
        float radius = 0.5f,
        float height = 1.0f,
        int slices = 16,
        bool smoothNormals = true,
        bool generateUVs = true,
        bool inward = false,
        bool centered = true,
        bool cappedTop = true,
        bool cappedBottom = true);

    Model GenConeModel(
        float radius = 0.5f,
        float height = 1.0f,
        int slices = 16,
        bool smoothNormals = true,
        bool generateUVs = true,
        bool inward = false,
        bool centered = true,
        bool capped = true,
        float topRadius = 0.0f);

    Model GenQuadModel(
        float width = 1.0f,
        float length = 1.0f,
        int resX = 1,
        int resZ = 1,
        bool smoothNormals = true,
        bool generateUVs = true,
        bool inward = false,
        bool centered = true,
        Vector2 texRepeat = { 1.0f, 1.0f },
        bool doubleSided = false);

    Model GenCapsuleModel(
        float radius = 0.5f,
        float height = 1.0f,
        int slices = 16,
        int stacks = 8,
        bool smoothNormals = true,
        bool generateUVs = true,
        bool inward = false,
        bool centered = true,
        float capRatio = 0.5f);
}
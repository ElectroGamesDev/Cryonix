#include "loaders/OBJLoader.h"
#include "Maths.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <stb_image.h>
#include <rapidobj/rapidobj.hpp>
#include <mutex>
#include <thread>
#include <vector>
#include <atomic>
#include <future>
#include <algorithm>
#include <memory>
#include <limits>
#include <execution>

namespace cx
{
    struct VertexKey
    {
        int pos;
        int tex;
        int norm;

        bool operator==(VertexKey const& o) const noexcept
        {
            return pos == o.pos && tex == o.tex && norm == o.norm;
        }
    };

    struct VertexKeyHash
    {
        size_t operator()(VertexKey const& k) const noexcept
        {
            size_t h1 = std::hash<int>()(k.pos);
            size_t h2 = std::hash<int>()(k.tex);
            size_t h3 = std::hash<int>()(k.norm);
            size_t seed = h1;

            seed ^= h2 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
            seed ^= h3 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);

            return seed;
        }
    };

    struct DecodedImage
    {
        int width = 0;
        int height = 0;
        int channels = 0;
        std::vector<unsigned char> pixels;
    };

    struct TextureCache
    {
        TextureCache() = default;

        std::shared_ptr<DecodedImage> GetIfExists(std::string_view path)
        {
            std::shared_lock lock(mutex_);
            auto it = cache_.find(path.data());
            return (it != cache_.end()) ? it->second : nullptr;
        }

        void Insert(std::string_view path, const std::shared_ptr<DecodedImage>& image)
        {
            std::unique_lock lock(mutex_);
            cache_[path.data()] = image;
        }

        std::unordered_map<std::string, std::shared_ptr<DecodedImage>> cache_;
        mutable std::shared_mutex mutex_;
    };

    struct MaterialData
    {
        Color albedo = Color::White();
        float roughness = 0.5f;
        float metallic = 0.0f;
        Color emissive = Color::White();

        std::shared_ptr<DecodedImage> mapAlbedo;
        std::shared_ptr<DecodedImage> mapMetallicRoughness;
        std::shared_ptr<DecodedImage> mapNormal;
        std::shared_ptr<DecodedImage> mapAO;
        std::shared_ptr<DecodedImage> mapEmissive;
    };

    struct MeshData
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        int materialIndex = -1;
    };

    static size_t GetThreadCount()
    {
        unsigned int hwThreads = std::thread::hardware_concurrency();

        // Fallback if unknown
        if (hwThreads == 0)
            hwThreads = 4;

        return static_cast<size_t>(hwThreads >= 2 ? hwThreads - 1 : 1);
    }

    Model* LoadOBJ(std::string_view filePath, bool mergeMeshes)
    {
        if (!std::filesystem::exists(filePath))
        {
            std::cerr << "[ERROR] Failed to load \"" << filePath << "\". File does not exist." << std::endl;
            return nullptr;
        }

        // Parse OBJ
        rapidobj::Result result = rapidobj::ParseFile(filePath, rapidobj::MaterialLibrary::Default(rapidobj::Load::Optional));
        if (result.error)
        {
            std::cerr << "[ERROR] Failed to parse OBJ file: " << result.error.code.message() << std::endl;
            return nullptr;
        }

        // Triangulate
        if (!rapidobj::Triangulate(result))
        {
            std::cerr << "[ERROR] Failed to triangulate OBJ mesh: " << result.error.code.message() << std::endl;
            return nullptr;
        }

        Model* model = new Model();

        const std::filesystem::path objPath = filePath;
        const std::filesystem::path objDir = objPath.parent_path();
        TextureCache textureCache;

        // Worker threads only decode image data; bgfx resources are created on the main thread
        auto loadImageFromFile = [&](const std::filesystem::path& fullPath, bool isColorTexture) -> std::shared_ptr<DecodedImage>
            {
                if (!std::filesystem::exists(fullPath))
                    return nullptr;

                int width = 0, height = 0, channels = 0;
                int desired = isColorTexture ? 4 : 0;
                unsigned char* pixels = stbi_load(fullPath.string().c_str(), &width, &height, &channels, desired);
                if (!pixels)
                {
                    std::cerr << "[WARNING] Failed to load texture: " << fullPath << " - " << stbi_failure_reason() << std::endl;
                    return nullptr;
                }

                auto image = std::make_shared<DecodedImage>();
                image->width = width;
                image->height = height;
                image->channels = (desired == 0) ? channels : desired;
                size_t pixelCount = static_cast<size_t>(width) * height * image->channels;
                image->pixels.assign(pixels, pixels + pixelCount);

                stbi_image_free(pixels);
                return image;
            };

        auto loadImageWithCache = [&](std::string_view texPath, bool isColorTexture) -> std::shared_ptr<DecodedImage>
            {
                if (texPath.empty())
                    return nullptr;

                // Fast read path
                if (auto cached = textureCache.GetIfExists(texPath))
                    return cached;

                // Build full path
                std::filesystem::path fullPath = objDir / texPath;

                // Load image then insert into cache
                auto image = loadImageFromFile(fullPath, isColorTexture);
                if (!image)
                    return nullptr;

                textureCache.Insert(texPath, image);
                return image;
            };

        // Decode materials in parallel (CPU-only; no bgfx calls on worker threads)
        std::vector<MaterialData> materialData(result.materials.size());

        const size_t materialCount = result.materials.size();
        size_t threadCount = std::min(materialCount == 0 ? size_t(1) : materialCount, GetThreadCount());
        std::atomic<size_t> materialIndex(0);
        std::vector<std::thread> materialWorkers;
        materialWorkers.reserve(threadCount);

        for (size_t t = 0; t < threadCount; ++t)
        {
            materialWorkers.emplace_back([&]() {
                size_t i;

                while ((i = materialIndex.fetch_add(1)) < materialCount)
                {
                    const rapidobj::Material& objMat = result.materials[i];
                    MaterialData& md = materialData[i];

                    md.albedo = Color(
                        static_cast<unsigned char>(std::clamp(objMat.diffuse[0] * 255.0f, 0.0f, 255.0f)),
                        static_cast<unsigned char>(std::clamp(objMat.diffuse[1] * 255.0f, 0.0f, 255.0f)),
                        static_cast<unsigned char>(std::clamp(objMat.diffuse[2] * 255.0f, 0.0f, 255.0f)),
                        static_cast<unsigned char>(std::clamp((1.0f - objMat.dissolve) * 255.0f, 0.0f, 255.0f))
                    );

                    // Roughness/metallic
                    md.roughness = (objMat.roughness >= 0.0f) ? objMat.roughness : (1.0f - (objMat.shininess / 1000.0f));
                    md.metallic = (objMat.metallic >= 0.0f) ? objMat.metallic : 0.0f;

                    md.emissive = Color(
                        static_cast<unsigned char>(std::clamp(objMat.emission[0] * 255.0f, 0.0f, 255.0f)),
                        static_cast<unsigned char>(std::clamp(objMat.emission[1] * 255.0f, 0.0f, 255.0f)),
                        static_cast<unsigned char>(std::clamp(objMat.emission[2] * 255.0f, 0.0f, 255.0f)),
                        255
                    );

                    // Texture decoding
                    if (!objMat.diffuse_texname.empty())
                        md.mapAlbedo = loadImageWithCache(objMat.diffuse_texname, true);

                    // Metallic-roughness // Todo: Need to separate these
                    if (!objMat.roughness_texname.empty())
                        md.mapMetallicRoughness = loadImageWithCache(objMat.roughness_texname, false);

                    if (!objMat.specular_texname.empty())
                        md.mapMetallicRoughness = loadImageWithCache(objMat.specular_texname, false);

                    // Normal map
                    if (!objMat.normal_texname.empty())
                        md.mapNormal = loadImageWithCache(objMat.normal_texname, false);
                    else if (!objMat.bump_texname.empty())
                        md.mapNormal = loadImageWithCache(objMat.bump_texname, false);

                    if (!objMat.ambient_texname.empty())
                        md.mapAO = loadImageWithCache(objMat.ambient_texname, false);

                    if (!objMat.emissive_texname.empty())
                        md.mapEmissive = loadImageWithCache(objMat.emissive_texname, true);
                }
                });
        }

        for (auto& w : materialWorkers)
            w.join();

        size_t totalShapes = result.shapes.size();
        std::vector<MeshData> meshData;
        meshData.reserve(totalShapes * 2);
        std::mutex meshDataMutex;

        size_t shapeThreadCount = std::min<size_t>(totalShapes == 0 ? 1 : totalShapes, GetThreadCount());
        std::atomic<size_t> shapeIndex(0);
        std::vector<std::thread> shapeWorkers;
        shapeWorkers.reserve(shapeThreadCount);

        const auto& positions = result.attributes.positions;
        const auto& normals = result.attributes.normals;
        const auto& texcoords = result.attributes.texcoords;
        const auto& colors = result.attributes.colors;

        for (size_t t = 0; t < shapeThreadCount; ++t)
        {
            shapeWorkers.emplace_back([&, t]() {
                size_t s;
                while ((s = shapeIndex.fetch_add(1)) < result.shapes.size())
                {
                    const auto& shape = result.shapes[s];
                    const auto& mesh = shape.mesh;

                    if (mesh.num_face_vertices.empty())
                        continue;

                    bool hasNormals = !normals.empty();

                    std::vector<Vector3> smoothNormals;
                    if (!hasNormals)
                    {
                        size_t posCount = positions.size() / 3;
                        smoothNormals.assign(posCount, { 0,0,0 });
                        std::vector<int> normalCounts(posCount, 0);

                        size_t idxOffset = 0;
                        for (size_t f = 0; f < mesh.num_face_vertices.size(); ++f)
                        {
                            int numVerts = mesh.num_face_vertices[f];
                            uint32_t smoothGroup = mesh.smoothing_group_ids.empty() ? 1u : mesh.smoothing_group_ids[f];

                            int i0 = mesh.indices[idxOffset].position_index;
                            int i1 = mesh.indices[idxOffset + 1].position_index;
                            int i2 = mesh.indices[idxOffset + 2].position_index;

                            Vector3 v0{ positions[i0 * 3 + 0], positions[i0 * 3 + 1], positions[i0 * 3 + 2] };
                            Vector3 v1{ positions[i1 * 3 + 0], positions[i1 * 3 + 1], positions[i1 * 3 + 2] };
                            Vector3 v2{ positions[i2 * 3 + 0], positions[i2 * 3 + 1], positions[i2 * 3 + 2] };

                            Vector3 faceNormal = Vector3::Cross(v1 - v0, v2 - v0).Normalize();

                            for (int vi = 0; vi < numVerts; ++vi)
                            {
                                int posIdx = mesh.indices[idxOffset + vi].position_index;
                                if (smoothGroup != 0)
                                {
                                    smoothNormals[posIdx] = smoothNormals[posIdx] + faceNormal;
                                    normalCounts[posIdx]++;
                                }
                                else
                                {
                                    smoothNormals[posIdx] = faceNormal;
                                    normalCounts[posIdx] = 1;
                                }
                            }
                            idxOffset += numVerts;
                        }

                        // Normalize if needed. Use parallel only for big arrays
                        static const int NormalizeThreshold = [] {
                            const int threads = static_cast<int>(GetThreadCount());
                            const int baseThreshold = 50000;
                            const int minThreshold = (threads > 8) ? 10000 : 20000;
                            return std::max(minThreshold, baseThreshold / std::max(1, threads));
                        }();

                        if (smoothNormals.size() > NormalizeThreshold)
                        {
                            std::for_each(std::execution::par, smoothNormals.begin(), smoothNormals.end(), [&](Vector3& n) {
                                size_t i = &n - smoothNormals.data();
                                if (normalCounts[i] > 1)
                                    n = n / static_cast<float>(normalCounts[i]);
                                if (normalCounts[i] > 0)
                                    n = n.Normalize();
                                });
                        }
                        else
                        {
                            for (size_t i = 0; i < smoothNormals.size(); ++i)
                            {
                                if (normalCounts[i] > 1)
                                    smoothNormals[i] = smoothNormals[i] / static_cast<float>(normalCounts[i]);

                                if (normalCounts[i] > 0)
                                    smoothNormals[i] = smoothNormals[i].Normalize();
                            }
                        }
                    }

                    // Group faces by material id for batching
                    std::unordered_map<int, std::vector<size_t>> facesByMaterial;
                    facesByMaterial.reserve(mesh.material_ids.size() ? mesh.material_ids.size() : 4);
                    for (size_t fi = 0; fi < mesh.num_face_vertices.size(); ++fi)
                    {
                        int matId = mesh.material_ids.empty() ? -1 : mesh.material_ids[fi];
                        facesByMaterial[matId].push_back(fi);
                    }

                    // For each material group produce a mesh
                    for (auto const& pair : facesByMaterial)
                    {
                        int matId = pair.first;
                        const std::vector<size_t>& faces = pair.second;

                        MeshData data;
                        data.materialIndex = matId;

                        std::vector<char> faceMask(mesh.num_face_vertices.size(), 0);
                        for (size_t f : faces)
                            faceMask[f] = 1;

                        // Reserve approximate sizes
                        size_t estimatedVerts = faces.size() * 3;
                        std::vector<Vertex>& vertices = data.vertices;
                        std::vector<uint32_t>& indices = data.indices;
                        vertices.reserve(estimatedVerts);
                        indices.reserve(estimatedVerts);

                        // Use unordered_map with VertexKey struct for deduplication
                        std::unordered_map<VertexKey, uint32_t, VertexKeyHash> vertexMap;
                        vertexMap.reserve(estimatedVerts * 2);

                        // Iterate over faces once, adding those marked in faceMask
                        size_t idxOffset = 0;
                        for (size_t f = 0; f < mesh.num_face_vertices.size(); ++f)
                        {
                            int numVerts = mesh.num_face_vertices[f];
                            if (!faceMask[f])
                            {
                                idxOffset += numVerts;
                                continue;
                            }

                            // Each face is triangle (triangulated). Create vertices
                            for (int vi = 0; vi < numVerts; ++vi)
                            {
                                const rapidobj::Index& idx = mesh.indices[idxOffset + vi];
                                VertexKey key{
                                    idx.position_index,
                                    (idx.texcoord_index >= 0) ? idx.texcoord_index : std::numeric_limits<int>::min(),
                                    (idx.normal_index >= 0) ? idx.normal_index : std::numeric_limits<int>::min()
                                };

                                auto it = vertexMap.find(key);
                                if (it != vertexMap.end())
                                {
                                    indices.push_back(it->second);
                                }
                                else
                                {
                                    Vertex vert{};

                                    // position
                                    vert.position = {
                                        positions[idx.position_index * 3 + 0],
                                        positions[idx.position_index * 3 + 1],
                                        positions[idx.position_index * 3 + 2]
                                    };

                                    // texcoord
                                    if (idx.texcoord_index >= 0 && !texcoords.empty())
                                    {
                                        vert.texCoord = Vector2{ texcoords[idx.texcoord_index * 2 + 0], texcoords[idx.texcoord_index * 2 + 1] };
                                        //vert.texCoord = Vector2{ texcoords[idx.texcoord_index * 2 + 0], 1.0f - texcoords[idx.texcoord_index * 2 + 1] };
                                    }
                                    else
                                        vert.texCoord = Vector2{ 0.0f, 0.0f };

                                    // normal
                                    if (idx.normal_index >= 0 && hasNormals)
                                    {
                                        vert.normal = Vector3{
                                            normals[idx.normal_index * 3 + 0],
                                            normals[idx.normal_index * 3 + 1],
                                            normals[idx.normal_index * 3 + 2]
                                        }.Normalize();
                                    }
                                    else if (!smoothNormals.empty())
                                        vert.normal = smoothNormals[idx.position_index];
                                    else
                                        vert.normal = { 0.0f, 1.0f, 0.0f };

                                    // color
                                    //if (!colors.empty())
                                    //{
                                    //    size_t cBase = static_cast<size_t>(idx.position_index) * 3;
                                    //    if (cBase + 2 < colors.size())
                                    //    {
                                    //        vert.color = Color(
                                    //            static_cast<unsigned char>(std::clamp(colors[cBase + 0] * 255.0f, 0.0f, 255.0f)),
                                    //            static_cast<unsigned char>(std::clamp(colors[cBase + 1] * 255.0f, 0.0f, 255.0f)),
                                    //            static_cast<unsigned char>(std::clamp(colors[cBase + 2] * 255.0f, 0.0f, 255.0f)),
                                    //            255
                                    //        );
                                    //    }
                                    //}

                                    // default tangent/bitangent
                                    vert.tangent = Vector4(0, 0, 0, 1.0f);  // Default handedness to +1


                                    // skeletal defaults
                                    for (int bi = 0; bi < 4; ++bi)
                                    {
                                        vert.boneIndices[bi] = 0.0f;
                                        vert.boneWeights[bi] = 0.0f;
                                    }

                                    uint32_t newIndex = static_cast<uint32_t>(vertices.size());
                                    vertices.push_back(vert);
                                    indices.push_back(newIndex);
                                    vertexMap.emplace(key, newIndex);
                                }
                            }

                            idxOffset += numVerts;
                        } //

                        // Compute tangents only if the material has a normal map
                        bool hasNormalMap = false;
                        if (matId >= 0 && static_cast<size_t>(matId) < materialData.size())
                            hasNormalMap = (materialData[matId].mapNormal != nullptr);

                        if (hasNormalMap && !vertices.empty())
                        {
                            // Accumulate tangents
                            std::vector<Vector3> tanAccum(vertices.size(), { 0,0,0 });
                            std::vector<int> tanCount(vertices.size(), 0);
                            size_t triCount = indices.size() / 3;

                            // For large meshes use parallel
                            static const int TriThreshold = []() {
                                const int threads = static_cast<int>(GetThreadCount());
                                const int baseThreshold = 40000;
                                const int minThreshold = (threads > 8) ? 10000 : 20000;
                                return std::max(minThreshold, baseThreshold / std::max(1, threads));
                             }();

                            if (triCount > static_cast<size_t>(TriThreshold))
                            {
                                // parallel loop by chunks
                                const size_t chunk = 4096;
                                std::atomic<size_t> triIdx(0);
                                size_t workerCount = std::min(GetThreadCount(), size_t(std::max<size_t>(1, (int)std::thread::hardware_concurrency())));
                                std::vector<std::thread> workers;
                                workers.reserve(workerCount);
                                for (size_t w = 0; w < workerCount; ++w)
                                {
                                    workers.emplace_back([&]() {
                                        size_t i;
                                        while ((i = triIdx.fetch_add(chunk)) < triCount)
                                        {
                                            size_t end = std::min(triCount, i + chunk);
                                            for (size_t tri = i; tri < end; ++tri)
                                            {
                                                uint32_t i0 = indices[tri * 3 + 0];
                                                uint32_t i1 = indices[tri * 3 + 1];
                                                uint32_t i2 = indices[tri * 3 + 2];

                                                const Vertex& v0 = vertices[i0];
                                                const Vertex& v1 = vertices[i1];
                                                const Vertex& v2 = vertices[i2];

                                                Vector3 edge1 = v1.position - v0.position;
                                                Vector3 edge2 = v2.position - v0.position;
                                                Vector2 deltaUV1 = v1.texCoord - v0.texCoord;
                                                Vector2 deltaUV2 = v2.texCoord - v0.texCoord;

                                                float denom = (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
                                                float f = fabs(denom) < 1e-6f ? 0.0f : (1.0f / denom);

                                                Vector3 tangent;
                                                tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
                                                tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
                                                tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
                                                tangent = tangent.Normalize();

                                                tanAccum[i0] = tanAccum[i0] + tangent;
                                                tanAccum[i1] = tanAccum[i1] + tangent;
                                                tanAccum[i2] = tanAccum[i2] + tangent;
                                                tanCount[i0]++;
                                                tanCount[i1]++;
                                                tanCount[i2]++;
                                            }
                                        }
                                        });
                                }
                                for (auto& w : workers) w.join();
                            }
                            else
                            {
                                for (size_t tri = 0; tri < triCount; ++tri)
                                {
                                    uint32_t i0 = indices[tri * 3 + 0];
                                    uint32_t i1 = indices[tri * 3 + 1];
                                    uint32_t i2 = indices[tri * 3 + 2];

                                    const Vertex& v0 = vertices[i0];
                                    const Vertex& v1 = vertices[i1];
                                    const Vertex& v2 = vertices[i2];

                                    Vector3 edge1 = v1.position - v0.position;
                                    Vector3 edge2 = v2.position - v0.position;
                                    Vector2 deltaUV1 = v1.texCoord - v0.texCoord;
                                    Vector2 deltaUV2 = v2.texCoord - v0.texCoord;

                                    float denom = (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
                                    float f = fabs(denom) < 1e-6f ? 0.0f : (1.0f / denom);

                                    Vector3 tangent;
                                    tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
                                    tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
                                    tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
                                    tangent = tangent.Normalize();

                                    tanAccum[i0] = tanAccum[i0] + tangent;
                                    tanAccum[i1] = tanAccum[i1] + tangent;
                                    tanAccum[i2] = tanAccum[i2] + tangent;
                                    tanCount[i0]++;
                                    tanCount[i1]++;
                                    tanCount[i2]++;
                                }
                            }

                            // Normalize and orthogonalize per-vertex
                            for (size_t vi = 0; vi < vertices.size(); ++vi)
                            {
                                if (tanCount[vi] > 0)
                                {
                                    Vector3 tangent = (tanAccum[vi] / static_cast<float>(tanCount[vi])).Normalize();
                                    Vector3 normal = vertices[vi].normal;

                                    // Gram-Schmidt orthogonalize
                                    tangent = (tangent - normal * Vector3::Dot(normal, tangent)).Normalize();

                                    vertices[vi].tangent = Vector4(tangent.x, tangent.y, tangent.z, 1.0f);
                                }
                            }
                        }

                        std::lock_guard<std::mutex> glock(meshDataMutex);
                        meshData.push_back(std::move(data));
                    }
                }
            });
        }

        for (auto& worker : shapeWorkers)
            worker.join();

        // Create materials on the main thread (bgfx resource creation is not thread-safe)
        std::vector<Material*> materials;
        materials.resize(result.materials.size(), nullptr);

        for (size_t i = 0; i < result.materials.size(); ++i)
        {
            const MaterialData& md = materialData[i];
            Material* material = new Material();
            material->SetShader(s_defaultShader);
            material->SetAlbedo(md.albedo);
            material->SetRoughness(md.roughness);
            material->SetMetallic(md.metallic);
            material->SetEmissive(md.emissive);

            auto createMap = [&](const std::shared_ptr<DecodedImage>& image, MaterialMapType type)
            {
                if (!image || image->pixels.empty())
                    return;

                Texture* tex = new Texture();
                bool isColor = (type == MaterialMapType::Albedo || type == MaterialMapType::Emissive);
                if (tex->LoadFromMemory(image->pixels.data(), image->width, image->height, image->channels, isColor))
                    material->SetMaterialMap(type, tex);
                else
                    delete tex;
            };

            createMap(md.mapAlbedo, MaterialMapType::Albedo);
            createMap(md.mapMetallicRoughness, MaterialMapType::MetallicRoughness);
            createMap(md.mapNormal, MaterialMapType::Normal);
            createMap(md.mapAO, MaterialMapType::AO);
            createMap(md.mapEmissive, MaterialMapType::Emissive);

            materials[i] = material;
        }

        // Create default material
        Material* defaultMaterial = new Material();
        defaultMaterial->SetShader(s_defaultShader);
        defaultMaterial->SetAlbedo(Color(200, 200, 200, 255));
        defaultMaterial->SetRoughness(0.5f);
        defaultMaterial->SetMetallic(0.0f);

        // Create meshes on the main thread (Mesh registration is not thread-safe)
        std::vector<std::shared_ptr<Mesh>> allMeshes;
        allMeshes.reserve(meshData.size());

        for (auto& data : meshData)
        {
            Material* mat = defaultMaterial;
            if (data.materialIndex >= 0 && static_cast<size_t>(data.materialIndex) < materials.size() && materials[data.materialIndex])
                mat = materials[data.materialIndex];

            auto newMesh = std::make_shared<Mesh>();
            newMesh->SetSkinned(false);
            newMesh->SetMaterial(mat);
            newMesh->SetVertices(std::move(data.vertices));
            newMesh->SetIndices(std::move(data.indices));
            allMeshes.push_back(newMesh);
        }

        for (auto& mesh : allMeshes)
        {
            if (!mergeMeshes)
                mesh->Upload();

            model->AddMesh(mesh);
        }

        if (mergeMeshes)
        {
            if (!model->MergeMeshes())
            {
                for (auto& mesh : model->GetMeshes())
                    mesh.get()->Upload();
            }
        }

        return model;
    }
}
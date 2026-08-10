#include "Mesh.h"

namespace cx
{
    std::vector<Mesh*> Mesh::s_meshes;

    Mesh::Mesh()
        : m_vbh(BGFX_INVALID_HANDLE)
        , m_dvbh(BGFX_INVALID_HANDLE)
        , m_ibh(BGFX_INVALID_HANDLE)
        , m_uploaded(false)
        , m_skinned(false)
        , m_material(nullptr)
    {
        s_meshes.push_back(this);
    }

    Mesh::~Mesh()
    {
        Destroy();
    }

    Mesh::Mesh(const Mesh& other)
        : m_vertices(other.m_vertices)
        , m_verticesOriginal(other.m_verticesOriginal)
        , m_indices(other.m_indices)
        , m_vbh(BGFX_INVALID_HANDLE)
        , m_dvbh(BGFX_INVALID_HANDLE)
        , m_ibh(BGFX_INVALID_HANDLE)
        , m_morphTargets(other.m_morphTargets)
        , m_morphWeights(other.m_morphWeights)
        , m_dynamic(other.m_dynamic)
        , m_uploaded(false)
        , m_skinned(other.m_skinned)
        , m_material(other.m_material)
        , m_nodeIndex(other.m_nodeIndex)
    {
        Upload();
        s_meshes.push_back(this);
    }

    void Mesh::SetVertices(const std::vector<Vertex>& vertices)
    {
        m_vertices = vertices;
        m_verticesOriginal = vertices;
        m_uploaded = false;
    }

    void Mesh::SetVertices(std::vector<Vertex>&& vertices)
    {
        m_vertices = std::move(vertices);
        m_verticesOriginal = m_vertices;
        m_uploaded = false;
    }

    void Mesh::SetIndices(const std::vector<uint32_t>& indices)
    {
        m_indices = indices;
        m_uploaded = false;
    }

    void Mesh::SetIndices(std::vector<uint32_t>&& indices)
    {
        m_indices = std::move(indices);
        m_uploaded = false;
    }

    std::vector<Vertex>& Mesh::GetVertices()
    {
        return m_vertices;
    }

    std::vector<uint32_t>& Mesh::GetIndices()
    {
        return m_indices;
    }

    int Mesh::GetTriangleCount()
    {
        if (!m_indices.empty())
            return m_indices.size() / 3;
        else
            return m_vertices.size() / 3;
    }

    void Mesh::Upload()
    {
        if (m_uploaded || m_vertices.empty() || m_indices.empty())
            return;

        bgfx::VertexLayout layout;
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord1, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Indices, 4, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Weight, 4, bgfx::AttribType::Float)
            .end();

        const bgfx::Memory* vbMem = bgfx::copy(m_vertices.data(), static_cast<uint32_t>(m_vertices.size() * sizeof(Vertex)));
        if (m_dynamic)
            m_dvbh = bgfx::createDynamicVertexBuffer(vbMem, layout);
        else
            m_vbh = bgfx::createVertexBuffer(vbMem, layout);
        const bgfx::Memory* ibMem = bgfx::copy(m_indices.data(), static_cast<uint32_t>(m_indices.size() * sizeof(uint32_t)));
        m_ibh = bgfx::createIndexBuffer(ibMem, BGFX_BUFFER_INDEX32);

        m_uploaded = true;
    }

    void Mesh::Destroy()
    {
        if (bgfx::isValid(m_vbh))
        {
            bgfx::destroy(m_vbh);
            m_vbh = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(m_dvbh))
        {
            bgfx::destroy(m_dvbh);
            m_dvbh = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(m_ibh))
        {
            bgfx::destroy(m_ibh);
            m_ibh = BGFX_INVALID_HANDLE;
        }

        m_uploaded = false;

        for (size_t i = 0; i < s_meshes.size(); ++i)
        {
            if (s_meshes[i] == this)
            {
                if (i != s_meshes.size() - 1)
                    std::swap(s_meshes[i], s_meshes.back());

                s_meshes.pop_back();
                return;
            }
        }
    }

    void Mesh::UpdateBuffer()
    {
        if (!m_dynamic || !bgfx::isValid(m_dvbh) || m_vertices.empty())
            return;

        const bgfx::Memory* mem = bgfx::copy(m_vertices.data(), static_cast<uint32_t>(m_vertices.size() * sizeof(Vertex)));
        bgfx::update(m_dvbh, 0, mem);
    }

    void Mesh::ApplyMorphTargets()
    {
        if (m_morphTargets.empty())
            return;

        if (m_verticesOriginal.size() != m_vertices.size())
            m_verticesOriginal = m_vertices;

        size_t numVertices = m_vertices.size();
        size_t numTargets = m_morphTargets.size();

        for (size_t i = 0; i < numVertices; ++i)
        {
            Vector3 morphedPos = m_verticesOriginal[i].position;
            Vector3 morphedNormal = m_verticesOriginal[i].normal;
            Vector3 morphedTangent(m_verticesOriginal[i].tangent.x, m_verticesOriginal[i].tangent.y, m_verticesOriginal[i].tangent.z);
            bool hasTangentDeltas = false;

            for (size_t t = 0; t < numTargets; ++t)
            {
                if (t >= m_morphWeights.size())
                    break;

                float weight = m_morphWeights[t];
                if (weight == 0.0f)
                    continue;

                const MorphTarget& target = m_morphTargets[t];

                if (i < target.positionDeltas.size())
                    morphedPos += target.positionDeltas[i] * weight;

                if (i < target.normalDeltas.size())
                    morphedNormal += target.normalDeltas[i] * weight;

                if (i < target.tangentDeltas.size())
                {
                    morphedTangent += target.tangentDeltas[i] * weight;
                    hasTangentDeltas = true;
                }
            }

            m_vertices[i].position = morphedPos;
            m_vertices[i].normal = morphedNormal.Normalize();
            if (hasTangentDeltas)
            {
                Vector3 normalizedTangent = morphedTangent.Normalize();
                m_vertices[i].tangent = Vector4(normalizedTangent.x, normalizedTangent.y, normalizedTangent.z, m_verticesOriginal[i].tangent.w);
            }
        }
    }

    void Mesh::SetMaterial(Material* material)
    {
        // Todo: Should this be set by default?
        //if (m_material && material)
        //    material->SetShaderParam("u_IsSkinned", m_material->GetShaderParam("u_IsSkinned"));

        m_material = material;
    }

    void Mesh::SetSkinned(bool skinned)
    {
        m_skinned = skinned;

        if (m_material)
            m_material->SetShaderParam("u_IsSkinned", skinned);
    }
}
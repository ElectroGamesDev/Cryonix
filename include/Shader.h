#pragma once
#include <string>
#include <unordered_map>
#include <cstdint>
#include "Texture.h"
#include <variant>
#include <array>
#include <bgfx.h>

namespace cl
{
    struct ShaderImpl;

    enum class UniformType
    {
        Vec4, // Used for float, int, vec2, vec3, and vec4
        Mat3,
        Mat4,
        Sampler
    };

    struct ShaderUniform
    {
        std::string name;
        UniformType type;
        std::variant<
            float,
            int,
            std::array<float, 2>,
            std::array<float, 3>,
            std::array<float, 4>,
            std::array<float, 16>,
            Texture*> value;
    };

    class Shader
    {
        friend class Material;
    public:
        static std::vector<Shader*> s_shaders; // Used for cl::Shutdown()

        Shader();
        ~Shader();

        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;

        Shader(Shader&& other) noexcept;
        Shader& operator=(Shader&& other) noexcept;

        bool Load(const std::string& vertexPath, const std::string& fragmentPath);
        void Destroy();
        bool IsValid() const;

        // Uniform setters
        void SetUniform(const std::string& name, const float v);
        void SetUniform(const std::string& name, const int v);
        void SetUniform(const std::string& name, const float(&v2)[2]);
        void SetUniform(const std::string& name, const float(&v3)[3]);
        void SetUniform(const std::string& name, const float(&v4)[4]);
        void SetUniform(const std::string& name, const float(&m4)[16]);
        void SetUniform(const std::string& name, Texture* texture);

        bool HasUniform(const std::string& name) const;

        bgfx::ProgramHandle GetHandle() const;

        /// <summary>
        /// Applys the shader uniforms. WARNING: This should be only used internally.
        /// </summary>
        void ApplyUniforms();

    private:
        ShaderImpl* m_impl;
        std::vector<ShaderUniform> m_Uniforms;

        void* LoadShaderFile(const std::string& path) const;

        void* GetOrCreateUniform(const std::string& name, UniformType type, uint16_t num) const;
        void SetUniformInternal(const std::string& name, const float v) const;
        void SetUniformInternal(const std::string& name, const int v) const;
        void SetUniformInternal(const std::string& name, const float(&v2)[2]) const;
        void SetUniformInternal(const std::string& name, const float(&v3)[3]) const;
        void SetUniformInternal(const std::string& name, const float(&v4)[4]) const;
        void SetUniformInternal(const std::string& name, const float(&m4)[16]) const;
        void SetUniformInternal(const std::string& name, const Texture* texture) const;
    };

    extern Shader* s_defaultShader;
    Shader* LoadDefaultShader(const std::string& vertexPath, const std::string& fragmentPath);
    Shader* GetDefaultShader();
}
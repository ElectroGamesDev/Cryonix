#pragma once

#include <bgfx.h>
#include <vector>
#include <string>
#include "Maths.h"

namespace cl
{
    class Texture
    {
    public:
        static std::vector<Texture*> s_textures; // Used for cl::Shutdown()

        /// Called once per frame to process async readback operations. WARNING: This should only be used internally!
        static void ProcessPendingReadbacks(uint32_t currentFrame);

        Texture();
        ~Texture();

        // Loading functions
        bool LoadFromFile(std::string_view path, bool isColorTexture = true);
        bool LoadFromMemory(const void* data, int width, int height, int channels, bool isColorTexture = true);

        // Creation functions
        bool CreateEmpty(int width, int height, int channels = 4, bool isColorTexture = true);
        bool CreateSolidColor(int width, int height, const Color& color);
        bool CreateCheckerboard(int width, int height, int checkerSize = 8, const Color& color1 = Color::White(), const Color& color2 = Color::Black());

        // Save functions
        /// Save texture to file. Supports PNG, JPG, TGA, and BMP.
        bool SaveToFile(std::string_view path);
        bool SaveToPNG(std::string_view path);
        bool SaveToJPG(std::string_view path, int quality = 100);
        bool SaveToTGA(std::string_view path);
        bool SaveToBMP(std::string_view path);

        // Pixel data access
        bool GetPixelData(std::vector<uint8_t>& outData);
        bool SetPixelData(const void* data, int channels);
        Color GetPixel(int x, int y);
        void SetPixel(int x, int y, const Color& color);
        bool IsCacheReady() const { return m_cachePixelData && !m_cachedPixelData.empty(); }

        // Information getters
        bgfx::TextureHandle GetHandle() const { return m_handle; }
        int GetWidth() const { return m_width; }
        int GetHeight() const { return m_height; }
        int GetChannels() const { return m_channels; }
        std::string GetFilePath() const { return m_filePath; }
        bgfx::TextureFormat::Enum GetFormat() const { return m_format; }
        bool IsColorTexture() const { return m_isColorTexture; }
        bool IsValid() const { return bgfx::isValid(m_handle); }
        bool HasMipmaps() const { return m_hasMipmaps; }
        float GetAspectRatio() const { return m_width > 0 ? (float)m_width / m_height : 0.0f; }

        // Texture operations
        bool Resize(int newWidth, int newHeight);
        bool GenerateMipmaps();
        bool FlipVertical();
        bool FlipHorizontal();
        bool Rotate90(bool clockwise = true);
        bool Grayscale();
        bool Invert();
        bool ApplyTint(const Color& color);

        // Utility functions
        void Destroy();
        Texture* Clone() const;
        bool CopyFrom(const Texture& other);
        bool LoadPixelDataToCache();

    private:
        enum class PendingOpType
        {
            Resize,
            FlipVertical,
            FlipHorizontal,
            Rotate90CW,
            Rotate90CCW,
            Grayscale,
            Invert,
            ApplyTint,
            SetPixel
        };

        struct PendingOperation
        {
            PendingOpType type;
            int param1;
            int param2;
            Color color;
        };

        struct ReadbackRequest
        {
            Texture* texture;
            uint32_t finishedFrame;
            bgfx::TextureHandle stagingTexture;
            std::vector<PendingOperation> pendingOps;
        };

        static std::vector<ReadbackRequest> s_pendingReadbacks;

        bgfx::TextureHandle m_handle;
        int m_width;
        int m_height;
        int m_channels;
        bgfx::TextureFormat::Enum m_format;
        bool m_isColorTexture;
        bool m_hasMipmaps;
        std::string m_filePath;
        bool m_cachePixelData;
        std::vector<uint8_t> m_cachedPixelData;
        bool m_readbackPending;

        bool UpdateTextureFromCache();
        bgfx::TextureFormat::Enum ChannelsToFormat(int channels) const;
        bool EnsureCacheLoaded();
        void QueueOperation(PendingOpType type, int param1 = 0, int param2 = 0, const Color& color = Color());
        bool ExecuteOperation(const PendingOperation& op);
        bgfx::TextureHandle CreateStagingTexture();
        static int GetFormatChannels(bgfx::TextureFormat::Enum format);
    };
}
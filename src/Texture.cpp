#include "Texture.h"
#include <bimg.h>
#include <decode.h>
#include <bx/bx.h>
#include <bx/allocator.h>
#include <bx/file.h>
#include <iostream>
#include <cstring>
#include <algorithm>

// For stb_image_write
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace cx
{
    std::vector<Texture*> Texture::s_textures;
    std::vector<Texture::ReadbackRequest> Texture::s_pendingReadbacks;
    static bx::DefaultAllocator s_allocator;

    Texture::Texture()
        : m_handle(BGFX_INVALID_HANDLE)
        , m_width(0)
        , m_height(0)
        , m_channels(0)
        , m_format(bgfx::TextureFormat::Unknown)
        , m_isColorTexture(true)
        , m_hasMipmaps(false)
        , m_cachePixelData(false)
        , m_readbackPending(false)
    {
        s_textures.push_back(this);
    }

    Texture::~Texture()
    {
        Destroy();

        // Remove from pending readbacks and cleanup staging textures
        for (auto it = s_pendingReadbacks.begin(); it != s_pendingReadbacks.end();)
        {
            if (it->texture == this)
            {
                if (bgfx::isValid(it->stagingTexture))
                    bgfx::destroy(it->stagingTexture);
                it = s_pendingReadbacks.erase(it);
            }
            else
                ++it;
        }

        for (size_t i = 0; i < s_textures.size(); ++i)
        {
            if (s_textures[i] == this)
            {
                if (i != s_textures.size() - 1)
                    std::swap(s_textures[i], s_textures.back());

                s_textures.pop_back();
                return;
            }
        }
    }

    bool Texture::LoadFromFile(std::string_view path, bool isColorTexture)
    {
        bx::FileReaderI* reader = BX_NEW(&s_allocator, bx::FileReader);
        if (!bx::open(reader, path.data()))
        {
            bx::deleteObject(&s_allocator, reader);
            return false;
        }

        uint32_t size = (uint32_t)bx::getSize(reader);
        void* data = bx::alloc(&s_allocator, size);
        bx::read(reader, data, size, bx::ErrorAssert{});
        bx::close(reader);
        bx::deleteObject(&s_allocator, reader);

        bimg::ImageContainer* imageContainer = bimg::imageParse(&s_allocator, data, size);
        if (!imageContainer)
        {
            bx::free(&s_allocator, data);
            return false;
        }

        uint64_t flags = BGFX_TEXTURE_NONE;
        if (isColorTexture)
            flags |= BGFX_TEXTURE_SRGB;

        const bgfx::Memory* mem = bgfx::makeRef(imageContainer->m_data, imageContainer->m_size);
        m_handle = bgfx::createTexture2D(
            uint16_t(imageContainer->m_width),
            uint16_t(imageContainer->m_height),
            imageContainer->m_numMips > 1,
            imageContainer->m_numLayers,
            bgfx::TextureFormat::Enum(imageContainer->m_format),
            flags,
            mem
        );

        m_width = imageContainer->m_width;
        m_height = imageContainer->m_height;
        m_format = bgfx::TextureFormat::Enum(imageContainer->m_format);
        m_channels = GetFormatChannels(m_format);
        m_isColorTexture = isColorTexture;
        m_hasMipmaps = imageContainer->m_numMips > 1;
        m_filePath = std::string(path);

        bimg::imageFree(imageContainer);
        bx::free(&s_allocator, data);

        return bgfx::isValid(m_handle);
    }

    bool Texture::LoadFromMemory(const void* data, int width, int height, int channels, bool isColorTexture)
    {
        if (!data || width <= 0 || height <= 0)
        {
            std::cerr << "[ERROR] Failed to load texture from memory." << std::endl;
            return false;
        }

        bgfx::TextureFormat::Enum format = ChannelsToFormat(channels);
        uint64_t flags = BGFX_TEXTURE_NONE;
        if (isColorTexture)
            flags |= BGFX_TEXTURE_SRGB;

        const bgfx::Memory* mem = bgfx::copy(data, static_cast<uint32_t>(width * height * channels));
        m_handle = bgfx::createTexture2D(static_cast<uint16_t>(width), static_cast<uint16_t>(height), false, 1, format, flags, mem);

        m_width = width;
        m_height = height;
        m_channels = channels;
        m_format = format;
        m_isColorTexture = isColorTexture;
        m_hasMipmaps = false;

        bool valid = bgfx::isValid(m_handle);
        if (!valid)
            std::cerr << "[ERROR] Failed to load texture from memory." << std::endl;

        if (valid && m_cachePixelData)
        {
            m_cachedPixelData.resize(width * height * channels);
            std::memcpy(m_cachedPixelData.data(), data, width * height * channels);
        }

        return valid;
    }

    bool Texture::CreateEmpty(int width, int height, int channels, bool isColorTexture)
    {
        std::vector<uint8_t> emptyData(width * height * channels, 0);
        return LoadFromMemory(emptyData.data(), width, height, channels, isColorTexture);
    }

    bool Texture::CreateSolidColor(int width, int height, const Color& color)
    {
        std::vector<uint8_t> data(width * height * 4);

        for (int i = 0; i < width * height; ++i)
        {
            data[i * 4 + 0] = color.r;
            data[i * 4 + 1] = color.g;
            data[i * 4 + 2] = color.b;
            data[i * 4 + 3] = color.a;
        }

        return LoadFromMemory(data.data(), width, height, 4, true);
    }

    bool Texture::CreateCheckerboard(int width, int height, int checkerSize, const Color& color1, const Color& color2)
    {
        std::vector<uint8_t> data(width * height * 4);

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                bool isColor1 = ((x / checkerSize) + (y / checkerSize)) % 2 == 0;
                const Color& color = isColor1 ? color1 : color2;

                int idx = (y * width + x) * 4;
                data[idx + 0] = color.r;
                data[idx + 1] = color.g;
                data[idx + 2] = color.b;
                data[idx + 3] = color.a;
            }
        }

        return LoadFromMemory(data.data(), width, height, 4, true);
    }

    bool Texture::SaveToFile(std::string_view path)
    {
        std::string pathStr(path);
        std::string ext = pathStr.substr(pathStr.find_last_of('.') + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == "png")
            return SaveToPNG(path);
        else if (ext == "jpg" || ext == "jpeg")
            return SaveToJPG(path);
        else if (ext == "tga")
            return SaveToTGA(path);
        else if (ext == "bmp")
            return SaveToBMP(path);

        std::cerr << "[ERROR] Unsupported file format: " << ext << std::endl;
        return false;
    }

    bool Texture::SaveToPNG(std::string_view path)
    {
        std::vector<uint8_t> pixelData;
        if (!GetPixelData(pixelData))
            return false;

        int channels = m_channels > 0 ? m_channels : 4;
        return stbi_write_png(path.data(), m_width, m_height, channels, pixelData.data(), m_width * channels) != 0;
    }

    bool Texture::SaveToJPG(std::string_view path, int quality)
    {
        std::vector<uint8_t> pixelData;
        if (!GetPixelData(pixelData))
            return false;

        int channels = m_channels > 0 ? m_channels : 4;
        return stbi_write_jpg(path.data(), m_width, m_height, channels, pixelData.data(), quality) != 0;
    }

    bool Texture::SaveToTGA(std::string_view path)
    {
        std::vector<uint8_t> pixelData;
        if (!GetPixelData(pixelData))
            return false;

        int channels = m_channels > 0 ? m_channels : 4;
        return stbi_write_tga(path.data(), m_width, m_height, channels, pixelData.data()) != 0;
    }

    bool Texture::SaveToBMP(std::string_view path)
    {
        std::vector<uint8_t> pixelData;
        if (!GetPixelData(pixelData))
            return false;

        int channels = m_channels > 0 ? m_channels : 4;
        return stbi_write_bmp(path.data(), m_width, m_height, channels, pixelData.data()) != 0;
    }

    bool Texture::GetPixelData(std::vector<uint8_t>& outData)
    {
        if (!IsValid())
            return false;

        if (!EnsureCacheLoaded())
        {
            std::cerr << "[WARNING] GetPixelData: Failed to get pixel data. The pixel data is either not cached yet or the pixel data is empty. Cache will be available in a few frames. You can call LoadPixelDataToCache() to pre-load the cache." << std::endl;
            return false;
        }

        outData = m_cachedPixelData;
        return true;
    }

    bool Texture::SetPixelData(const void* data, int channels)
    {
        if (!IsValid() || !data)
            return false;

        const bgfx::Memory* mem = bgfx::copy(data, m_width * m_height * channels);
        bgfx::updateTexture2D(m_handle, 0, 0, 0, 0, m_width, m_height, mem);

        if (m_cachePixelData)
        {
            m_cachedPixelData.resize(m_width * m_height * channels);
            std::memcpy(m_cachedPixelData.data(), data, m_width * m_height * channels);
        }

        return true;
    }

    Color Texture::GetPixel(int x, int y)
    {
        if (x < 0 || x >= m_width || y < 0 || y >= m_height)
            return Color();

        if (!EnsureCacheLoaded())
        {
            std::cerr << "[WARNING] GetPixel: Failed to get pixel. The pixel data is either not cached yet or the pixel data is empty. Cache will be available in a few frames. You can call LoadPixelDataToCache() to pre-load the cache." << std::endl;
            return Color();
        }

        int channels = m_channels > 0 ? m_channels : 4;
        int idx = (y * m_width + x) * channels;

        uint8_t r = channels > 0 ? m_cachedPixelData[idx + 0] : 0;
        uint8_t g = channels > 1 ? m_cachedPixelData[idx + 1] : 0;
        uint8_t b = channels > 2 ? m_cachedPixelData[idx + 2] : 0;
        uint8_t a = channels > 3 ? m_cachedPixelData[idx + 3] : 255;

        return Color(r, g, b, a);
    }

    void Texture::SetPixel(int x, int y, const Color& color)
    {
        if (x < 0 || x >= m_width || y < 0 || y >= m_height)
            return;

        if (!EnsureCacheLoaded())
        {
            QueueOperation(PendingOpType::SetPixel, x, y, color);
            return;
        }

        int channels = m_channels > 0 ? m_channels : 4;
        int idx = (y * m_width + x) * channels;

        if (channels > 0)
            m_cachedPixelData[idx + 0] = color.r;

        if (channels > 1)
            m_cachedPixelData[idx + 1] = color.g;

        if (channels > 2)
            m_cachedPixelData[idx + 2] = color.b;

        if (channels > 3)
            m_cachedPixelData[idx + 3] = color.a;

        UpdateTextureFromCache();
    }

    bool Texture::Resize(int newWidth, int newHeight)
    {
        if (!IsValid())
            return false;

        if (!EnsureCacheLoaded())
        {
            QueueOperation(PendingOpType::Resize, newWidth, newHeight);
            return true;
        }

        std::vector<uint8_t> newData(newWidth * newHeight * m_channels);

        for (int y = 0; y < newHeight; ++y)
        {
            for (int x = 0; x < newWidth; ++x)
            {
                int srcX = x * m_width / newWidth;
                int srcY = y * m_height / newHeight;
                int srcIdx = (srcY * m_width + srcX) * m_channels;
                int dstIdx = (y * newWidth + x) * m_channels;

                for (int c = 0; c < m_channels; ++c)
                    newData[dstIdx + c] = m_cachedPixelData[srcIdx + c];
            }
        }

        Destroy();
        return LoadFromMemory(newData.data(), newWidth, newHeight, m_channels, m_isColorTexture);
    }

    bool Texture::GenerateMipmaps()
    {
        if (!IsValid())
            return false;

        if (!EnsureCacheLoaded())
        {
            std::cerr << "[WARNING] GenerateMipmaps: Waiting for cache to load..." << std::endl;
            return false;
        }

        Destroy();

        uint64_t flags = BGFX_TEXTURE_NONE;
        if (m_isColorTexture)
            flags |= BGFX_TEXTURE_SRGB;

        const bgfx::Memory* mem = bgfx::copy(m_cachedPixelData.data(), m_width * m_height * m_channels);
        m_handle = bgfx::createTexture2D(m_width, m_height, true, 1, m_format, flags, mem);
        m_hasMipmaps = true;

        return bgfx::isValid(m_handle);
    }

    bool Texture::FlipVertical()
    {
        if (!IsValid())
            return false;

        if (!EnsureCacheLoaded())
        {
            QueueOperation(PendingOpType::FlipVertical);
            return true;
        }

        int rowSize = m_width * m_channels;
        std::vector<uint8_t> tempRow(rowSize);

        for (int y = 0; y < m_height / 2; ++y)
        {
            int topIdx = y * rowSize;
            int botIdx = (m_height - 1 - y) * rowSize;

            std::memcpy(tempRow.data(), &m_cachedPixelData[topIdx], rowSize);
            std::memcpy(&m_cachedPixelData[topIdx], &m_cachedPixelData[botIdx], rowSize);
            std::memcpy(&m_cachedPixelData[botIdx], tempRow.data(), rowSize);
        }

        return UpdateTextureFromCache();
    }

    bool Texture::FlipHorizontal()
    {
        if (!IsValid())
            return false;

        if (!EnsureCacheLoaded())
        {
            QueueOperation(PendingOpType::FlipHorizontal);
            return true;
        }

        for (int y = 0; y < m_height; ++y)
        {
            for (int x = 0; x < m_width / 2; ++x)
            {
                int leftIdx = (y * m_width + x) * m_channels;
                int rightIdx = (y * m_width + (m_width - 1 - x)) * m_channels;

                for (int c = 0; c < m_channels; ++c)
                    std::swap(m_cachedPixelData[leftIdx + c], m_cachedPixelData[rightIdx + c]);
            }
        }

        return UpdateTextureFromCache();
    }

    bool Texture::Rotate90(bool clockwise)
    {
        if (!IsValid())
            return false;

        if (!EnsureCacheLoaded())
        {
            QueueOperation(clockwise ? PendingOpType::Rotate90CW : PendingOpType::Rotate90CCW);
            return true;
        }

        std::vector<uint8_t> rotated(m_width * m_height * m_channels);

        for (int y = 0; y < m_height; ++y)
        {
            for (int x = 0; x < m_width; ++x)
            {
                int srcIdx = (y * m_width + x) * m_channels;
                int dstIdx;

                if (clockwise)
                    dstIdx = (x * m_height + (m_height - 1 - y)) * m_channels;
                else
                    dstIdx = ((m_width - 1 - x) * m_height + y) * m_channels;

                for (int c = 0; c < m_channels; ++c)
                    rotated[dstIdx + c] = m_cachedPixelData[srcIdx + c];
            }
        }

        Destroy();
        return LoadFromMemory(rotated.data(), m_height, m_width, m_channels, m_isColorTexture);
    }

    bool Texture::Grayscale()
    {
        if (!IsValid() || m_channels < 3)
            return false;

        if (!EnsureCacheLoaded())
        {
            QueueOperation(PendingOpType::Grayscale);
            return true;
        }

        for (int i = 0; i < m_width * m_height; ++i)
        {
            int idx = i * m_channels;
            uint8_t r = m_cachedPixelData[idx + 0];
            uint8_t g = m_cachedPixelData[idx + 1];
            uint8_t b = m_cachedPixelData[idx + 2];
            uint8_t gray = static_cast<uint8_t>(0.299f * r + 0.587f * g + 0.114f * b);

            m_cachedPixelData[idx + 0] = gray;
            m_cachedPixelData[idx + 1] = gray;
            m_cachedPixelData[idx + 2] = gray;
        }

        return UpdateTextureFromCache();
    }

    bool Texture::Invert()
    {
        if (!IsValid())
            return false;

        if (!EnsureCacheLoaded())
        {
            QueueOperation(PendingOpType::Invert);
            return true;
        }

        for (int i = 0; i < m_width * m_height; ++i)
        {
            for (int c = 0; c < m_channels; ++c)
            {
                if (c < 3)
                    m_cachedPixelData[i * m_channels + c] = 255 - m_cachedPixelData[i * m_channels + c];
            }
        }

        return UpdateTextureFromCache();
    }

    bool Texture::ApplyTint(const Color& color)
    {
        if (!IsValid() || m_channels < 3)
            return false;

        if (!EnsureCacheLoaded())
        {
            QueueOperation(PendingOpType::ApplyTint, 0, 0, color);
            return true;
        }

        float tintR = color.r / 255.0f;
        float tintG = color.g / 255.0f;
        float tintB = color.b / 255.0f;

        for (int i = 0; i < m_width * m_height; ++i)
        {
            int idx = i * m_channels;
            m_cachedPixelData[idx + 0] = static_cast<uint8_t>(m_cachedPixelData[idx + 0] * tintR);
            m_cachedPixelData[idx + 1] = static_cast<uint8_t>(m_cachedPixelData[idx + 1] * tintG);
            m_cachedPixelData[idx + 2] = static_cast<uint8_t>(m_cachedPixelData[idx + 2] * tintB);
        }

        return UpdateTextureFromCache();
    }

    void Texture::Destroy()
    {
        if (bgfx::isValid(m_handle))
        {
            bgfx::destroy(m_handle);
            m_handle = BGFX_INVALID_HANDLE;
        }

        m_width = 0;
        m_height = 0;
        m_channels = 0;
        m_format = bgfx::TextureFormat::Unknown;
        m_hasMipmaps = false;
        m_filePath.clear();
        m_cachedPixelData.clear();
        m_readbackPending = false;
    }

    Texture* Texture::Clone() const
    {
        if (!IsValid())
            return nullptr;

        Texture* clone = new Texture();
        clone->CopyFrom(*this);
        return clone;
    }

    bool Texture::CopyFrom(const Texture& other)
    {
        if (!other.IsValid())
            return false;

        if (other.m_cachedPixelData.empty())
        {
            std::cerr << "[WARNING] Cannot copy texture without cached pixel data" << std::endl;
            return false;
        }

        Destroy();

        m_cachePixelData = other.m_cachePixelData;
        bool result = LoadFromMemory(other.m_cachedPixelData.data(), other.m_width, other.m_height,
            other.m_channels, other.m_isColorTexture);

        if (result)
        {
            m_filePath = other.m_filePath;
            m_hasMipmaps = other.m_hasMipmaps;
        }

        return result;
    }

    int Texture::GetFormatChannels(bgfx::TextureFormat::Enum format)
    {
        switch (format)
        {
        case bgfx::TextureFormat::R8:
        case bgfx::TextureFormat::R16:
        case bgfx::TextureFormat::R16F:
        case bgfx::TextureFormat::R32F:
            return 1;

        case bgfx::TextureFormat::RG8:
        case bgfx::TextureFormat::RG16:
        case bgfx::TextureFormat::RG16F:
        case bgfx::TextureFormat::RG32F:
            return 2;

        case bgfx::TextureFormat::RGB8:
        case bgfx::TextureFormat::RGB9E5F:
            return 3;

        case bgfx::TextureFormat::RGBA8:
        case bgfx::TextureFormat::RGBA16:
        case bgfx::TextureFormat::RGBA16F:
        case bgfx::TextureFormat::RGBA32F:
        case bgfx::TextureFormat::BGRA8:
            return 4;

        default:
            return 4;
        }
    }

    bool Texture::LoadPixelDataToCache()
    {
        if (!IsValid() || m_readbackPending || !m_cachedPixelData.empty())
            return false;

        // Create staging texture
        bgfx::TextureHandle stagingTexture = CreateStagingTexture();
        if (!bgfx::isValid(stagingTexture))
        {
            std::cerr << "[ERROR] Failed to create staging texture for readback" << std::endl;
            return false;
        }

        // Blit from main texture to staging texture
        bgfx::blit(bgfx::ViewId(255), stagingTexture, 0, 0, m_handle, 0, 0, m_width, m_height);

        int dataSize = m_width * m_height * m_channels;
        m_cachedPixelData.resize(dataSize);

        ReadbackRequest request;
        request.texture = this;
        request.stagingTexture = stagingTexture;
        request.finishedFrame = bgfx::readTexture(stagingTexture, m_cachedPixelData.data());
        s_pendingReadbacks.push_back(request);

        m_readbackPending = true;
        m_cachePixelData = false;

        return true;
    }

    bool Texture::UpdateTextureFromCache()
    {
        if (m_cachedPixelData.empty() || !IsValid())
            return false;

        return SetPixelData(m_cachedPixelData.data(), m_channels);
    }

    bgfx::TextureFormat::Enum Texture::ChannelsToFormat(int channels) const
    {
        switch (channels)
        {
        case 1:
            return bgfx::TextureFormat::R8;
        case 2:
            return bgfx::TextureFormat::RG8;
        case 3:
            return bgfx::TextureFormat::RGB8;
        case 4:
            return bgfx::TextureFormat::RGBA8;
        default:
            return bgfx::TextureFormat::RGBA8;
        }
    }

    bool Texture::EnsureCacheLoaded()
    {
        if (m_cachePixelData && !m_cachedPixelData.empty())
            return true;

        if (m_readbackPending)
            return false;

        LoadPixelDataToCache();
        return false;
    }

    void Texture::QueueOperation(PendingOpType type, int param1, int param2, const Color& color)
    {
        for (auto& request : s_pendingReadbacks)
        {
            if (request.texture == this)
            {
                PendingOperation op;
                op.type = type;
                op.param1 = param1;
                op.param2 = param2;
                op.color = color;

                request.pendingOps.push_back(op);
                return;
            }
        }
    }

    bool Texture::ExecuteOperation(const PendingOperation& op)
    {
        switch (op.type)
        {
        case PendingOpType::Resize:
            return Resize(op.param1, op.param2);
        case PendingOpType::FlipVertical:
            return FlipVertical();
        case PendingOpType::FlipHorizontal:
            return FlipHorizontal();
        case PendingOpType::Rotate90CW:
            return Rotate90(true);
        case PendingOpType::Rotate90CCW:
            return Rotate90(false);
        case PendingOpType::Grayscale:
            return Grayscale();
        case PendingOpType::Invert:
            return Invert();
        case PendingOpType::ApplyTint:
            return ApplyTint(op.color);
        case PendingOpType::SetPixel:
            SetPixel(op.param1, op.param2, op.color);
            return true;
        }
        return false;
    }

    bgfx::TextureHandle Texture::CreateStagingTexture()
    {
        uint64_t flags = BGFX_TEXTURE_READ_BACK | BGFX_TEXTURE_BLIT_DST;
        if (m_isColorTexture)
            flags |= BGFX_TEXTURE_SRGB;

        return bgfx::createTexture2D(m_width, m_height, m_hasMipmaps, 1, m_format, flags);
    }

    void Texture::ProcessPendingReadbacks(uint32_t currentFrame)
    {
        for (auto it = s_pendingReadbacks.begin(); it != s_pendingReadbacks.end();)
        {
            if (currentFrame >= it->finishedFrame)
            {
                Texture* tex = it->texture;

                // Check if texture still exists
                bool textureValid = false;
                for (Texture* t : s_textures)
                {
                    if (t == tex)
                    {
                        textureValid = true;
                        break;
                    }
                }

                if (!textureValid)
                {
                    if (bgfx::isValid(it->stagingTexture))
                        bgfx::destroy(it->stagingTexture);
                    it = s_pendingReadbacks.erase(it);
                    continue;
                }

                // Mark cache as ready
                tex->m_readbackPending = false;
                tex->m_cachePixelData = true;

                // Cleanup staging texture
                if (bgfx::isValid(it->stagingTexture))
                    bgfx::destroy(it->stagingTexture);

                // Execute all pending operations
                for (const auto& op : it->pendingOps)
                    tex->ExecuteOperation(op);

                it = s_pendingReadbacks.erase(it);
            }
            else
                ++it;
        }
    }
}
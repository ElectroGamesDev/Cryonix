#include "Crylib.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/include/miniaudio.h"

namespace cl
{
    struct CrylibState
    {
        Window* window;
        Config config;
        bool initialized;
    };

    static CrylibState* s_crylib = nullptr;

    bool Init(const Config& config)
    {
        if (s_crylib)
            return false;

        s_crylib = new CrylibState();
        s_crylib->config = config;
        s_crylib->initialized = false;

        // Create platform window
        s_crylib->window = Window::Create();
        if (!s_crylib->window)
        {
            delete s_crylib;
            s_crylib = nullptr;

            return false;
        }

        // Initialize window
        if (!s_crylib->window->Init(config))
        {
            delete s_crylib->window;
            delete s_crylib;
            s_crylib = nullptr;

            return false;
        }

        // Initialize input system
        Input::Init();

        // Initialize the audio system
        if (config.audioEnabled)
            cl::InitAudioDevice();

        // Initialize renderer
        if (!InitRenderer(s_crylib->window, config))
        {
            Input::Shutdown();
            ShutdownAudioDevice();
            s_crylib->window->Shutdown();
            delete s_crylib->window;
            delete s_crylib;
            s_crylib = nullptr;

            return false;
        }

        s_crylib->initialized = true;
        return true;
    }

    void Update()
    {
        if (!s_crylib || !s_crylib->initialized)
            return;

        // Poll window events
        s_crylib->window->PollEvents();

        // Update input state
        Input::Update();
    }

    void Shutdown()
    {
        if (!s_crylib)
            return;

        // Todo: Add an option in config.h to not automatically handle deleting resources like Shaders, Meshes, etc. since this does cause overhead
        // We can't free the memory here due to not being able to know if its created in the heap or stack. We could techincally make a Create() function which returns a pointer, but
        // it's not that important as the platform should clean up when the program is closed and in most cases, the user should be cleaning up anyway

        // Todo: Use unordered_map instead of Vector, it will be faster
        Model::s_models.clear();
        for (int i = static_cast<int>(Model::s_models.size()) - 1; i >= 0; --i)
            Model::s_models[i]->Destroy();

        Model::s_models.clear();

        for (int i = static_cast<int>(Mesh::s_meshes.size()) - 1; i >= 0; --i)
            Mesh::s_meshes[i]->Destroy();
        Mesh::s_meshes.clear();

        for (int i = static_cast<int>(Shader::s_shaders.size()) - 1; i >= 0; --i)
            Shader::s_shaders[i]->Destroy();
        Shader::s_shaders.clear();

        for (int i = static_cast<int>(Texture::s_textures.size()) - 1; i >= 0; --i)
            Texture::s_textures[i]->Destroy();
        Texture::s_textures.clear();

        // Todo: Add Animation, AnimationClip, Skeletal, etc.

        // Todo: Fix this. Sounds are returned as copies, which therefore the pointers in s_sounds are invalid
        //for (int i = static_cast<int>(s_sounds.size()) - 1; i >= 0; --i)
        //    UnloadSound(*s_sounds[i]);
        //s_sounds.clear();

        //for (int i = static_cast<int>(s_musics.size()) - 1; i >= 0; --i)
        //    UnloadMusicStream(*s_musics[i]);
        //s_musics.clear();

        //for (int i = static_cast<int>(s_audioStreams.size()) - 1; i >= 0; --i)
        //    UnloadAudioStream(*s_audioStreams[i]);
        //s_audioStreams.clear();

        ShutdownRenderer();
        Input::Shutdown();

        if (s_crylib->config.audioEnabled)
            cl::ShutdownAudioDevice();

        if (s_crylib->window)
        {
            s_crylib->window->Shutdown();
            delete s_crylib->window;
            s_crylib->window = nullptr;
        }

        delete s_crylib;
        s_crylib = nullptr;
    }

    bool ShouldClose()
    {
        if (!s_crylib || !s_crylib->window)
            return true;

        return s_crylib->window->ShouldClose();
    }

    void SetWindowTitle(const char* title)
    {
        if (s_crylib && s_crylib->window)
        {
            s_crylib->window->SetWindowTitle(title);
        }
    }

    void GetWindowSize(int& width, int& height)
    {
        if (s_crylib && s_crylib->window)
            s_crylib->window->GetWindowSize(width, height);
        else
        {
            width = 0;
            height = 0;
        }
    }

    Window* GetWindow()
    {
        return s_crylib ? s_crylib->window : nullptr;
    }
}
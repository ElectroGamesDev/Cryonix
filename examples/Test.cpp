#include "Cryonix.h"
#include <iostream>

int main()
{
    cx::Config config;
    config.windowTitle = "Cryonix Test Window";
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.renderingAPI = cx::DirectX12;

    if (!cx::Init(config))
    {
        std::cerr << "Failed to initialize Cryonix!" << std::endl;
        return -1;
    }

    cx::LoadDefaultShader("shaders/vs_default.bin", "shaders/fs_default.bin");

    //cx::Model* model = cx::LoadModel("models/truck/binary/CesiumMilkTruck.glb");
    //cx::Model* model = cx::LoadModel("models/gltf/Tree.glb");
    //cx::Model* model = cx::LoadModel("models/Tree.glb"); // Todo: This isnt loading/rendering properly
    //cx::Model* model = cx::LoadModel("models/Tree3.fbx");
    //cx::Model* model = cx::LoadModel("models/Animated Character/Character_anim.fbx");
    //cx::Model* model = cx::LoadModel("models/gltf/Sponza/source/scene.glb");
    //cx::Model* model = cx::LoadModel("models/OBJ/sibenik/sibenik.obj");
    cx::Model* model = cx::LoadModel("models/Animation/Character.glb");

    // Primitives
    cx::InitPrimitives();

    cx::Model primitive = cx::GenQuadModel();

    // Camera
    cx::Camera camera(
        { 0, 0, -10 }, // Position
        { 0, 0, 0 }, // Rotation
        { 0, 1, 0}, // Up
        false); // Use Target

    // Animations
    //model->PlayAnimationByIndex(1, true);
    //cx::Model* model = cx::LoadModel("models/gltf/AnimatedCube/AnimatedCube.gltf");
    //cx::Model* model = cx::LoadModel("models/gltf/AnimatedTriangle/AnimatedTriangle.gltf"); // Todo: Not working
    //cx::Model* model = cx::LoadModel("models/Animated Character/Character_anim.fbx"); // Todo: not working
    //model->PlayAnimationByName("animation_AnimatedCube", true);

    // Animation with layers
    int walkLayer = model->GetAnimator()->CreateLayer("Walk", 0);
    model->GetAnimator()->PlayAnimationOnLayer(walkLayer, model->GetAnimation(1), true);

    //model->SetPosition({0,1,0});
    //model->SetRotation({ 0, 90, 0 });
    model->SetRotation({ 90, 180, 0 });
    model->SetScale({0.01f, 0.01f, 0.01f});

    //for (const auto& mesh : model->GetMeshes())
    //{
    //    if (mesh->GetMaterial())
    //        mesh->GetMaterial()->SetShader(cx::GetDefaultShader());
    //}

    //player.SetMaterial(0, bodyMaterial);
    //player.SetMaterial(1, weaponMaterial);

    //player.SetPosition(10.0f, 0.0f, 5.0f);
    //player.SetRotation(0.0f, 45.0f, 0.0f);
    //player.SetScale(1.5f);

    //cx::Model* clonedModel = cx::CloneModel(model);


    // Lighting:
    
    // Lighting can currently be set like this:
    //float lightingControl[4] = { 1.0f, 0.0f, 0.0f, 0.0f }; // x=1.0 enables lighting
    //cx::GetDefaultShader()->SetUniform("u_LightingControl", lightingControl);

    //// Setup lighting uniforms
    //float lightDir[3] = { -0.5f, -1.0f, -0.3f };
    //pbrShader->SetUniform("u_LightDir", lightDir);

    //float lightColor[4] = { 1.0f, 1.0f, 1.0f, 3.0f }; // White light, intensity 3.0
    //pbrShader->SetUniform("u_LightColor", lightColor);

    //float ambientColor[3] = { 0.03f, 0.03f, 0.03f }; // Subtle ambient
    //pbrShader->SetUniform("u_AmbientColor", ambientColor);

    //// Setup material flags (this is automatic in Material::ApplyPBRUniforms)
    //float flags0[4] = { 1.0f, 1.0f, 0.0f, 0.0f }; // hasAlbedo, hasNormal
    //pbrShader->SetUniform("u_MaterialFlags0", flags0);

    //float flags1[4] = { 1.0f, 1.0f, 0.0f, 0.0f }; // hasMetallicRoughness, hasAO
    //pbrShader->SetUniform("u_MaterialFlags1", flags1);

    // But eventually add a way to easily setup lights (assuming the shader supports the uniforms)

    // Todo: Need to make Sounds, Music, and AudioStream return a pointer. Sounds work fine with a copy, but Music doesn't. Not too sure about AudioStream. Pointers are needed to cleanup automatically anyways
    // Sounds
    //cx::Sound shot = cx::LoadSound("sounds/test.mp3");
    //cx::PlaySound(shot);

    // Load and play music
    //cx::Music music = cx::LoadMusicStream("sounds/test.mp3");
    //cx::SetMusicLooping(music, true);
    //cx::PlayMusicStream(music);

    // 3D Audio
    //cx::SetSoundPosition(shot, 10.0f, 0.0f, 5.0f);
    //cx::SetAudioListenerPosition(0.0f, 0.0f, 0.0f);

    // Effects
    //cx::SetMusicEffect(music, cx::AUDIO_EFFECT_LOWPASS, 500.0f);

    // Old Camera setup
    //float view[16];  // View matrix
    //float proj[16];  // Projection matrix

    //// Camera position and orientation
    //bx::Vec3 cameraPos = { 0.0f, 2.0f, -5.0f };   // In front of origin
    //bx::Vec3 target = { 0.0f, 0.0f, 0.0f };    // Look at origin
    //bx::Vec3 up = { 0.0f, 1.0f, 0.0f };    // Up vector

    //bx::mtxLookAt(view, cameraPos, target, up);
    //bx::mtxProj(proj, 60.0f, float(config.windowWidth) / config.windowHeight, 0.1f, 1000.0f, bgfx::getCaps()->homogeneousDepth);

    // Create uniform handles for camera matrices (assuming Cryonix exposes this)
    //static bgfx::UniformHandle u_View = bgfx::createUniform("u_View", bgfx::UniformType::Mat4);
    //static bgfx::UniformHandle u_Proj = bgfx::createUniform("u_Proj", bgfx::UniformType::Mat4);
    //static bgfx::UniformHandle u_CamPos = bgfx::createUniform("u_CameraPos", bgfx::UniformType::Vec4);

    cx::Clear(cx::Color(48, 48, 48, 255)); // This sets the background color. It can go anywhere after cl:Init() and can be used after camera.Begin() to let multiple cameras have different backgrounds
    bgfx::setDebug(BGFX_DEBUG_STATS | BGFX_DEBUG_TEXT);

    while (!cx::ShouldClose())
    {
        cx::Update();
        cx::BeginFrame();

        // Update the camera uniforms
        
        //bgfx::setViewTransform(0, view, proj); // Now being done in Camera

        // Movement // Todo: add delta time once it's implemented
        if (cx::Input::IsKeyDown(cx::KeyCode::W))
            camera.MoveForward(5 * cx::GetFrameTime());

        if (cx::Input::IsKeyDown(cx::KeyCode::A))
            camera.MoveLeft(5 * cx::GetFrameTime());

        if (cx::Input::IsKeyDown(cx::KeyCode::S))
            camera.MoveBackward(5 * cx::GetFrameTime());

        if (cx::Input::IsKeyDown(cx::KeyCode::D))
            camera.MoveRight(5 * cx::GetFrameTime());

        if (cx::Input::IsKeyDown(cx::KeyCode::E))
            camera.MoveUp(5 * cx::GetFrameTime());

        if (cx::Input::IsKeyDown(cx::KeyCode::Q))
            camera.MoveDown(5 * cx::GetFrameTime());

        // Mouse Look
        static float lookSensitivity = 0.1f;
        cx::Vector3 camRotation = camera.GetRotation();
        float deltaX = cx::Input::GetMouseDelta().x;
        float deltaY = cx::Input::GetMouseDelta().y;

        // Apply mouse deltas
        camRotation.y += deltaX * lookSensitivity;
        camRotation.x += deltaY * lookSensitivity;

        // Clamp pitch to avoid flipping
        if (camRotation.x > 89.0f) camRotation.x = 89.0f;
        if (camRotation.x < -89.0f) camRotation.x = -89.0f;

        // Apply rotation
        camera.Rotate(camRotation);

        // Rendering
        cx::BeginCamera(camera);

        cx::GetDefaultShader()->SetUniform("u_CameraPos", { camera.GetPosition().x, camera.GetPosition().y, camera.GetPosition().z });

        static float elapsed = 0.0f;
        elapsed += cx::GetFrameTime();
        if (elapsed > 5.0f)
        {
            model->GetAnimator()->CrossfadeToAnimation(model->GetAnimation(0), 1, true, walkLayer);
            elapsed = -10000;
        }

        // Animations
        model->UpdateAnimation(cx::GetFrameTime());

        //model->Rotate(0.0f, deltaTime * 90.0f, 0.0f); // Todo: Add delta time

        cx::DrawModel(model);

        //cx::DrawModel(&primitive);

        //cx::DrawModel(model, { 0,0,0 }, cx::Quaternion{ 0, 0, 0, 1 }, { 1, 1, 1 });


        // Drawing cloned models example
        
        // Option 1. Using transform parameters
        //cx::DrawModel(model, { 3,0,0 }, { 0,0,0 }, {1,1,1});

        // Option 2. Using model cloned models
        //cx::DrawModel(clonedModel);

        // Instancing example
        //for (int i = 0; i < 1000; i++)
        //    cx::DrawModelInstanced(model, { (float)i,0,0 }, cx::Quaternion{ 0, 0, 0, 1 }, { 1, 1, 1 });

        //cx::SubmitInstances();

        // Shaders

        // shader.SetUniform() sets the shader's global uniform, across all meshes using this uniform
        // material.SetUniform() sets the shader's uniform just for that material

        cx::EndFrame();
    }

    cx::Shutdown();

    return 0;
}
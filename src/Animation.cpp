#include "Animation.h"
#include <algorithm>
#include <iostream>
#include <functional>
#include <map>

namespace cx
{
    std::vector<AnimationClip*> AnimationClip::s_clips;

    AnimationClip::AnimationClip()
        : m_duration(0.0f)
        , m_rootMotionEnabled(false)
        , m_rootBoneIndex(0)
    {
        s_clips.push_back(this);
    }

    AnimationClip::~AnimationClip()
    {
        Destroy();

        for (size_t i = 0; i < s_clips.size(); ++i)
        {
            if (s_clips[i] == this)
            {
                if (i != s_clips.size() - 1)
                    std::swap(s_clips[i], s_clips.back());

                s_clips.pop_back();
                return;
            }
        }
    }

    AnimationClip::AnimationClip(AnimationClip&& other) noexcept
        : m_name(std::move(other.m_name))
        , m_duration(other.m_duration)
        , m_channels(std::move(other.m_channels))
        , m_nodeChannels(std::move(other.m_nodeChannels))
        , m_morphWeightChannels(std::move(other.m_morphWeightChannels))
        , m_rootMotionEnabled(other.IsRootMotionEnabled())
        , m_rootBoneIndex(other.GetRootBoneIndex())
    {
        other.m_duration = 0.0f;
        s_clips.push_back(this);
    }

    AnimationClip& AnimationClip::operator=(AnimationClip&& other) noexcept
    {
        if (this != &other)
        {
            Destroy();
            m_name = std::move(other.m_name);
            m_duration = other.m_duration;
            m_channels = std::move(other.m_channels);
            m_nodeChannels = std::move(other.m_nodeChannels);
            m_morphWeightChannels = std::move(other.m_morphWeightChannels);
            other.m_duration = 0.0f;
        }
        return *this;
    }

    void AnimationClip::Destroy()
    {
        m_channels.clear();
        m_nodeChannels.clear();
        m_morphWeightChannels.clear();
        m_duration = 0.0f;
    }

    void AnimationClip::SortEvents()
    {
        std::sort(m_events.begin(), m_events.end(), [](const AnimationEvent& a, const AnimationEvent& b)
            { return a.time < b.time; });
    }

    void AnimationClip::AddEvent(const AnimationEvent& event)
    {
        m_events.push_back(event);
        SortEvents();
    }

    void Animator::SetStateMachine(AnimationStateMachine* stateMachine)
    {
        m_stateMachine = stateMachine;
    }

    AnimationStateMachine::AnimationStateMachine()
        : m_nextStateId(0)
        , m_currentStateId(-1)
        , m_currentStateTime(0.0f)
        , m_isTransitioning(false)
        , m_transitionTime(0.0f)
        , m_transitionTargetStateId(-1)
        , m_blendedRootMotionDelta(0.0f, 0.0f, 0.0f)
        , m_blendedRootMotionRotation(0.0f, 0.0f, 0.0f, 1.0f)
    {
    }

    AnimationStateMachine::~AnimationStateMachine()
    {
        for (auto& state : m_states)
        {
            if (state.blendTree)
                delete state.blendTree;
        }
    }

    int AnimationStateMachine::CreateState(const std::string& name, AnimationClip* clip)
    {
        AnimationState state;
        state.id = m_nextStateId++;
        state.name = name;
        state.clip = clip;

        m_states.push_back(state);
        m_stateNames[name] = state.id;

        return state.id;
    }

    void AnimationStateMachine::RemoveState(int stateId)
    {
        for (auto it = m_states.begin(); it != m_states.end(); ++it)
        {
            if (it->id == stateId)
            {
                if (it->blendTree)
                    delete it->blendTree;

                // Remove from name map
                for (auto nameIt = m_stateNames.begin(); nameIt != m_stateNames.end(); ++nameIt)
                {
                    if (nameIt->second == stateId)
                    {
                        m_stateNames.erase(nameIt);
                        break;
                    }
                }

                m_states.erase(it);
                return;
            }
        }
    }

    AnimationState* AnimationStateMachine::GetState(int stateId)
    {
        for (auto& state : m_states)
        {
            if (state.id == stateId)
                return &state;
        }

        return nullptr;
    }

    AnimationState* AnimationStateMachine::GetState(const std::string& name)
    {
        auto it = m_stateNames.find(name);
        if (it != m_stateNames.end())
            return GetState(it->second);

        return nullptr;
    }

    void AnimationStateMachine::AddTransition(int fromStateId, int toStateId, float duration)
    {
        AnimationState* fromState = GetState(fromStateId);
        if (!fromState)
            return;

        AnimationTransition transition;
        transition.fromStateId = fromStateId;
        transition.toStateId = toStateId;
        transition.duration = duration;

        fromState->transitions.push_back(transition);
    }

    void AnimationStateMachine::SetTransitionCondition(int fromStateId, int toStateId, const std::string& parameter, AnimationTransition::ConditionType type, float value)
    {
        AnimationState* fromState = GetState(fromStateId);
        if (!fromState)
            return;

        for (auto& transition : fromState->transitions)
        {
            if (transition.toStateId == toStateId)
            {
                transition.conditionParameter = parameter;
                transition.conditionType = type;
                transition.conditionValue = value;
                return;
            }
        }
    }

    void AnimationStateMachine::AddTransitionCondition(int fromStateId, int toStateId, const std::string& parameter, AnimationTransition::ConditionType type, float value)
    {
        AnimationState* fromState = GetState(fromStateId);
        if (!fromState)
            return;

        for (auto& transition : fromState->transitions)
        {
            if (transition.toStateId == toStateId)
            {
                transition.conditions.push_back(AnimationTransition::Condition(parameter, type, value));
                transition.useMultipleConditions = true;
                return;
            }
        }
    }

    void AnimationStateMachine::SetStateLayer(int stateId, int layer)
    {
        AnimationState* state = GetState(stateId);
        if (state)
            state->layer = layer;
    }

    void AnimationStateMachine::SetLayerWeight(int layer, float weight)
    {
        m_layerWeights[layer] = std::max(0.0f, std::min(1.0f, weight));
    }

    float AnimationStateMachine::GetLayerWeight(int layer) const
    {
        auto it = m_layerWeights.find(layer);
        return (it != m_layerWeights.end()) ? it->second : 1.0f;
    }

    void AnimationStateMachine::SetLayerMask(int layer, const std::vector<int>& boneIndices)
    {
        m_layerMasks[layer] = boneIndices;
    }

    void AnimationStateMachine::SetCurrentState(int stateId)
    {
        AnimationState* state = GetState(stateId);
        if (!state)
            return;

        m_currentStateId = stateId;
        m_currentStateTime = 0.0f;
        m_isTransitioning = false;
    }

    AnimationState* AnimationStateMachine::GetCurrentState()
    {
        return GetState(m_currentStateId);
    }

    void AnimationStateMachine::SetParameter(const std::string& name, float value)
    {
        m_floatParameters[name] = value;
    }

    void AnimationStateMachine::SetParameter(const std::string& name, bool value)
    {
        m_boolParameters[name] = value;
    }

    float AnimationStateMachine::GetParameterFloat(const std::string& name) const
    {
        auto it = m_floatParameters.find(name);
        return (it != m_floatParameters.end()) ? it->second : 0.0f;
    }

    bool AnimationStateMachine::GetParameterBool(const std::string& name) const
    {
        auto it = m_boolParameters.find(name);
        return (it != m_boolParameters.end()) ? it->second : false;
    }

    void AnimationStateMachine::BlendRootMotion(Animator* animator, AnimationClip* fromClip, AnimationClip* toClip, float blend)
    {
        if (!animator->IsRootMotionEnabled())
            return;

        // Get the current root motion
        RootMotionData currentMotion = animator->GetRootMotion();

        // Scale by blend factor
        Vector3 blendedDelta = currentMotion.deltaPosition * (1.0f - blend);
        Quaternion blendedRot = Quaternion::Slerp({ 0.0f, 0.0f, 0.0f, 1.0f }, currentMotion.deltaRotation, 1.0f - blend);

        m_blendedRootMotionDelta = blendedDelta;
        m_blendedRootMotionRotation = blendedRot;

        // Modify the animator's root motion
        animator->m_rootMotion.deltaPosition = blendedDelta;
        animator->m_rootMotion.deltaRotation = blendedRot;
    }

    void AnimationStateMachine::EvaluateLayeredStates(float deltaTime, Animator* animator, std::vector<std::shared_ptr<Mesh>>& meshes)
    {
        if (!animator->GetSkeleton())
            return;

        AnimationState* currentState = GetCurrentState();
        if (!currentState)
            return;

        // Group all active states by layer
        std::map<int, AnimationState*> activeLayerStates;

        // Only current state is active
        activeLayerStates[currentState->layer] = currentState;

        // Update state time
        float prevTime = m_currentStateTime;
        m_currentStateTime += deltaTime * currentState->speed;

        // Handle looping
        if (currentState->clip)
        {
            float duration = currentState->clip->GetDuration();
            if (duration > 0.0f && m_currentStateTime > duration)
            {
                if (currentState->loop)
                {
                    while (m_currentStateTime > duration)
                        m_currentStateTime -= duration;
                }
                else
                    m_currentStateTime = duration;
            }
        }

        // Evaluate layers in order
        std::vector<Matrix4> baseTransforms;
        bool hasBase = false;

        for (auto& pair : activeLayerStates)
        {
            int layer = pair.first;
            AnimationState* state = pair.second;

            if (!state)
                continue;

            std::vector<Matrix4> layerTransforms;

            // Sample animation
            if (state->blendTree)
                animator->EvaluateBlendTree(state->blendTree, m_currentStateTime, layerTransforms);
            else if (state->clip)
                animator->SampleAnimationToBuffer(state->clip, m_currentStateTime, layerTransforms);

            if (layerTransforms.empty())
                continue;

            // First layer (base) or replace
            if (!hasBase || layer == 0)
            {
                baseTransforms = layerTransforms;
                hasBase = true;
            }
            else
            {
                // Blend this layer onto base
                float layerWeight = GetLayerWeight(layer);

                if (layerWeight > 0.0f)
                {
                    auto maskIt = m_layerMasks.find(layer);
                    int maskLayerId = layer;

                    if (maskIt != m_layerMasks.end() && layerWeight > 0.0f)
                    {
                        // Create a temporary mapping in animator for this blend operation
                        animator->SetBoneMask(maskIt->second, maskLayerId);

                        animator->BlendBoneTransforms(baseTransforms, layerTransforms, layerWeight, baseTransforms, maskLayerId);

                        // Clean up temporary mask
                        animator->ClearBoneMask(maskLayerId);
                    }
                    else
                        animator->BlendBoneTransforms(baseTransforms, layerTransforms, layerWeight, baseTransforms, -1);
                }
            }

            // Call onUpdate callback
            if (state->onUpdate)
                state->onUpdate(deltaTime);
        }

        // Apply final transforms
        if (hasBase)
        {
            animator->SetLocalTransforms(baseTransforms);
            animator->CalculateBoneTransforms();
        }

        // Process morph weights and events
        if (currentState && currentState->clip)
        {
            animator->SampleMorphWeights(m_currentStateTime, meshes);

            if (animator->m_eventCallback)
                animator->ProcessAnimationEvents(currentState->clip, prevTime, m_currentStateTime);

            // Handle root motion
            if (animator->IsRootMotionEnabled() && currentState->clip->IsRootMotionEnabled())
                animator->UpdateRootMotion(currentState->clip, deltaTime);
        }
    }


    void AnimationStateMachine::Update(float deltaTime, Animator* animator, std::vector<std::shared_ptr<Mesh>>& meshes)
    {
        if (!animator || m_currentStateId < 0)
            return;

        AnimationState* currentState = GetCurrentState();
        if (!currentState)
            return;

        // Group states by layer
        std::map<int, std::vector<AnimationState*>> layerStates;

        // Update transition
        if (m_isTransitioning)
        {
            m_transitionTime += deltaTime;
            float t = m_transitionTime / m_activeTransition.duration;

            if (t >= 1.0f)
            {
                // Call onExit for old state
                AnimationState* oldState = GetState(m_activeTransition.fromStateId);
                if (oldState && oldState->onExit)
                    oldState->onExit();

                // Transition complete
                m_currentStateId = m_transitionTargetStateId;
                m_currentStateTime = 0.0f;
                m_isTransitioning = false;
                currentState = GetCurrentState();

                // Call onEnter for new state
                if (currentState && currentState->onEnter)
                    currentState->onEnter();

                if (!currentState)
                    return;
            }
            else
            {
                // Blend between states during transition
                AnimationState* fromState = GetState(m_activeTransition.fromStateId);
                AnimationState* toState = GetState(m_activeTransition.toStateId);

                if (fromState && toState && animator->GetSkeleton())
                {
                    // Handle different layers
                    if (fromState->layer == toState->layer)
                    {
                        // Same layer
                        std::vector<Matrix4> fromTransforms;
                        std::vector<Matrix4> toTransforms;

                        if (fromState->clip)
                            animator->SampleAnimationToBuffer(fromState->clip, m_currentStateTime, fromTransforms);

                        if (toState->clip)
                        {
                            float toStateTime = m_transitionTime * toState->speed;
                            animator->SampleAnimationToBuffer(toState->clip, toStateTime, toTransforms);
                        }

                        if (!fromTransforms.empty() && !toTransforms.empty())
                        {
                            std::vector<Matrix4> blendedTransforms;

                            // Apply layer mask if exists
                            auto maskIt = m_layerMasks.find(fromState->layer);
                            int layerId = -1; // No mask by default
                            if (maskIt != m_layerMasks.end())
                            {
                                // Create temporary layer ID for masking
                                layerId = fromState->layer;
                                animator->SetBoneMask(maskIt->second, layerId);
                            }

                            animator->BlendBoneTransforms(fromTransforms, toTransforms, t, blendedTransforms, layerId);

                            // Apply layer weight
                            float layerWeight = GetLayerWeight(fromState->layer);
                            if (fromState->layer == 0 || layerWeight >= 0.9999f)
                                animator->m_localTransforms = blendedTransforms;
                            else
                            {
                                // Blend with base layer
                                std::vector<Matrix4> currentTransforms = animator->GetLocalTransforms();
                                animator->BlendBoneTransforms(currentTransforms, blendedTransforms, layerWeight, currentTransforms, layerId);
                                animator->SetLocalTransforms(currentTransforms);

                            }

                            animator->CalculateBoneTransforms();
                        }

                        // Process events from both states
                        if (fromState->clip && animator->m_eventCallback)
                        {
                            float prevTime = m_currentStateTime - deltaTime * fromState->speed;
                            animator->ProcessAnimationEvents(fromState->clip, prevTime, m_currentStateTime);
                        }
                    }

                    // Blend root motion during transition
                    if (fromState->clip && toState->clip)
                        BlendRootMotion(animator, fromState->clip, toState->clip, t);
                }

                m_currentStateTime += deltaTime * (currentState->speed);
                return;
            }
        }

        // Normal state update with layer support
        EvaluateLayeredStates(deltaTime, animator, meshes);

        // Check for transitions
        if (!m_isTransitioning)
        {
            float normalizedTime = 0.0f;
            if (currentState->clip && currentState->clip->GetDuration() > 0.0f)
                normalizedTime = m_currentStateTime / currentState->clip->GetDuration();

            for (const auto& transition : currentState->transitions)
            {
                if (transition.hasExitTime && normalizedTime < transition.exitTime)
                    continue;

                if (CheckTransitionConditions(transition))
                {
                    StartTransition(transition);
                    break;
                }
            }
        }
        else
        {
            // Check for interrupt
            if (m_activeTransition.canInterrupt && m_transitionTime >= m_activeTransition.interruptibleAfter)
            {
                AnimationState* fromState = GetState(m_activeTransition.fromStateId);
                if (fromState)
                {
                    for (const auto& transition : fromState->transitions)
                    {
                        if (transition.toStateId == m_activeTransition.toStateId)
                            continue;

                        if (CheckTransitionConditions(transition))
                        {
                            StartTransition(transition);
                            break;
                        }
                    }
                }
            }
        }
    }

    bool AnimationStateMachine::CheckTransitionConditions(const AnimationTransition& transition)
    {
        if (transition.useMultipleConditions)
        {
            // Check all conditions
            for (const auto& condition : transition.conditions)
            {
                bool conditionMet = false;

                switch (condition.type)
                {
                    case AnimationTransition::ConditionType::Greater:
                    {
                        float value = GetParameterFloat(condition.parameter);
                        conditionMet = value > condition.value;
                        break;
                    }
                    case AnimationTransition::ConditionType::Less:
                    {
                        float value = GetParameterFloat(condition.parameter);
                        conditionMet = value < condition.value;
                        break;
                    }
                    case AnimationTransition::ConditionType::Equal:
                    {
                        float value = GetParameterFloat(condition.parameter);
                        conditionMet = std::abs(value - condition.value) < 0.001f;
                        break;
                    }
                    case AnimationTransition::ConditionType::NotEqual:
                    {
                        float value = GetParameterFloat(condition.parameter);
                        conditionMet = std::abs(value - condition.value) >= 0.001f;
                        break;
                    }
                    case AnimationTransition::ConditionType::True:
                    {
                        conditionMet = GetParameterBool(condition.parameter);
                        break;
                    }
                    case AnimationTransition::ConditionType::False:
                    {
                        conditionMet = !GetParameterBool(condition.parameter);
                        break;
                    }
                }

                if (!conditionMet)
                    return false;
            }

            return true;
        }
        else
        {
            if (transition.conditionParameter.empty())
                return transition.conditionType == AnimationTransition::ConditionType::True;

            switch (transition.conditionType)
            {
                case AnimationTransition::ConditionType::Greater:
                {
                    float value = GetParameterFloat(transition.conditionParameter);
                    return value > transition.conditionValue;
                }
                case AnimationTransition::ConditionType::Less:
                {
                    float value = GetParameterFloat(transition.conditionParameter);
                    return value < transition.conditionValue;
                }
                case AnimationTransition::ConditionType::Equal:
                {
                    float value = GetParameterFloat(transition.conditionParameter);
                    return std::abs(value - transition.conditionValue) < 0.001f;
                }
                case AnimationTransition::ConditionType::NotEqual:
                {
                    float value = GetParameterFloat(transition.conditionParameter);
                    return std::abs(value - transition.conditionValue) >= 0.001f;
                }
                case AnimationTransition::ConditionType::True:
                {
                    return GetParameterBool(transition.conditionParameter);
                }
                case AnimationTransition::ConditionType::False:
                {
                    return !GetParameterBool(transition.conditionParameter);
                }
            }

            return false;
        }

        return false;
    }

    void AnimationStateMachine::StartTransition(const AnimationTransition& transition)
    {
        AnimationState* currentState = GetCurrentState();
        if (currentState && currentState->onExit)
            currentState->onExit();

        m_isTransitioning = true;
        m_activeTransition = transition;
        m_transitionTime = 0.0f;
        m_transitionTargetStateId = transition.toStateId;
    }

    float AnimationStateMachine::GetTransitionProgress() const
    {
        if (!m_isTransitioning || m_activeTransition.duration <= 0.0f)
            return 0.0f;

        return std::min(1.0f, m_transitionTime / m_activeTransition.duration);
    }

    Animator::Animator()
        : m_skeleton(nullptr)
        , m_currentClip(nullptr)
        , m_currentTime(0.0f)
        , m_speed(1.0f)
        , m_playing(false)
        , m_paused(false)
        , m_loop(true)
        , m_blendTreeRoot(nullptr)
        , m_blendParameter(0.0f)
        , m_blendParameterY(0.0f)
        , m_additiveRefClip(nullptr)
        , m_nextLayerId(0)
        , m_rootMotionEnabled(false)
        , m_rootMotionScale(1.0f, 1.0f, 1.0f)
        , m_previousRootPosition(0.0f, 0.0f, 0.0f)
        , m_previousRootRotation(0.0f, 0.0f, 0.0f, 1.0f)
        , m_ikEnabled(true)
        , m_stateMachine(nullptr)
    {
        CreateLayer("Base", 0);
    }

    Animator::~Animator()
    {
        delete m_blendTreeRoot;
    }

    void Animator::SetSkeleton(Skeleton* skeleton)
    {
        m_skeleton = skeleton;

        if (!m_skeleton)
            return;

        m_localTransforms.resize(m_skeleton->bones.size());
        m_boneMatrices.resize(m_skeleton->bones.size());

        for (auto& bone : m_skeleton->bones)
            bone.children.clear();

        for (size_t i = 0; i < m_skeleton->bones.size(); ++i)
        {
            m_localTransforms[i] = m_skeleton->bones[i].localTransform;

            int parent = m_skeleton->bones[i].parentIndex;
            if (parent >= 0)
                m_skeleton->bones[parent].children.push_back((int)i);
        }

        CalculateBoneTransforms();

        m_skeleton->finalMatrices = m_boneMatrices;
    }

    void Animator::PlayAnimation(AnimationClip* clip, bool loop)
    {
        if (!clip)
            return;

        if (m_layers.empty())
            CreateLayer("Base", 0);

        AnimationLayer& baseLayer = m_layers[0];

        baseLayer.clip = clip;
        baseLayer.currentTime = 0.0f;
        baseLayer.loop = loop;
        baseLayer.active = true;
        baseLayer.weight = 1.0f;
        baseLayer.timeScale = 1.0f;

        m_currentClip = clip;
        m_currentTime = 0.0f;
        m_playing = true;
        m_paused = false;
        m_loop = loop;

        if (m_crossfade.active && m_crossfade.targetLayer == 0)
        {
            m_crossfade.active = false;
            m_crossfade.fromClip = nullptr;
            m_crossfade.toClip = nullptr;
            m_crossfade.elapsed = 0.0f;
            m_crossfade.duration = 0.0f;
        }

        // Initialize root motion tracking
        if (m_rootMotionEnabled && clip->IsRootMotionEnabled() && m_skeleton && !m_skeleton->bones.empty())
        {
            int rootBoneIdx = clip->GetRootBoneIndex();
            if (rootBoneIdx >= 0 && rootBoneIdx < static_cast<int>(m_boneMatrices.size()))
            {
                m_previousRootPosition = m_boneMatrices[rootBoneIdx].GetTranslation();
                m_previousRootRotation = m_boneMatrices[rootBoneIdx].GetRotation();
            }
        }
    }

    void Animator::StopAnimation()
    {
        m_playing = false;
        m_paused = false;
        m_currentTime = 0.0f;
    }

    void Animator::PauseAnimation()
    {
        m_paused = true;
    }

    void Animator::ResumeAnimation()
    {
        m_paused = false;
    }

    void Animator::Update(float deltaTime, std::vector<std::shared_ptr<Mesh>>& meshes)
    {
        if (!m_playing || m_paused)
            return;

        float prevTime = m_currentTime;

        if (m_crossfade.active)
        {
            UpdateCrossfade(deltaTime);

            // Sample morph weights for crossfade
            if (m_crossfade.active && m_crossfade.toClip)
            {
                float toTime = m_crossfade.elapsed * m_speed * m_layers[m_crossfade.targetLayer].timeScale;
                SampleMorphWeights(toTime, meshes);
            }

            // Apply IK
            if (m_ikEnabled && m_skeleton)
                ApplyIK();

            // Returning here since we don't run any other animation updates
            return;
        }

        // Normal animation updates
        if (m_stateMachine)
            m_stateMachine->Update(deltaTime, this, meshes);
        else if (m_blendTreeRoot)
            UpdateBlendTree(deltaTime);
        else if (!m_layers.empty())
            UpdateLayers(deltaTime, meshes);
        else if (m_currentClip)
        {
            float duration = m_currentClip->GetDuration();
            if (duration <= 0.0f)
                return;

            m_currentTime += deltaTime * m_speed;
            if (m_currentTime > duration)
            {
                if (m_loop)
                {
                    while (m_currentTime > duration)
                        m_currentTime -= duration;
                }
                else
                {
                    m_currentTime = duration;
                    m_playing = false;
                }
            }

            if (m_currentClip->GetAnimationType() == AnimationType::Skeletal)
            {
                if (m_skeleton)
                {
                    SampleAnimation(m_currentTime);
                    CalculateBoneTransforms();
                }
            }
            else
                SampleNodeAnimation(m_currentTime);

            SampleMorphWeights(m_currentTime, meshes);
            ProcessAnimationEvents(m_currentClip, prevTime, m_currentTime);

            if (m_rootMotionEnabled && m_currentClip->IsRootMotionEnabled())
                UpdateRootMotion(m_currentClip, deltaTime);
        }

        if (m_ikEnabled && m_skeleton)
            ApplyIK();
    }

    void Animator::SetTime(float time)
    {
        if (!m_currentClip)
            return;

        m_currentTime = std::max(0.0f, std::min(time, m_currentClip->GetDuration()));

        if (m_skeleton)
        {
            SampleAnimation(m_currentTime);
            CalculateBoneTransforms();

            m_skeleton->finalMatrices = m_boneMatrices;
        }
    }

    void Animator::SampleAnimationToBoneTransforms(AnimationClip* clip, float time, std::vector<BoneTransform>& transforms)
    {
        if (!clip || !m_skeleton)
            return;

        transforms.resize(m_skeleton->bones.size());

        // Initialize all bones with rest pose
        for (size_t i = 0; i < transforms.size(); ++i)
        {
            transforms[i].translation = m_skeleton->bones[i].localTransform.GetTranslation();
            transforms[i].rotation = m_skeleton->bones[i].localTransform.GetRotation();
            transforms[i].scale = m_skeleton->bones[i].localTransform.GetScale();
            transforms[i].hasTranslation = false;
            transforms[i].hasRotation = false;
            transforms[i].hasScale = false;
        }

        // Apply animation data
        for (const AnimationChannel& channel : clip->GetChannels())
        {
            int boneIndex = channel.targetBoneIndex;
            if (boneIndex < 0 || boneIndex >= static_cast<int>(transforms.size()))
                continue;

            if (!channel.translations.empty())
            {
                transforms[boneIndex].translation = InterpolateTranslation(channel, time);
                transforms[boneIndex].hasTranslation = true;
            }

            if (!channel.rotations.empty())
            {
                transforms[boneIndex].rotation = InterpolateRotation(channel, time);
                transforms[boneIndex].hasRotation = true;
            }

            if (!channel.scales.empty())
            {
                transforms[boneIndex].scale = InterpolateScale(channel, time);
                transforms[boneIndex].hasScale = true;
            }
        }
    }

    void Animator::BlendBoneTransformsToMatrices(const std::vector<BoneTransform>& from,
        const std::vector<BoneTransform>& to,
        float weight,
        std::vector<Matrix4>& result)
    {
        result.resize(from.size());

        for (size_t i = 0; i < from.size(); ++i)
        {
            // Blend translation
            Vector3 blendedTranslation = from[i].translation + (to[i].translation - from[i].translation) * weight;

            // Blend scale
            Vector3 blendedScale = from[i].scale + (to[i].scale - from[i].scale) * weight;

            // Blend rotation
            Quaternion fromRot = from[i].rotation;
            Quaternion toRot = to[i].rotation;

            // Normalize both quaternions
            fromRot = fromRot.Normalize();
            toRot = toRot.Normalize();

            // Check dot product for shortest path
            float dot = fromRot.x * toRot.x + fromRot.y * toRot.y +
                fromRot.z * toRot.z + fromRot.w * toRot.w;

            if (dot < 0.0f)
            {
                toRot.x = -toRot.x;
                toRot.y = -toRot.y;
                toRot.z = -toRot.z;
                toRot.w = -toRot.w;
            }

            Quaternion blendedRotation = Quaternion::Slerp(fromRot, toRot, weight);
            blendedRotation = blendedRotation.Normalize();

            // Construct matrix
            result[i] = Matrix4::Translate(blendedTranslation) * Matrix4::FromQuaternion(blendedRotation) * Matrix4::Scale(blendedScale);
        }
    }

    int Animator::GetLayerIndex(int layerId) const
    {
        auto it = m_layerIdToIndex.find(layerId);
        return (it != m_layerIdToIndex.end()) ? static_cast<int>(it->second) : -1;
    }

    int Animator::CreateLayer(const std::string& name, int priority)
    {
        AnimationLayer layer;
        layer.id = m_nextLayerId++;
        layer.priority = priority;

        m_layers.push_back(layer);

        // Sort by priority
        std::sort(m_layers.begin(), m_layers.end(), [](const AnimationLayer& a, const AnimationLayer& b)
            { return a.priority < b.priority; });

        // Rebuild mappings
        m_layerIdToIndex.clear();
        for (size_t i = 0; i < m_layers.size(); ++i)
            m_layerIdToIndex[m_layers[i].id] = i;

        m_layerNames[name] = layer.id;

        return layer.id;
    }

    void Animator::RemoveLayer(int layerId)
    {
        auto it = m_layerIdToIndex.find(layerId);
        if (it == m_layerIdToIndex.end())
            return;

        size_t index = it->second;
        m_layers.erase(m_layers.begin() + index);
        m_boneMasks.erase(layerId);

        // Rebuild mappings
        m_layerIdToIndex.clear();
        for (size_t i = 0; i < m_layers.size(); ++i)
            m_layerIdToIndex[m_layers[i].id] = i;
    }

    void Animator::SetLayerWeight(int layerId, float weight)
    {
        int index = GetLayerIndex(layerId);
        if (index < 0)
            return;

        m_layers[index].weight = std::max(0.0f, std::min(1.0f, weight));
    }

    float Animator::GetLayerWeight(int layerId) const
    {
        int index = GetLayerIndex(layerId);
        if (index < 0)
            return 0.0f;

        return m_layers[index].weight;
    }

    void Animator::SetLayerBlendMode(int layerId, AnimationBlendMode mode)
    {
        int index = GetLayerIndex(layerId);
        if (index < 0)
            return;

        m_layers[index].blendMode = mode;
    }

    bool Animator::PlayAnimationOnLayer(int layerId, AnimationClip* clip, bool loop)
    {
        if (!clip)
            return false;

        int index = GetLayerIndex(layerId);
        if (index < 0)
            return false;

        AnimationLayer& layer = m_layers[index];
        layer.clip = clip;
        layer.currentTime = 0.0f;
        layer.loop = loop;
        layer.active = true;
        layer.weight = 1.0f;
        layer.timeScale = 1.0f;
        m_playing = true;

        return true;
    }

    void Animator::StopLayer(int layerId)
    {
        int index = GetLayerIndex(layerId);
        if (index < 0)
            return;

        m_layers[index].active = false;
    }

    void Animator::CrossfadeToAnimation(AnimationClip* clip, float duration, bool loop, int layerIndex)
    {
        if (!clip)
            return;

        if (m_layers.empty())
            CreateLayer("Base", 0);

        if (layerIndex < 0 || layerIndex >= static_cast<int>(m_layers.size()))
            return;

        AnimationLayer& layer = m_layers[layerIndex];

        // Store the current clip and time
        m_crossfade.fromClip = layer.clip;
        m_crossfade.toClip = clip;
        m_crossfade.duration = duration;
        m_crossfade.elapsed = 0.0f;
        m_crossfade.active = true;
        m_crossfade.targetLayer = layerIndex;

        layer.loop = loop;

        // If there's no fromClip, we need to initialize from somewhere
        if (m_crossfade.fromClip == nullptr)
        {
            layer.currentTime = 0.0f;
            layer.clip = clip;

            if (m_skeleton)
            {
                SampleAnimationToBuffer(clip, 0.0f, m_localTransforms);
                CalculateBoneTransforms();

                // Initialize root motion tracking for the new clip
                if (m_rootMotionEnabled && clip->IsRootMotionEnabled() && !m_skeleton->bones.empty())
                {
                    int rootBoneIdx = clip->GetRootBoneIndex();
                    if (rootBoneIdx >= 0 && rootBoneIdx < static_cast<int>(m_boneMatrices.size()))
                    {
                        m_previousRootPosition = m_boneMatrices[rootBoneIdx].GetTranslation();
                        m_previousRootRotation = m_boneMatrices[rootBoneIdx].GetRotation();
                    }
                }
            }
        }
        else
        {
            if (m_rootMotionEnabled && m_skeleton && !m_skeleton->bones.empty())
            {
                // If fromClip has root motion, keep tracking
                if (m_crossfade.fromClip->IsRootMotionEnabled())
                {
                    int rootBoneIdx = m_crossfade.fromClip->GetRootBoneIndex();
                    if (rootBoneIdx >= 0 && rootBoneIdx < static_cast<int>(m_boneMatrices.size()))
                    {
                        // Update to current position
                        m_previousRootPosition = m_boneMatrices[rootBoneIdx].GetTranslation();
                        m_previousRootRotation = m_boneMatrices[rootBoneIdx].GetRotation();
                    }
                }
                // If toClip has root motion but fromClip doesn't, initialize from current pose
                else if (clip->IsRootMotionEnabled())
                {
                    int rootBoneIdx = clip->GetRootBoneIndex();
                    if (rootBoneIdx >= 0 && rootBoneIdx < static_cast<int>(m_boneMatrices.size()))
                    {
                        m_previousRootPosition = m_boneMatrices[rootBoneIdx].GetTranslation();
                        m_previousRootRotation = m_boneMatrices[rootBoneIdx].GetRotation();
                    }
                }
            }
        }
    }


    float Animator::GetCrossfadeProgress() const
    {
        if (!m_crossfade.active || m_crossfade.duration <= 0.0f)
            return 0.0f;

        return m_crossfade.elapsed / m_crossfade.duration;
    }

    void Animator::SetBoneMask(const std::vector<int>& boneIndices, int layerIndex)
    {
        m_boneMasks[layerIndex] = boneIndices;
    }

    void Animator::ClearBoneMask(int layerIndex)
    {
        m_boneMasks.erase(layerIndex);
    }

    const std::vector<int>& Animator::GetBoneMask(int layerIndex) const
    {
        static std::vector<int> empty;
        auto it = m_boneMasks.find(layerIndex);
        return (it != m_boneMasks.end()) ? it->second : empty;
    }

    void Animator::SyncLayerToLayer(int sourceLayerId, int targetLayerId)
    {
        int srcIdx = GetLayerIndex(sourceLayerId);
        int tgtIdx = GetLayerIndex(targetLayerId);

        if (srcIdx < 0 || tgtIdx < 0)
            return;

        m_layers[tgtIdx].currentTime = m_layers[srcIdx].currentTime;
    }

    void Animator::SetLayerTimeScale(int layerIndex, float scale)
    {
        if (layerIndex < 0 || layerIndex >= static_cast<int>(m_layers.size()))
            return;

        m_layers[layerIndex].timeScale = scale;
    }

    void Animator::UpdateLayers(float deltaTime, std::vector<std::shared_ptr<Mesh>>& meshes)
    {
        if (!m_skeleton || m_layers.empty())
            return;

        std::vector<Matrix4> finalTransforms(m_skeleton->bones.size());
        for (size_t i = 0; i < finalTransforms.size(); ++i)
            finalTransforms[i] = m_skeleton->bones[i].localTransform;

        bool firstLayer = true;

        for (size_t layerIdx = 0; layerIdx < m_layers.size(); ++layerIdx)
        {
            auto& layer = m_layers[layerIdx];

            if (!layer.active || !layer.clip || layer.weight <= 0.0f)
                continue;

            if (m_crossfade.active && m_crossfade.targetLayer == static_cast<int>(layerIdx))
                continue;

            float duration = layer.clip->GetDuration();
            if (duration <= 0.0f)
                continue;

            layer.currentTime += deltaTime * m_speed * layer.timeScale;
            if (layer.currentTime > duration)
            {
                if (layer.loop)
                {
                    while (layer.currentTime > duration)
                        layer.currentTime -= duration;
                }
                else
                {
                    layer.currentTime = duration;
                    layer.active = false;
                }
            }

            std::vector<Matrix4> layerTransforms(m_skeleton->bones.size());
            SampleAnimationToBuffer(layer.clip, layer.currentTime, layerTransforms);

            if (firstLayer && layer.blendMode == AnimationBlendMode::Override)
            {
                finalTransforms = layerTransforms;
                firstLayer = false;
            }
            else
            {
                switch (layer.blendMode)
                {
                    case AnimationBlendMode::Override:
                    case AnimationBlendMode::Blend:
                        BlendBoneTransforms(finalTransforms, layerTransforms, layer.weight, finalTransforms, layer.id);
                        break;
                    case AnimationBlendMode::Additive:
                        ApplyAdditiveAnimation(layerTransforms, finalTransforms);
                        break;
                }
            }
        }

        m_localTransforms = finalTransforms;
        CalculateBoneTransforms();
        SampleMorphWeights(m_layers[0].currentTime, meshes);
    }

    void Animator::UpdateCrossfade(float deltaTime)
    {
        if (!m_crossfade.active)
            return;

        if (!m_skeleton || !m_crossfade.toClip)
        {
            m_crossfade.active = false;
            return;
        }

        if (!m_crossfade.fromClip)
        {
            PlayAnimationOnLayer(m_crossfade.targetLayer, m_crossfade.toClip, m_layers[m_crossfade.targetLayer].loop);
            m_crossfade.active = false;
            return;
        }

        m_crossfade.elapsed += deltaTime;
        float t = std::min(1.0f, m_crossfade.elapsed / m_crossfade.duration);

        if (t >= 1.0f)
        {
            // Crossfade complete
            AnimationLayer& layer = m_layers[m_crossfade.targetLayer];
            layer.clip = m_crossfade.toClip;
            layer.currentTime = m_crossfade.elapsed * m_speed * layer.timeScale;
            layer.active = true;

            m_crossfade.active = false;
            m_crossfade.fromClip = nullptr;
            m_crossfade.toClip = nullptr;
            return;
        }

        AnimationLayer& layer = m_layers[m_crossfade.targetLayer];

        float fromDuration = m_crossfade.fromClip->GetDuration();
        float toDuration = m_crossfade.toClip->GetDuration();

        // Calculate time for from animation
        float fromTime = layer.currentTime;
        fromTime += deltaTime * m_speed * layer.timeScale;
        if (fromDuration > 0.0f && fromTime > fromDuration)
        {
            if (layer.loop)
            {
                while (fromTime > fromDuration)
                    fromTime -= fromDuration;
            }
            else
                fromTime = fromDuration;
        }

        // Calculate time for to animation
        float toTime = m_crossfade.elapsed * m_speed * layer.timeScale;
        if (toDuration > 0.0f && toTime > toDuration)
        {
            if (layer.loop)
            {
                while (toTime > toDuration)
                    toTime -= toDuration;
            }
            else
                toTime = toDuration;
        }

        // Sample both animations to TRS format
        std::vector<BoneTransform> fromTransforms;
        std::vector<BoneTransform> toTransforms;

        SampleAnimationToBoneTransforms(m_crossfade.fromClip, fromTime, fromTransforms);
        SampleAnimationToBoneTransforms(m_crossfade.toClip, toTime, toTransforms);

        // Blend and convert to matrices
        BlendBoneTransformsToMatrices(fromTransforms, toTransforms, t, m_localTransforms);

        // Calculate final bone matrices
        CalculateBoneTransforms();

        // Extract root motion during crossfade
        if (m_rootMotionEnabled)
        {
            // Blend root motion from both clips
            if (m_crossfade.fromClip->IsRootMotionEnabled() || m_crossfade.toClip->IsRootMotionEnabled())
            {
                // Update root motion will calculate the delta
                UpdateRootMotion(m_crossfade.toClip, deltaTime);

                // Scale the root motion by the blend factor
                m_rootMotion.deltaPosition = m_rootMotion.deltaPosition * t;
                m_rootMotion.deltaRotation = Quaternion::Slerp({ 0.0f, 0.0f, 0.0f, 1.0f }, m_rootMotion.deltaRotation, t);
            }
        }

        // Update layer time
        layer.currentTime = fromTime;
        layer.clip = m_crossfade.fromClip;
    }

    void Animator::UpdateBlendTree(float deltaTime)
    {
        if (!m_blendTreeRoot || !m_skeleton)
            return;

        EvaluateBlendTree(m_blendTreeRoot, deltaTime, m_localTransforms);
        CalculateBoneTransforms();
    }

    void Animator::BlendBoneTransforms(const std::vector<Matrix4>& from, const std::vector<Matrix4>& to, float weight, std::vector<Matrix4>& result, int layerId)
    {
        if (result.size() != from.size())
            result.resize(from.size());

        // Get bone mask for this layer
        const auto& mask = GetBoneMask(layerId);
        bool hasMask = !mask.empty();

        for (size_t i = 0; i < from.size(); ++i)
        {
            // Check if a mask is being used
            if (hasMask)
            {
                bool isMasked = std::find(mask.begin(), mask.end(), static_cast<int>(i)) != mask.end();
                if (!isMasked)
                {
                    result[i] = from[i];
                    continue;
                }
            }

            // Decompose both matrices
            Vector3 transFrom = from[i].GetTranslation();
            Vector3 transTo = to[i].GetTranslation();
            Quaternion rotFrom = from[i].GetRotation();
            Quaternion rotTo = to[i].GetRotation();
            Vector3 scaleFrom = from[i].GetScale();
            Vector3 scaleTo = to[i].GetScale();

            // Ensure shortest path for quaternion interpolation
            float dot = rotFrom.x * rotTo.x + rotFrom.y * rotTo.y + rotFrom.z * rotTo.z + rotFrom.w * rotTo.w;
            if (dot < 0.0f)
            {
                rotTo.x = -rotTo.x;
                rotTo.y = -rotTo.y;
                rotTo.z = -rotTo.z;
                rotTo.w = -rotTo.w;
            }

            // Blend components
            Vector3 trans = transFrom + (transTo - transFrom) * weight;
            Quaternion rot = Quaternion::Slerp(rotFrom, rotTo, weight);
            Vector3 scale = scaleFrom + (scaleTo - scaleFrom) * weight;

            // Reconstruct matrix
            result[i] = Matrix4::Translate(trans) * Matrix4::FromQuaternion(rot) * Matrix4::Scale(scale);
        }
    }
    void Animator::ApplyAdditiveAnimation(const std::vector<Matrix4>& additive, std::vector<Matrix4>& result)
    {
        // If we don't have a reference pose, sample it once
        if (m_additiveBaseTransforms.empty() && m_additiveRefClip)
            SampleAnimationToBuffer(m_additiveRefClip, 0.0f, m_additiveBaseTransforms);

        for (size_t i = 0; i < additive.size() && i < result.size(); ++i)
        {
            Matrix4 baseRef = (i < m_additiveBaseTransforms.size()) ? m_additiveBaseTransforms[i] : Matrix4::Identity();

            // Extract components for proper additive calculation
            Vector3 baseRefT = baseRef.GetTranslation();
            Quaternion baseRefR = baseRef.GetRotation();
            Vector3 baseRefS = baseRef.GetScale();

            Vector3 additiveT = additive[i].GetTranslation();
            Quaternion additiveR = additive[i].GetRotation();
            Vector3 additiveS = additive[i].GetScale();

            Vector3 resultT = result[i].GetTranslation();
            Quaternion resultR = result[i].GetRotation();
            Vector3 resultS = result[i].GetScale();

            // Compute deltas
            Vector3 deltaT = additiveT - baseRefT;
            Quaternion deltaR = additiveR * baseRefR.Inverse();
            Vector3 deltaS = { additiveS.x / (baseRefS.x != 0.0f ? baseRefS.x : 1.0f), additiveS.y / (baseRefS.y != 0.0f ? baseRefS.y : 1.0f), additiveS.z / (baseRefS.z != 0.0f ? baseRefS.z : 1.0f) };

            // Apply deltas
            Vector3 finalT = resultT + deltaT;
            Quaternion finalR = resultR * deltaR;
            Vector3 finalS = { resultS.x * deltaS.x, resultS.y * deltaS.y, resultS.z * deltaS.z };

            result[i] = Matrix4::Translate(finalT) * Matrix4::FromQuaternion(finalR) * Matrix4::Scale(finalS);
        }
    }

    void Animator::SampleAnimationToBuffer(AnimationClip* clip, float time, std::vector<Matrix4>& buffer)
    {
        if (!clip || !m_skeleton)
            return;

        buffer.resize(m_skeleton->bones.size());
        for (size_t i = 0; i < buffer.size(); ++i)
            buffer[i] = m_skeleton->bones[i].localTransform;

        struct AnimData
        {
            bool hasT = false, hasR = false, hasS = false;
            Vector3 t;
            Quaternion r;
            Vector3 s;
        };

        std::vector<AnimData> animData(m_skeleton->bones.size());

        for (const AnimationChannel& channel : clip->GetChannels())
        {
            int boneIndex = channel.targetBoneIndex;
            if (boneIndex < 0 || boneIndex >= static_cast<int>(m_skeleton->bones.size()))
                continue;

            if (!channel.translations.empty())
            {
                animData[boneIndex].t = InterpolateTranslation(channel, time);
                animData[boneIndex].hasT = true;
            }
            if (!channel.rotations.empty())
            {
                animData[boneIndex].r = InterpolateRotation(channel, time);
                animData[boneIndex].hasR = true;
            }
            if (!channel.scales.empty())
            {
                animData[boneIndex].s = InterpolateScale(channel, time);
                animData[boneIndex].hasS = true;
            }
        }

        for (size_t i = 0; i < m_skeleton->bones.size(); ++i)
        {
            if (animData[i].hasT || animData[i].hasR || animData[i].hasS)
            {
                Vector3 t = animData[i].hasT ? animData[i].t : m_skeleton->bones[i].localTransform.GetTranslation();
                Quaternion r = animData[i].hasR ? animData[i].r : m_skeleton->bones[i].localTransform.GetRotation();
                Vector3 s = animData[i].hasS ? animData[i].s : m_skeleton->bones[i].localTransform.GetScale();
                buffer[i] = Matrix4::Translate(t) * Matrix4::FromQuaternion(r) * Matrix4::Scale(s);
            }
        }
    }

    void Animator::EvaluateBlendTree(BlendTreeNode* node, float time, std::vector<Matrix4>& result)
    {
        if (!node)
            return;

        switch (node->type)
        {
            case BlendTreeNode::Type::Clip:
                if (node->clip)
                    SampleAnimationToBuffer(node->clip, time, result);
                break;

            case BlendTreeNode::Type::Blend1D:
            {
                if (node->children.size() < 2)
                    return;

                float param = m_blendParameter;

                // Find the appropriate interval
                int idx = 0;
                for (size_t i = 0; i < node->thresholds.size(); ++i)
                {
                    if (param >= node->thresholds[i])
                        idx = static_cast<int>(i);
                }

                int nextIdx = std::min(idx + 1, static_cast<int>(node->children.size()) - 1);

                // Handle edge cases
                if (idx >= static_cast<int>(node->children.size()) - 1)
                {
                    EvaluateBlendTree(node->children.back(), time, result);
                    return;
                }

                // Clamp indices to valid thresholds
                idx = std::min(idx, static_cast<int>(node->thresholds.size()) - 1);
                nextIdx = std::min(nextIdx, static_cast<int>(node->thresholds.size()) - 1);

                float t0 = node->thresholds[idx];
                float t1 = node->thresholds[nextIdx];
                float blend = (t1 - t0 > 0.0001f) ? (param - t0) / (t1 - t0) : 0.0f;
                blend = std::max(0.0f, std::min(1.0f, blend));

                std::vector<Matrix4> temp1, temp2;
                EvaluateBlendTree(node->children[idx], time, temp1);
                EvaluateBlendTree(node->children[nextIdx], time, temp2);

                // Verify animations have compatible bone counts
                if (temp1.empty() || temp2.empty())
                {
                    if (!temp1.empty())
                        result = temp1;
                    else if (!temp2.empty())
                        result = temp2;
                    return;
                }

                if (temp1.size() != temp2.size())
                {
                    std::cerr << "[WARNING] Blend1D: Animations have mismatched bone counts (" << temp1.size() << " vs " << temp2.size() << ")" << std::endl;
                    result = temp1; // Use first animation as fallback
                    return;
                }

                BlendBoneTransforms(temp1, temp2, blend, result);
                break;
            }

            case BlendTreeNode::Type::Blend2D:
            {
                if (node->children.size() < 3)
                {
                    if (node->children.size() == 2)
                    {
                        std::vector<Matrix4> temp1, temp2;
                        EvaluateBlendTree(node->children[0], time, temp1);
                        EvaluateBlendTree(node->children[1], time, temp2);

                        // Verify animations have compatible bone counts
                        if (temp1.empty() || temp2.empty())
                        {
                            if (!temp1.empty())
                                result = temp1;
                            else if (!temp2.empty())
                                result = temp2;
                            return;
                        }

                        if (temp1.size() != temp2.size())
                        {
                            std::cerr << "[WARNING] Blend2D: Animations have mismatched bone counts" << std::endl;
                            result = temp1;
                            return;
                        }

                        float blend = (m_blendParameter + 1.0f) * 0.5f;
                        blend = std::max(0.0f, std::min(1.0f, blend));
                        BlendBoneTransforms(temp1, temp2, blend, result);
                    }
                    return;
                }

                // This uses inverse distance weighting for blend space sampling. We could use Delaunay triangulation with barycentric interpolation as it would be more mathematically
                // correct but it requires preprocessing and is more complex to implement. The current approach provides good results with better performance and flexibility.

                Vector2 point(m_blendParameter, m_blendParameterY);

                // Calculate inverse distance weights
                std::vector<std::pair<float, size_t>> distances;
                for (size_t i = 0; i < node->positions.size() && i < node->children.size(); ++i)
                {
                    Vector2 pos = node->positions[i];
                    float dist = (pos.x - point.x) * (pos.x - point.x) +
                        (pos.y - point.y) * (pos.y - point.y);
                    distances.push_back({ dist, i });
                }

                std::sort(distances.begin(), distances.end());

                // Use 3 closest points
                size_t numBlend = std::min(size_t(3), distances.size());

                // Calculate normalized weights
                std::vector<float> weights;
                float totalWeight = 0.0f;
                for (size_t i = 0; i < numBlend; ++i)
                {
                    // Inverse distance weighting
                    float w = 1.0f / (std::sqrt(distances[i].first) + 0.001f);
                    weights.push_back(w);
                    totalWeight += w;
                }

                // Normalize weights
                for (size_t i = 0; i < numBlend; ++i)
                    weights[i] /= totalWeight;

                // Sample all animations
                std::vector<std::vector<Matrix4>> sampledAnims(numBlend);
                for (size_t i = 0; i < numBlend; ++i)
                    EvaluateBlendTree(node->children[distances[i].second], time, sampledAnims[i]);

                // Verify all animations have data and compatible bone counts
                if (sampledAnims.empty() || sampledAnims[0].empty())
                    return;

                size_t expectedBoneCount = sampledAnims[0].size();
                bool allSizesMatch = true;

                for (size_t i = 1; i < sampledAnims.size(); ++i)
                {
                    if (sampledAnims[i].empty() || sampledAnims[i].size() != expectedBoneCount)
                    {
                        std::cerr << "[WARNING] Blend2D: Animation " << i << " has mismatched bone count (expected " << expectedBoneCount << ", got " << sampledAnims[i].size() << ")" << std::endl;
                        allSizesMatch = false;
                        break;
                    }
                }

                if (!allSizesMatch)
                {
                    // Fallback to first valid animation
                    result = sampledAnims[0];
                    return;
                }

                // Blend all animations with their weights
                result.resize(expectedBoneCount);

                // Blend each bone across all animations
                for (size_t boneIdx = 0; boneIdx < expectedBoneCount; ++boneIdx)
                {
                    // Decompose all matrices for this bone
                    std::vector<Vector3> translations;
                    std::vector<Quaternion> rotations;
                    std::vector<Vector3> scales;

                    for (size_t i = 0; i < numBlend; ++i)
                    {
                        translations.push_back(sampledAnims[i][boneIdx].GetTranslation());
                        rotations.push_back(sampledAnims[i][boneIdx].GetRotation());
                        scales.push_back(sampledAnims[i][boneIdx].GetScale());
                    }

                    // Weighted blend of translation and scale
                    Vector3 blendedTrans(0.0f, 0.0f, 0.0f);
                    Vector3 blendedScale(0.0f, 0.0f, 0.0f);

                    for (size_t i = 0; i < numBlend; ++i)
                    {
                        blendedTrans = blendedTrans + translations[i] * weights[i];
                        blendedScale = blendedScale + scales[i] * weights[i];
                    }

                    // For quaternions, use incremental slerp with weights
                    Quaternion blendedRot = rotations[0];
                    float accumulatedWeight = weights[0];

                    for (size_t i = 1; i < numBlend; ++i)
                    {
                        // Ensure shortest path
                        Quaternion rot = rotations[i];
                        float dot = blendedRot.x * rot.x + blendedRot.y * rot.y + blendedRot.z * rot.z + blendedRot.w * rot.w;

                        if (dot < 0.0f)
                        {
                            rot.x = -rot.x;
                            rot.y = -rot.y;
                            rot.z = -rot.z;
                            rot.w = -rot.w;
                        }

                        // Blend with accumulated rotation
                        float t = weights[i] / (accumulatedWeight + weights[i]);
                        blendedRot = Quaternion::Slerp(blendedRot, rot, t);
                        accumulatedWeight += weights[i];
                    }

                    blendedRot = blendedRot.Normalize();

                    // Reconstruct matrix
                    result[boneIdx] = Matrix4::Translate(blendedTrans) *
                        Matrix4::FromQuaternion(blendedRot) *
                        Matrix4::Scale(blendedScale);
                }
                break;
            }

            case BlendTreeNode::Type::Additive:
                if (!node->children.empty())
                {
                    EvaluateBlendTree(node->children[0], time, result);

                    if (node->children.size() > 1)
                    {
                        std::vector<Matrix4> additiveTransforms;
                        EvaluateBlendTree(node->children[1], time, additiveTransforms);

                        // Verify compatible bone counts
                        if (!additiveTransforms.empty() && !result.empty())
                        {
                            if (additiveTransforms.size() != result.size())
                            {
                                std::cerr << "[WARNING] Additive: Animations have mismatched bone counts ("
                                    << result.size() << " vs " << additiveTransforms.size() << ")" << std::endl;
                                return; // Skip additive blend
                            }

                            ApplyAdditiveAnimation(additiveTransforms, result);
                        }
                    }
                }
                break;
        }
    }

    Matrix4 Animator::BlendMatrices(const Matrix4& a, const Matrix4& b, float t)
    {
        Vector3 transA = a.GetTranslation();
        Vector3 transB = b.GetTranslation();
        Quaternion rotA = a.GetRotation();
        Quaternion rotB = b.GetRotation();
        Vector3 scaleA = a.GetScale();
        Vector3 scaleB = b.GetScale();

        Vector3 trans = transA + (transB - transA) * t;
        Quaternion rot = Quaternion::Slerp(rotA, rotB, t);
        Vector3 scale = scaleA + (scaleB - scaleA) * t;

        return Matrix4::Translate(trans) * Matrix4::FromQuaternion(rot) * Matrix4::Scale(scale);
    }

    void Animator::UpdateRootMotion(AnimationClip* clip, float deltaTime)
    {
        if (!clip || !m_skeleton || m_skeleton->bones.empty())
            return;

        int rootBoneIdx = clip->GetRootBoneIndex();
        if (rootBoneIdx < 0 || rootBoneIdx >= static_cast<int>(m_skeleton->bones.size()))
            rootBoneIdx = 0;

        // Use global/world space transforms
        if (rootBoneIdx >= static_cast<int>(m_boneMatrices.size()))
            return;

        Vector3 currentPosition = m_boneMatrices[rootBoneIdx].GetTranslation();
        Quaternion currentRotation = m_boneMatrices[rootBoneIdx].GetRotation();

        // Calculate delta in world space
        m_rootMotion.deltaPosition = (currentPosition - m_previousRootPosition);
        m_rootMotion.deltaPosition.x *= m_rootMotionScale.x;
        m_rootMotion.deltaPosition.y *= m_rootMotionScale.y;
        m_rootMotion.deltaPosition.z *= m_rootMotionScale.z;

        m_rootMotion.deltaRotation = currentRotation * m_previousRootRotation.Inverse();

        // Store for next frame
        m_previousRootPosition = currentPosition;
        m_previousRootRotation = currentRotation;

        // Extract root motion from animation
        if (m_rootMotion.extractPosition || m_rootMotion.extractRotation)
        {
            Vector3 rootLocalPos = m_localTransforms[rootBoneIdx].GetTranslation();
            Quaternion rootLocalRot = m_localTransforms[rootBoneIdx].GetRotation();
            Vector3 rootLocalScale = m_localTransforms[rootBoneIdx].GetScale();

            // Zero out XZ movement, keep Y for vertical motion
            if (m_rootMotion.extractPosition)
            {
                rootLocalPos.x = 0.0f;
                rootLocalPos.z = 0.0f;
            }

            // Remove Y-axis rotation using quaternion projection
            if (m_rootMotion.extractRotation)
            {
                // Project quaternion onto XZ plane 
                // Extract Y rotation component
                Vector3 forward = rootLocalRot * Vector3(0.0f, 0.0f, 1.0f);
                forward.y = 0.0f;
                float forwardLen = forward.Length();

                if (forwardLen > 0.0001f)
                {
                    forward = forward / forwardLen;
                    // Create rotation with only Y component removed
                    Vector3 originalForward(0.0f, 0.0f, 1.0f);
                    rootLocalRot = Quaternion::FromToRotation(originalForward, forward);
                }
                else
                    rootLocalRot = { 0.0f, 0.0f, 0.0f, 1.0f };
            }

            m_localTransforms[rootBoneIdx] = Matrix4::Translate(rootLocalPos) * Matrix4::FromQuaternion(rootLocalRot) * Matrix4::Scale(rootLocalScale);

            // Recalculate bone matrices after modifying root
            CalculateBoneTransforms();
        }
    }

    void Animator::ClearRootMotion()
    {
        m_rootMotion.deltaPosition = { 0.0f, 0.0f, 0.0f };
        m_rootMotion.deltaRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    }

    void Animator::ProcessAnimationEvents(AnimationClip* clip, float prevTime, float currentTime)
    {
        if (!clip || !m_eventCallback)
            return;

        const auto& events = clip->GetEvents();
        if (events.empty())
            return;

        float duration = clip->GetDuration();
        bool looped = currentTime < prevTime;

        for (const auto& event : events)
        {
            bool shouldFire = false;

            if (looped)
            {
                // Handle loop wrap-around
                if (event.time >= prevTime || event.time <= currentTime)
                    shouldFire = true;
            }
            else
            {
                if (event.time >= prevTime && event.time <= currentTime)
                    shouldFire = true;
            }

            if (shouldFire)
            {
                // Check if we already fired this specific event this frame
                FiredEvent firedEvent = { event.time, event.eventName };

                bool alreadyFired = false;
                for (const FiredEvent& fired : m_firedEvents)
                {
                    if (fired == firedEvent)
                    {
                        alreadyFired = true;
                        break;
                    }
                }

                if (!alreadyFired)
                {
                    m_eventCallback(event);
                    m_firedEvents.push_back(firedEvent);
                }
            }
        }

        // Clear fired events list when animation loops or list gets too large
        if (looped || m_firedEvents.size() > 100)
            m_firedEvents.clear();
    }

    Quaternion Animator::ApplyJointConstraint(const Quaternion& rotation, const JointConstraint& constraint)
    {
        if (constraint.type == JointConstraint::Type::None)
            return rotation;

        switch (constraint.type)
        {
            case JointConstraint::Type::Hinge:
            {
                // Project rotation onto hinge axis
                Vector3 axis = constraint.axis.Normalize();
                Vector3 rotatedAxis = rotation * axis;

                // Calculate swing rotation
                Quaternion swing = Quaternion::FromToRotation(axis, rotatedAxis);

                // Extract twist rotation
                Quaternion twist = swing.Inverse() * rotation;

                // Get twist angle
                float twistAngle = 2.0f * std::atan2(twist.x * axis.x + twist.y * axis.y + twist.z * axis.z, twist.w);

                // Clamp angle
                twistAngle = std::max(constraint.minAngle * 3.14159f / 180.0f, std::min(constraint.maxAngle * 3.14159f / 180.0f, twistAngle));

                // Reconstruct constrained rotation
                Quaternion constrainedTwist = Quaternion::FromAxisAngle(axis, twistAngle);
                return swing * constrainedTwist;
            }

            case JointConstraint::Type::Cone:
            {
                // Cone constraint
                Vector3 forward = rotation * Vector3(0.0f, 0.0f, 1.0f);
                Vector3 constraintForward = constraint.twistAxis;

                float angle = std::acos(std::max(-1.0f, std::min(1.0f, Vector3::Dot(forward, constraintForward))));
                float maxAngle = constraint.coneAngle * 3.14159f / 180.0f;

                if (angle > maxAngle)
                {
                    // Clamp to cone edge
                    Vector3 axis = Vector3::Cross(constraintForward, forward).Normalize();
                    if (axis.Length() < 0.0001f)
                        axis = {1.0f, 0.0f, 0.0f};

                    Quaternion coneRot = Quaternion::FromAxisAngle(axis, maxAngle);
                    return coneRot;
                }

                return rotation;
            }

            case JointConstraint::Type::BallAndSocket:
            {
                Vector3 euler = rotation.ToEuler();
                euler.x = std::max(constraint.minAngle * 3.14159f / 180.0f, std::min(constraint.maxAngle * 3.14159f / 180.0f, euler.x));
                euler.y = std::max(constraint.minAngle * 3.14159f / 180.0f, std::min(constraint.maxAngle * 3.14159f / 180.0f, euler.y));
                euler.z = std::max(constraint.minAngle * 3.14159f / 180.0f, std::min(constraint.maxAngle * 3.14159f / 180.0f, euler.z));

                return Quaternion::FromEuler(euler.y, euler.x, euler.z);
            }

            default:
                return rotation;
        }
    }

    void Animator::SortIKChainsByDependency()
    {
        // Build dependency graph. A chain depends on another if they share bones, with dependency going root to tip

        m_ikChainOrder.clear();
        std::vector<bool> processed(m_ikChains.size(), false);

        // Simple topological sort
        for (size_t i = 0; i < m_ikChains.size(); ++i)
        {
            if (processed[i])
                continue;

            // Find chains with no unprocessed dependencies
            bool hasDependency = false;
            for (size_t j = 0; j < m_ikChains.size(); ++j)
            {
                if (i == j || processed[j])
                    continue;

                if (HasIKChainDependency(static_cast<int>(i), static_cast<int>(j)))
                {
                    hasDependency = true;
                    break;
                }
            }

            if (!hasDependency)
            {
                m_ikChainOrder.push_back(static_cast<int>(i));
                processed[i] = true;
                i = -1; // Restart scan
            }
        }

        // Add any remaining chains
        for (size_t i = 0; i < m_ikChains.size(); ++i)
        {
            if (!processed[i])
                m_ikChainOrder.push_back(static_cast<int>(i));
        }
    }

    bool Animator::HasIKChainDependency(int chainA, int chainB) const
    {
        if (chainA < 0 || chainA >= static_cast<int>(m_ikChains.size()) || chainB < 0 || chainB >= static_cast<int>(m_ikChains.size()))
            return false;

        const IKChain& a = m_ikChains[chainA];
        const IKChain& b = m_ikChains[chainB];

        // Check if any bone in A is a parent of bones in B
        for (int boneA : a.boneIndices)
        {
            for (int boneB : b.boneIndices)
            {
                // Check if boneA is ancestor of boneB
                if (!m_skeleton)
                    continue;

                int current = boneB;
                while (current >= 0)
                {
                    if (current == boneA)
                        return true; // A must be solved before B

                    if (current >= static_cast<int>(m_skeleton->bones.size()))
                        break;

                    current = m_skeleton->bones[current].parentIndex;
                }
            }
        }

        return false;
    }

    int Animator::AddIKChain(IKSolverType type, const std::vector<int>& boneIndices)
    {
        IKChain chain;
        chain.solverType = type;
        chain.boneIndices = boneIndices;

        // Store rest pose rotations
        if (m_skeleton && chain.useRestPose)
        {
            for (int boneIdx : boneIndices)
            {
                if (boneIdx >= 0 && boneIdx < static_cast<int>(m_skeleton->bones.size()))
                {
                    chain.restPoseRotations.push_back(
                        m_skeleton->bones[boneIdx].localTransform.GetRotation()
                    );
                }
            }
        }

        m_ikChains.push_back(chain);
        m_ikChainOrder.clear(); // Force re-sort

        return static_cast<int>(m_ikChains.size()) - 1;
    }

    void Animator::RemoveIKChain(int chainIndex)
    {
        if (chainIndex < 0 || chainIndex >= static_cast<int>(m_ikChains.size()))
            return;

        m_ikChains.erase(m_ikChains.begin() + chainIndex);
        m_ikChainOrder.clear(); // Force re-sort
    }

    IKChain* Animator::GetIKChain(int chainIndex)
    {
        if (chainIndex < 0 || chainIndex >= static_cast<int>(m_ikChains.size()))
            return nullptr;
        return &m_ikChains[chainIndex];
    }

    void Animator::SetIKTarget(int chainIndex, const Vector3& position, const Quaternion& rotation)
    {
        IKChain* chain = GetIKChain(chainIndex);
        if (chain)
        {
            chain->targetPosition = position;
            chain->targetRotation = rotation;
        }
    }

    void Animator::SetIKWeight(int chainIndex, float weight)
    {
        IKChain* chain = GetIKChain(chainIndex);
        if (chain)
            chain->weight = std::max(0.0f, std::min(1.0f, weight));
    }

    void Animator::SetIKEnabled(int chainIndex, bool enabled)
    {
        IKChain* chain = GetIKChain(chainIndex);
        if (chain)
            chain->enabled = enabled;
    }

    void Animator::SetIKPoleTarget(int chainIndex, const Vector3& poleTarget)
    {
        IKChain* chain = GetIKChain(chainIndex);
        if (chain)
        {
            chain->poleTarget = poleTarget;
            chain->usePoleTarget = true;
        }
    }

    void Animator::ApplyIK()
    {
        if (m_ikChains.empty())
            return;

        // Sort chains by dependency if order is empty or chains changed
        if (m_ikChainOrder.empty() || m_ikChainOrder.size() != m_ikChains.size())
            SortIKChainsByDependency();

        // Solve in dependency order
        for (int chainIndex : m_ikChainOrder)
        {
            if (chainIndex < 0 || chainIndex >= static_cast<int>(m_ikChains.size()))
                continue;

            IKChain& chain = m_ikChains[chainIndex];

            if (!chain.enabled || chain.weight <= 0.0f)
                continue;

            switch (chain.solverType)
            {
                case IKSolverType::TwoBone:
                    SolveTwoBoneIK(chain);
                    break;
                case IKSolverType::LookAt:
                    SolveLookAtIK(chain);
                    break;
                case IKSolverType::FABRIK:
                    SolveFABRIK(chain);
                    break;
                case IKSolverType::CCD:
                    SolveCCDIK(chain);
                    break;
            }
        }

        // Recalculate after all IK
        CalculateBoneTransforms();
    }
    void Animator::SolveTwoBoneIK(IKChain& chain)
    {
        if (chain.boneIndices.size() < 3 || !m_skeleton)
            return;

        int rootIdx = chain.boneIndices[0];
        int midIdx = chain.boneIndices[1];
        int tipIdx = chain.boneIndices[2];

        if (rootIdx >= static_cast<int>(m_boneMatrices.size()) ||
            midIdx >= static_cast<int>(m_boneMatrices.size()) ||
            tipIdx >= static_cast<int>(m_boneMatrices.size()))
            return;

        // Get world space positions
        Vector3 rootPos = m_boneMatrices[rootIdx].GetTranslation();
        Vector3 midPos = m_boneMatrices[midIdx].GetTranslation();
        Vector3 tipPos = m_boneMatrices[tipIdx].GetTranslation();
        Vector3 target = chain.targetPosition;

        // Calculate limb segment lengths
        float upperLen = (midPos - rootPos).Length();
        float lowerLen = (tipPos - midPos).Length();

        if (upperLen < 0.0001f || lowerLen < 0.0001f)
            return;

        // Direction from root to target
        Vector3 rootToTarget = target - rootPos;
        float targetDist = rootToTarget.Length();
        Vector3 targetDir = targetDist > 0.0001f ? rootToTarget / targetDist : Vector3(0.0f, 1.0f, 0.0f);

        // Clamp target to reachable distance
        float chainLen = upperLen + lowerLen;
        float epsilon = 0.001f;

        if (targetDist > chainLen - epsilon)
        {
            targetDist = chainLen - epsilon;
            target = rootPos + targetDir * targetDist;
        }

        float minDist = std::abs(upperLen - lowerLen) + epsilon;
        if (targetDist < minDist)
            targetDist = minDist;

        // Calculate bend direction using pole target
        Vector3 poleDir;
        if (chain.usePoleTarget)
        {
            Vector3 toPole = chain.poleTarget - rootPos;
            Vector3 projPole = toPole - targetDir * Vector3::Dot(toPole, targetDir);
            float poleLen = projPole.Length();
            poleDir = poleLen > 0.0001f ? projPole / poleLen : Vector3(0.0f, 1.0f, 0.0f);
        }
        else
        {
            Vector3 currentMidDir = (midPos - rootPos).Normalize();
            Vector3 projMid = currentMidDir - targetDir * Vector3::Dot(currentMidDir, targetDir);
            float midLen = projMid.Length();
            poleDir = midLen > 0.0001f ? projMid / midLen : Vector3(0.0f, 1.0f, 0.0f);
        }

        // Law of cosines
        float cos_rootAngle = (upperLen * upperLen + targetDist * targetDist - lowerLen * lowerLen) / (2.0f * upperLen * targetDist);
        float cos_midAngle = (upperLen * upperLen + lowerLen * lowerLen - targetDist * targetDist) / (2.0f * upperLen * lowerLen);

        cos_rootAngle = std::max(-1.0f, std::min(1.0f, cos_rootAngle));
        cos_midAngle = std::max(-1.0f, std::min(1.0f, cos_midAngle));

        float rootAngle = std::acos(cos_rootAngle);
        float midAngle = std::acos(cos_midAngle);

        // Calculate new positions
        Vector3 bendAxis = Vector3::Cross(targetDir, poleDir).Normalize();
        if (bendAxis.Length() < 0.0001f)
            bendAxis = { 1.0f, 0.0f, 0.0f };

        Quaternion rootBendRot = Quaternion::FromAxisAngle(bendAxis, rootAngle);
        Vector3 upperDir = rootBendRot * targetDir;
        Vector3 newMidPos = rootPos + upperDir * upperLen;

        // Calculate rotations
        // Upper bone
        Vector3 originalUpperDir = (midPos - rootPos).Normalize();
        Vector3 newUpperDir = (newMidPos - rootPos).Normalize();
        Quaternion upperDeltaRot = Quaternion::FromToRotation(originalUpperDir, newUpperDir);

        Matrix4 parentTransform = Matrix4::Identity();
        if (m_skeleton->bones[rootIdx].parentIndex >= 0)
            parentTransform = m_boneMatrices[m_skeleton->bones[rootIdx].parentIndex];

        Quaternion parentRot = parentTransform.GetRotation();
        Quaternion currentLocalUpperRot = m_localTransforms[rootIdx].GetRotation();

        // Apply delta in world space
        Quaternion currentWorldRot = parentRot * currentLocalUpperRot;
        Quaternion newWorldRot = upperDeltaRot * currentWorldRot;

        // Convert back to local space, accounting for rest pose
        Quaternion newLocalUpperRot = parentRot.Inverse() * newWorldRot;

        // If using rest pose, we need to express rotation relative to rest pose
        if (chain.useRestPose && !chain.restPoseRotations.empty())
        {
            Quaternion restPoseRot = chain.restPoseRotations[0];
            Quaternion restInverse = restPoseRot.Inverse();
            newLocalUpperRot = restPoseRot * (restInverse * newLocalUpperRot);
        }

        // Mid bone
        Vector3 originalLowerDir = (tipPos - midPos).Normalize();
        Vector3 newLowerDir = (target - newMidPos).Normalize();
        Quaternion lowerDeltaRot = Quaternion::FromToRotation(originalLowerDir, newLowerDir);

        Matrix4 midParentTransform = m_boneMatrices[rootIdx];
        Quaternion midParentRot = midParentTransform.GetRotation();
        Quaternion currentLocalMidRot = m_localTransforms[midIdx].GetRotation();

        Quaternion currentMidWorldRot = midParentRot * currentLocalMidRot;
        Quaternion newMidWorldRot = lowerDeltaRot * currentMidWorldRot;
        Quaternion newLocalMidRot = midParentRot.Inverse() * newMidWorldRot;

        if (chain.useRestPose && chain.restPoseRotations.size() > 1)
        {
            Quaternion restPoseRot = chain.restPoseRotations[1];
            Quaternion restInverse = restPoseRot.Inverse();
            newLocalMidRot = restPoseRot * (restInverse * newLocalMidRot);
        }

        // Blend with weight
        Quaternion finalUpperRot = Quaternion::Slerp(currentLocalUpperRot, newLocalUpperRot, chain.weight);
        Quaternion finalMidRot = Quaternion::Slerp(currentLocalMidRot, newLocalMidRot, chain.weight);

        // Apply joint constraints before final blend
        if (!chain.jointConstraints.empty())
        {
            if (chain.jointConstraints.size() > 0)
                finalUpperRot = ApplyJointConstraint(finalUpperRot, chain.jointConstraints[0]);

            if (chain.jointConstraints.size() > 1)
                finalMidRot = ApplyJointConstraint(finalMidRot, chain.jointConstraints[1]);
        }

        // Update transforms
        Vector3 upperTrans = m_localTransforms[rootIdx].GetTranslation();
        Vector3 upperScale = m_localTransforms[rootIdx].GetScale();
        m_localTransforms[rootIdx] = Matrix4::Translate(upperTrans) *Matrix4::FromQuaternion(finalUpperRot) * Matrix4::Scale(upperScale);

        Vector3 midTrans = m_localTransforms[midIdx].GetTranslation();
        Vector3 midScale = m_localTransforms[midIdx].GetScale();
        m_localTransforms[midIdx] = Matrix4::Translate(midTrans) * Matrix4::FromQuaternion(finalMidRot) * Matrix4::Scale(midScale);
    }

    void Animator::SolveLookAtIK(IKChain& chain)
    {
        if (chain.boneIndices.empty() || !m_skeleton)
            return;

        int boneIdx = chain.boneIndices[0];
        if (boneIdx >= static_cast<int>(m_boneMatrices.size()))
            return;

        Vector3 bonePos = m_boneMatrices[boneIdx].GetTranslation();
        Vector3 toTarget = (chain.targetPosition - bonePos).Normalize();
        Vector3 forward(0.0f, 0.0f, 1.0f);

        Quaternion lookRotation = Quaternion::FromToRotation(forward, toTarget);
        Quaternion originalRot = m_localTransforms[boneIdx].GetRotation();
        Quaternion finalRot = Quaternion::Slerp(originalRot, lookRotation, chain.weight);

        Vector3 trans = m_localTransforms[boneIdx].GetTranslation();
        Vector3 scale = m_localTransforms[boneIdx].GetScale();
        m_localTransforms[boneIdx] = Matrix4::Translate(trans) * Matrix4::FromQuaternion(finalRot) * Matrix4::Scale(scale);
    }

    void Animator::SolveFABRIK(IKChain& chain)
    {
        if (chain.boneIndices.size() < 2 || !m_skeleton)
            return;

        // Get initial world positions and rotations
        std::vector<Vector3> positions;
        std::vector<Quaternion> rotations;
        std::vector<Vector3> upVectors;
        std::vector<float> lengths;

        for (size_t i = 0; i < chain.boneIndices.size(); ++i)
        {
            int boneIdx = chain.boneIndices[i];
            if (boneIdx < static_cast<int>(m_boneMatrices.size()))
            {
                positions.push_back(m_boneMatrices[boneIdx].GetTranslation());
                rotations.push_back(m_boneMatrices[boneIdx].GetRotation());

                // Store up vector for twist tracking
                Vector3 up = m_boneMatrices[boneIdx].GetRotation() * Vector3(0.0f, 1.0f, 0.0f);
                upVectors.push_back(up);
            }
        }

        if (positions.size() < 2)
            return;

        // Calculate segment lengths
        for (size_t i = 0; i < positions.size() - 1; ++i)
            lengths.push_back((positions[i + 1] - positions[i]).Length());

        Vector3 rootPos = positions[0];
        Vector3 target = chain.targetPosition;
        Quaternion targetRot = chain.targetRotation;

        // Check reachability
        float totalLength = 0.0f;
        for (float len : lengths)
            totalLength += len;

        float distToTarget = (target - rootPos).Length();
        if (distToTarget > totalLength)
        {
            // Stretch towards target
            Vector3 direction = (target - rootPos).Normalize();
            positions[0] = rootPos;
            for (size_t i = 0; i < lengths.size(); ++i)
                positions[i + 1] = positions[i] + direction * lengths[i];
        }
        else
        {
            // Iterative solving
            for (int iter = 0; iter < static_cast<int>(chain.maxIterations); ++iter)
            {
                // Forward pass
                positions.back() = target;
                for (int i = static_cast<int>(positions.size()) - 2; i >= 0; --i)
                {
                    Vector3 direction = (positions[i] - positions[i + 1]).Normalize();
                    positions[i] = positions[i + 1] + direction * lengths[i];
                }

                // Backward pass
                positions[0] = rootPos;
                for (size_t i = 0; i < positions.size() - 1; ++i)
                {
                    Vector3 direction = (positions[i + 1] - positions[i]).Normalize();
                    positions[i + 1] = positions[i] + direction * lengths[i];
                }

                // Check convergence
                if ((positions.back() - target).Length() < chain.tolerance)
                    break;
            }
        }

        // Update rotations with twist preservation
        for (size_t i = 0; i < chain.boneIndices.size() - 1; ++i)
        {
            int boneIdx = chain.boneIndices[i];
            if (boneIdx >= static_cast<int>(m_localTransforms.size()))
                continue;

            // Calculate bone direction
            Vector3 oldDir = (m_boneMatrices[chain.boneIndices[i + 1]].GetTranslation() - m_boneMatrices[boneIdx].GetTranslation()).Normalize();
            Vector3 newDir = (positions[i + 1] - positions[i]).Normalize();

            // Calculate rotation to align directions
            Quaternion dirRot = Quaternion::FromToRotation(oldDir, newDir);

            // Preserve twist by maintaining up vector
            Vector3 oldUp = upVectors[i];
            Vector3 newUp = dirRot * oldUp;

            // Project up vectors onto plane perpendicular to bone direction
            Vector3 oldUpProj = oldUp - newDir * Vector3::Dot(oldUp, newDir);
            Vector3 newUpProj = newUp - newDir * Vector3::Dot(newUp, newDir);

            float oldUpLen = oldUpProj.Length();
            float newUpLen = newUpProj.Length();

            Quaternion twistRot = { 0.0f, 0.0f, 0.0f, 1.0f };
            if (oldUpLen > 0.0001f && newUpLen > 0.0001f)
            {
                oldUpProj = oldUpProj / oldUpLen;
                newUpProj = newUpProj / newUpLen;
                twistRot = Quaternion::FromToRotation(newUpProj, oldUpProj);
            }

            // Combine direction and twist rotations
            Quaternion finalWorldRot = twistRot * dirRot;

            // Convert to local space
            Matrix4 parentTransform = Matrix4::Identity();
            if (m_skeleton->bones[boneIdx].parentIndex >= 0)
                parentTransform = m_boneMatrices[m_skeleton->bones[boneIdx].parentIndex];

            Quaternion parentRot = parentTransform.GetRotation();
            Quaternion currentLocalRot = m_localTransforms[boneIdx].GetRotation();
            Quaternion currentWorldRot = parentRot * currentLocalRot;

            Quaternion newWorldRot = finalWorldRot * currentWorldRot;
            Quaternion newLocalRot = parentRot.Inverse() * newWorldRot;

            // Apply rest pose if needed
            if (chain.useRestPose && i < chain.restPoseRotations.size())
            {
                Quaternion restPoseRot = chain.restPoseRotations[i];
                Quaternion restInverse = restPoseRot.Inverse();
                newLocalRot = restPoseRot * (restInverse * newLocalRot);
            }

            // Blend with weight
            Quaternion finalRot = Quaternion::Slerp(currentLocalRot, newLocalRot, chain.weight);

            // Update transform
            Vector3 trans = m_localTransforms[boneIdx].GetTranslation();
            Vector3 scale = m_localTransforms[boneIdx].GetScale();
            m_localTransforms[boneIdx] = Matrix4::Translate(trans) * Matrix4::FromQuaternion(finalRot) * Matrix4::Scale(scale);
        }

        // Handle end effector rotation
        if (!chain.boneIndices.empty())
        {
            int tipIdx = chain.boneIndices.back();
            if (tipIdx < static_cast<int>(m_localTransforms.size()))
            {
                Matrix4 parentTransform = Matrix4::Identity();
                if (m_skeleton->bones[tipIdx].parentIndex >= 0)
                    parentTransform = m_boneMatrices[m_skeleton->bones[tipIdx].parentIndex];

                Quaternion parentRot = parentTransform.GetRotation();
                Quaternion currentLocalRot = m_localTransforms[tipIdx].GetRotation();

                // Convert target rotation to local space
                Quaternion newLocalRot = parentRot.Inverse() * targetRot;

                // Blend with weight
                Quaternion finalRot = Quaternion::Slerp(currentLocalRot, newLocalRot, chain.weight);

                Vector3 trans = m_localTransforms[tipIdx].GetTranslation();
                Vector3 scale = m_localTransforms[tipIdx].GetScale();
                m_localTransforms[tipIdx] = Matrix4::Translate(trans) * Matrix4::FromQuaternion(finalRot) * Matrix4::Scale(scale);
            }
        }
    }

    void Animator::SolveCCDIK(IKChain& chain)
    {
        if (chain.boneIndices.size() < 2 || !m_skeleton)
            return;

        // Cyclic Coordinate Descent
        Vector3 target = chain.targetPosition;

        for (int iter = 0; iter < static_cast<int>(chain.maxIterations); ++iter)
        {
            // Work backwards from tip to root
            for (int i = static_cast<int>(chain.boneIndices.size()) - 2; i >= 0; --i)
            {
                int boneIdx = chain.boneIndices[i];
                int tipIdx = chain.boneIndices.back();

                if (boneIdx >= static_cast<int>(m_boneMatrices.size()) ||
                    tipIdx >= static_cast<int>(m_boneMatrices.size()))
                    continue;

                Vector3 bonePos = m_boneMatrices[boneIdx].GetTranslation();
                Vector3 tipPos = m_boneMatrices[tipIdx].GetTranslation();

                Vector3 toTip = (tipPos - bonePos).Normalize();
                Vector3 toTarget = (target - bonePos).Normalize();

                Quaternion rotation = Quaternion::FromToRotation(toTip, toTarget);
                Quaternion originalRot = m_localTransforms[boneIdx].GetRotation();
                Quaternion finalRot = Quaternion::Slerp(originalRot, rotation * originalRot, chain.weight);

                Vector3 trans = m_localTransforms[boneIdx].GetTranslation();
                Vector3 scale = m_localTransforms[boneIdx].GetScale();
                m_localTransforms[boneIdx] = Matrix4::Translate(trans) * Matrix4::FromQuaternion(finalRot) * Matrix4::Scale(scale);

                // Recalculate for next iteration
                CalculateBoneTransforms();
            }

            // Check if we're close enough
            int tipIdx = chain.boneIndices.back();
            Vector3 tipPos = m_boneMatrices[tipIdx].GetTranslation();
            if ((tipPos - target).Length() < chain.tolerance)
                break;
        }
    }

    Vector3 Animator::GetBoneWorldPosition(int boneIndex) const
    {
        if (boneIndex < 0 || boneIndex >= static_cast<int>(m_boneMatrices.size()))
            return { 0.0f, 0.0f, 0.0f };

        return m_boneMatrices[boneIndex].GetTranslation();
    }

    void Animator::SetBoneWorldPosition(int boneIndex, const Vector3& position)
    {
        if (boneIndex < 0 || boneIndex >= static_cast<int>(m_localTransforms.size()))
            return;

        Quaternion rot = m_localTransforms[boneIndex].GetRotation();
        Vector3 scale = m_localTransforms[boneIndex].GetScale();
        m_localTransforms[boneIndex] = Matrix4::Translate(position) * Matrix4::FromQuaternion(rot) * Matrix4::Scale(scale);
    }

    void Animator::SampleAnimation(float time)
    {
        if (!m_currentClip || !m_skeleton)
            return;

        for (size_t i = 0; i < m_skeleton->bones.size(); ++i)
            m_localTransforms[i] = m_skeleton->bones[i].localTransform;

        struct AnimData
        {
            bool hasT = false, hasR = false, hasS = false;
            Vector3 t;
            Quaternion r;
            Vector3 s;
        };
        std::vector<AnimData> animData(m_skeleton->bones.size());

        // Get the animated components
        for (const AnimationChannel& channel : m_currentClip->GetChannels())
        {
            int boneIndex = channel.targetBoneIndex;
            if (boneIndex < 0 || boneIndex >= static_cast<int>(m_skeleton->bones.size()))
                continue;

            if (!channel.translations.empty())
            {
                animData[boneIndex].t = InterpolateTranslation(channel, time);
                animData[boneIndex].hasT = true;
            }

            if (!channel.rotations.empty())
            {
                animData[boneIndex].r = InterpolateRotation(channel, time);
                animData[boneIndex].hasR = true;
            }

            if (!channel.scales.empty())
            {
                animData[boneIndex].s = InterpolateScale(channel, time);
                animData[boneIndex].hasS = true;
            }
        }

        // Reconstruct only bones that have animation
        for (size_t i = 0; i < m_skeleton->bones.size(); ++i)
        {
            if (animData[i].hasT || animData[i].hasR || animData[i].hasS)
            {
                Vector3 t = animData[i].hasT ? animData[i].t : m_skeleton->bones[i].localTransform.GetTranslation();
                Quaternion r = animData[i].hasR ? animData[i].r : m_skeleton->bones[i].localTransform.GetRotation();
                Vector3 s = animData[i].hasS ? animData[i].s : m_skeleton->bones[i].localTransform.GetScale();

                m_localTransforms[i] = Matrix4::Translate(t) * Matrix4::FromQuaternion(r) * Matrix4::Scale(s);
            }
        }
    }

    void Animator::SampleNodeAnimation(float time)
    {
        if (!m_currentClip)
            return;

        const auto& nodeChannels = m_currentClip->GetNodeChannels();

        if (nodeChannels.empty())
        {
            m_animatedNodeTransforms.clear();
            return;
        }

        m_animatedNodeTransforms.clear();

        for (const auto& channel : nodeChannels)
        {
            if (channel.targetNodeIndex < 0)
                continue;

            Vector3 translation = InterpolateNodeTranslation(channel, time);
            Quaternion rotation = InterpolateNodeRotation(channel, time);
            Vector3 scale = InterpolateNodeScale(channel, time);

            // Build the transform matrix
            Matrix4 t = Matrix4::Translate(translation);
            Matrix4 r = Matrix4::FromQuaternion(rotation);
            Matrix4 s = Matrix4::Scale(scale);

            Matrix4 localTransform = t * r * s;

            // Store the transform for this node
            m_animatedNodeTransforms[channel.targetNodeIndex] = localTransform;

            // Update the node transforms vector
            if (channel.targetNodeIndex < static_cast<int>(m_nodeTransforms.size()))
                m_nodeTransforms[channel.targetNodeIndex] = localTransform;
        }
    }

    void Animator::SampleMorphWeights(float time, std::vector<std::shared_ptr<Mesh>>& meshes)
    {
        if (!m_currentClip)
            return;

        const auto& morphChannels = m_currentClip->GetMorphWeightChannels();
        if (morphChannels.empty())
            return;

        for (const auto& channel : morphChannels)
        {
            if (channel.weights.empty() || channel.times.empty())
                continue;

            int index = FindKeyframeIndex(channel.times, time);
            int nextIndex = index + 1;

            float factor = 0.0f;
            if (nextIndex < static_cast<int>(channel.times.size()))
            {
                float t0 = channel.times[index];
                float t1 = channel.times[nextIndex];
                factor = (time - t0) / (t1 - t0);
            }

            // Determine interpolated weights for this keyframe
            std::vector<float> interpolatedWeights(channel.weights[index].size());

            for (size_t w = 0; w < interpolatedWeights.size(); ++w)
            {
                float w0 = channel.weights[index][w];
                float w1 = (nextIndex < static_cast<int>(channel.weights.size())) ? channel.weights[nextIndex][w] : w0;

                if (channel.interpolation == AnimationInterpolation::Step)
                    interpolatedWeights[w] = w0;
                else
                    interpolatedWeights[w] = w0 + (w1 - w0) * factor;
            }

            // Apply to the target mesh
            if (channel.targetNodeIndex >= 0 && channel.targetNodeIndex < static_cast<int>(m_nodeTransforms.size()))
            {
                if (channel.targetNodeIndex >= static_cast<int>(meshes.size()))
                {
                    std::cout << "[WARNING] Failed to apply morph weight during animation." << std::endl;
                    continue;
                }

                meshes[channel.targetNodeIndex]->SetMorphWeights(interpolatedWeights);
            }
        }
    }

    Matrix4 Animator::GetNodeTransform(int nodeIndex) const
    {
        // Check if this node has an animated transform
        auto it = m_animatedNodeTransforms.find(nodeIndex);
        if (it != m_animatedNodeTransforms.end())
            return it->second;

        // Check the vector
        if (nodeIndex >= 0 && nodeIndex < static_cast<int>(m_nodeTransforms.size()))
            return m_nodeTransforms[nodeIndex];

        return Matrix4::Identity();
    }

    Vector3 Animator::InterpolateNodeTranslation(const NodeAnimationChannel& channel, float time) const
    {
        if (channel.translations.empty())
            return { 0.0f, 0.0f, 0.0f };

        if (channel.translations.size() == 1 || channel.times.empty())
            return channel.translations[0];

        int index = FindKeyframeIndex(channel.times, time);

        if (index < 0)
            return channel.translations[0];

        if (index >= static_cast<int>(channel.translations.size()) - 1)
            return channel.translations.back();

        if (channel.interpolation == AnimationInterpolation::Step)
            return channel.translations[index];

        float t0 = channel.times[index];
        float t1 = channel.times[index + 1];
        float factor = (time - t0) / (t1 - t0);

        const Vector3& v0 = channel.translations[index];
        const Vector3& v1 = channel.translations[index + 1];

        return {
            v0.x + (v1.x - v0.x) * factor,
            v0.y + (v1.y - v0.y) * factor,
            v0.z + (v1.z - v0.z) * factor
        };
    }

    Quaternion Animator::InterpolateNodeRotation(const NodeAnimationChannel& channel, float time) const
    {
        if (channel.rotations.empty())
            return { 0.0f, 0.0f, 0.0f, 1.0f };

        if (channel.rotations.size() == 1 || channel.times.empty())
            return channel.rotations[0];

        int index = FindKeyframeIndex(channel.times, time);

        if (index < 0)
            return channel.rotations[0];

        if (index >= static_cast<int>(channel.rotations.size()) - 1)
            return channel.rotations.back();

        if (channel.interpolation == AnimationInterpolation::Step)
            return channel.rotations[index];

        float t0 = channel.times[index];
        float t1 = channel.times[index + 1];
        float factor = (time - t0) / (t1 - t0);

        return Quaternion::Slerp(channel.rotations[index], channel.rotations[index + 1], factor);
    }

    Vector3 Animator::InterpolateNodeScale(const NodeAnimationChannel& channel, float time) const
    {
        if (channel.scales.empty())
            return { 1.0f, 1.0f, 1.0f };

        if (channel.scales.size() == 1 || channel.times.empty())
            return channel.scales[0];

        int index = FindKeyframeIndex(channel.times, time);

        if (index < 0)
            return channel.scales[0];

        if (index >= static_cast<int>(channel.scales.size()) - 1)
            return channel.scales.back();

        if (channel.interpolation == AnimationInterpolation::Step)
            return channel.scales[index];

        float t0 = channel.times[index];
        float t1 = channel.times[index + 1];
        float factor = (time - t0) / (t1 - t0);

        const Vector3& v0 = channel.scales[index];
        const Vector3& v1 = channel.scales[index + 1];

        return {
            v0.x + (v1.x - v0.x) * factor,
            v0.y + (v1.y - v0.y) * factor,
            v0.z + (v1.z - v0.z) * factor
        };
    }

    void Animator::CalculateBoneTransforms()
    {
        if (!m_skeleton)
            return;

        m_boneMatrices.resize(m_skeleton->bones.size());

        std::function<void(int, const Matrix4&)> compute = [&](int index, const Matrix4& parentGlobal)
        {
            const Bone& bone = m_skeleton->bones[index];
            Matrix4 global = parentGlobal * m_localTransforms[index];

            m_boneMatrices[index] = global * bone.inverseBindMatrix;

            for (int child : bone.children)
                compute(child, global);
        };

        for (size_t i = 0; i < m_skeleton->bones.size(); ++i)
        {
            if (m_skeleton->bones[i].parentIndex == -1)
                compute((int)i, Matrix4::Identity());
        }

        m_skeleton->finalMatrices = m_boneMatrices;
    }

    Vector3 Animator::InterpolateTranslation(const AnimationChannel& channel, float time) const
    {
        if (channel.translations.empty())
            return { 0.0f, 0.0f, 0.0f };

        if (channel.translations.size() == 1 || channel.times.empty())
            return channel.translations[0];

        int index = FindKeyframeIndex(channel.times, time);

        if (index < 0)
            return channel.translations[0];

        if (index >= static_cast<int>(channel.translations.size()) - 1)
            return channel.translations.back();

        if (channel.interpolation == AnimationInterpolation::Step)
            return channel.translations[index];

        float t0 = channel.times[index];
        float t1 = channel.times[index + 1];
        float dt = t1 - t0;
        float s = (time - t0) / dt;
        const Vector3& p0 = channel.translations[index];
        const Vector3& p1 = channel.translations[index + 1];

        if (channel.interpolation == AnimationInterpolation::CubicSpline)
        {
            if (channel.outTangents.empty() || channel.inTangents.empty())
                return p0 + (p1 - p0) * s;

            const Vector3& a0 = channel.outTangents[index];
            const Vector3& b1 = channel.inTangents[index + 1];

            float s2 = s * s;
            float s3 = s2 * s;
            float h00 = 2 * s3 - 3 * s2 + 1;
            float h10 = s3 - 2 * s2 + s;
            float h01 = -2 * s3 + 3 * s2;
            float h11 = s3 - s2;

            return p0 * h00 + (a0 * dt) * h10 + p1 * h01 + (b1 * dt) * h11;
        }
        else
            return p0 + (p1 - p0) * s;
    }

    Quaternion Animator::InterpolateRotation(const AnimationChannel& channel, float time) const
    {
        if (channel.rotations.empty())
            return { 0.0f, 0.0f, 0.0f, 1.0f };

        if (channel.rotations.size() == 1 || channel.times.empty())
            return channel.rotations[0];

        int index = FindKeyframeIndex(channel.times, time);
        if (index < 0)
            return channel.rotations[0];

        if (index >= static_cast<int>(channel.rotations.size()) - 1)
            return channel.rotations.back();

        if (channel.interpolation == AnimationInterpolation::Step)
            return channel.rotations[index];

        float t0 = channel.times[index];
        float t1 = channel.times[index + 1];
        float dt = t1 - t0;
        float s = (time - t0) / dt;
        const Quaternion& q0 = channel.rotations[index];
        const Quaternion& q1 = channel.rotations[index + 1];

        if (channel.interpolation == AnimationInterpolation::CubicSpline)
        {
            if (channel.outTangentsQuat.empty() || channel.inTangentsQuat.empty())
                return Quaternion::Slerp(q0, q1, s);

            const Quaternion& a0 = channel.outTangentsQuat[index];
            const Quaternion& b1 = channel.inTangentsQuat[index + 1];

            float s2 = s * s;
            float s3 = s2 * s;
            float h00 = 2 * s3 - 3 * s2 + 1;
            float h10 = s3 - 2 * s2 + s;
            float h01 = -2 * s3 + 3 * s2;
            float h11 = s3 - s2;

            // Interpolate as 4D vector
            Quaternion res = q0 * h00 + (a0 * (h10 * dt)) + q1 * h01 + (b1 * (h11 * dt));
            return res.Normalize();
        }
        else
            return Quaternion::Slerp(q0, q1, s);
    }

    Vector3 Animator::InterpolateScale(const AnimationChannel& channel, float time) const
    {
        if (channel.scales.empty())
            return { 1.0f, 1.0f, 1.0f };

        if (channel.scales.size() == 1 || channel.times.empty())
            return channel.scales[0];

        int index = FindKeyframeIndex(channel.times, time);
        if (index < 0)
            return channel.scales[0];

        if (index >= static_cast<int>(channel.scales.size()) - 1)
            return channel.scales.back();

        if (channel.interpolation == AnimationInterpolation::Step)
            return channel.scales[index];

        float t0 = channel.times[index];
        float t1 = channel.times[index + 1];
        float dt = t1 - t0;
        float s = (time - t0) / dt;
        const Vector3& p0 = channel.scales[index];
        const Vector3& p1 = channel.scales[index + 1];

        if (channel.interpolation == AnimationInterpolation::CubicSpline)
        {
            if (channel.outTangentsScale.empty() || channel.inTangentsScale.empty())
                return p0 + (p1 - p0) * s;

            const Vector3& a0 = channel.outTangentsScale[index];
            const Vector3& b1 = channel.inTangentsScale[index + 1];

            float s2 = s * s;
            float s3 = s2 * s;
            float h00 = 2 * s3 - 3 * s2 + 1;
            float h10 = s3 - 2 * s2 + s;
            float h01 = -2 * s3 + 3 * s2;
            float h11 = s3 - s2;

            return p0 * h00 + (a0 * dt) * h10 + p1 * h01 + (b1 * dt) * h11;
        }
        else
            return p0 + (p1 - p0) * s;
    }

    int Animator::FindKeyframeIndex(const std::vector<float>& times, float time) const
    {
        if (times.empty())
            return -1;

        for (size_t i = 0; i < times.size() - 1; ++i)
        {
            if (time >= times[i] && time < times[i + 1])
                return static_cast<int>(i);
        }

        if (time >= times.back())
            return static_cast<int>(times.size()) - 1;

        return 0;
    }
}
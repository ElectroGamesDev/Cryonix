#pragma once
#include "Maths.h"
#include <vector>
#include <string>
#include <unordered_map>
#include "Mesh.h"
#include <functional>

namespace cl
{
    struct Bone
    {
        std::string name;
        int parentIndex;
        std::vector<int> children;
        Matrix4 inverseBindMatrix;
        Matrix4 localTransform;

        Bone() : parentIndex(-1), inverseBindMatrix(Matrix4::Identity()), localTransform(Matrix4::Identity()) {}
    };

    struct Skeleton
    {
        std::vector<Bone> bones;
        std::unordered_map<std::string, int> boneMap;
        std::vector<Matrix4> finalMatrices;

        int FindBoneIndex(std::string_view name) const
        {
            auto it = boneMap.find(name.data());
            if (it != boneMap.end())
                return it->second;

            return -1;
        }

        void ComputeBoneMatrix(int index, const Matrix4& parentTransform)
        {
            Bone& bone = bones[index];

            Matrix4 globalTransform = parentTransform * bone.localTransform;

            finalMatrices[index] = globalTransform * bone.inverseBindMatrix;

            for (int child : bone.children)
                ComputeBoneMatrix(child, globalTransform);
        }


        void UpdateFinalMatrices()
        {
            if (bones.empty())
                return;

            finalMatrices.resize(bones.size());

            for (size_t i = 0; i < bones.size(); ++i)
            {
                if (bones[i].parentIndex == -1)
                    ComputeBoneMatrix((int)i, Matrix4::Identity());
            }
        }
    };

    enum class AnimationInterpolation
    {
        Linear,
        Step,
        CubicSpline
    };

    struct AnimationKeyframe
    {
        float time;
        Vector3 translation;
        Quaternion rotation;
        Vector3 scale;

        AnimationKeyframe() : time(0.0f), translation(0.0f, 0.0f, 0.0f), rotation(0.0f, 0.0f, 0.0f, 1.0f), scale(1.0f, 1.0f, 1.0f) {}
    };

    struct NodeAnimationChannel
    {
        int targetNodeIndex;
        std::vector<float> times;
        std::vector<Vector3> translations;
        std::vector<Quaternion> rotations;
        std::vector<Vector3> scales;
        AnimationInterpolation interpolation;

        // Cubic spline interpolation
        std::vector<Vector3> inTangents;
        std::vector<Vector3> outTangents;
        std::vector<Vector3> inTangentsScale;
        std::vector<Vector3> outTangentsScale;
        std::vector<Quaternion> inTangentsQuat;
        std::vector<Quaternion> outTangentsQuat;

        NodeAnimationChannel() : targetNodeIndex(-1), interpolation(AnimationInterpolation::Linear) {}
    };

    struct AnimationChannel
    {
        int targetBoneIndex;
        std::vector<float> times;
        std::vector<Vector3> translations;
        std::vector<Quaternion> rotations;
        std::vector<Vector3> scales;
        AnimationInterpolation interpolation;

        // Cubic spline interpolation
        std::vector<Vector3> inTangents;
        std::vector<Vector3> outTangents;
        std::vector<Vector3> inTangentsScale;
        std::vector<Vector3> outTangentsScale;
        std::vector<Quaternion> inTangentsQuat;
        std::vector<Quaternion> outTangentsQuat;

        AnimationChannel() : targetBoneIndex(-1), interpolation(AnimationInterpolation::Linear) {}
    };

    struct MorphWeightChannel
    {
        int targetNodeIndex;
        std::vector<float> times;
        std::vector<std::vector<float>> weights; // [keyframe][target_index]
        AnimationInterpolation interpolation;

        MorphWeightChannel() : targetNodeIndex(-1), interpolation(AnimationInterpolation::Linear) {}
    };


    enum class AnimationType
    {
        Skeletal,
        NodeBased
    };

    enum class AnimationBlendMode
    {
        Override,
        Additive,
        Blend
    };

    class AnimationClip;
    class Animator;

    // Animation Events
    struct AnimationEvent
    {
        float time;
        std::string eventName;
        std::string stringParameter;
        float floatParameter;
        int intParameter;

        AnimationEvent() : time(0.0f), floatParameter(0.0f), intParameter(0) {}
        AnimationEvent(float t, std::string_view name) : time(t), eventName(name), floatParameter(0.0f), intParameter(0) {}
    };

    using AnimationEventCallback = std::function<void(const AnimationEvent&)>;

    struct FiredEvent
    {
        float time;
        std::string eventName;

        bool operator==(const FiredEvent& other) const
        {
            return std::abs(time - other.time) < 0.001f && eventName == other.eventName;
        }
    };

    // Root Motion
    struct RootMotionData
    {
        Vector3 deltaPosition;
        Quaternion deltaRotation;
        bool extractPosition;
        bool extractRotation;
        int rootBoneIndex;

        RootMotionData()
            : deltaPosition(0.0f, 0.0f, 0.0f)
            , deltaRotation(0.0f, 0.0f, 0.0f, 1.0f)
            , extractPosition(true)
            , extractRotation(true)
            , rootBoneIndex(0) {
        }
    };

    // IK System
    enum class IKSolverType
    {
        TwoBone, // For limbs
        LookAt, // For head/spine
        FABRIK, // Full body IK
        CCD // Cyclic Coordinate Descent
    };

    struct JointConstraint
    {
        enum class Type { None, Hinge, BallAndSocket, Cone };

        Type type;
        Vector3 axis; // For hinge joints
        float minAngle; // Constraint limits
        float maxAngle;
        Vector3 twistAxis; // For cone constraints
        float coneAngle;

        JointConstraint()
            : type(Type::None)
            , axis(0.0f, 1.0f, 0.0f)
            , minAngle(-180.0f)
            , maxAngle(180.0f)
            , twistAxis(0.0f, 1.0f, 0.0f)
            , coneAngle(45.0f) {
        }
    };


    struct IKChain
    {
        IKSolverType solverType;
        std::vector<int> boneIndices; // Chain of bones from root to tip
        Vector3 targetPosition;
        Quaternion targetRotation;
        float weight;
        bool enabled;

        // Two-bone specific
        Vector3 poleTarget; // For elbow/knee direction
        bool usePoleTarget;

        // Constraints
        float maxIterations;
        float tolerance;
        std::vector<JointConstraint> jointConstraints;

        // Rest pose support
        std::vector<Quaternion> restPoseRotations;
        bool useRestPose;

        IKChain()
            : solverType(IKSolverType::TwoBone)
            , targetPosition(0.0f, 0.0f, 0.0f)
            , targetRotation(0.0f, 0.0f, 0.0f, 1.0f)
            , weight(1.0f)
            , enabled(true)
            , poleTarget(0.0f, 1.0f, 0.0f)
            , usePoleTarget(false)
            , maxIterations(10)
            , tolerance(0.001f)
            , useRestPose(true) {}
    };

    // State Machine
    struct AnimationTransition
    {
        int fromStateId;
        int toStateId;
        float duration;
        float exitTime;
        bool hasExitTime;
        bool canInterrupt;
        float interruptibleAfter;

        // Conditions
        std::string conditionParameter;
        enum class ConditionType
        {
            Greater,
            Less,
            Equal,
            NotEqual,
            True,
            False
        };
        ConditionType conditionType;
        float conditionValue;

        struct Condition
        {
            std::string parameter;
            ConditionType type;
            float value;

            Condition() : type(ConditionType::True), value(0.0f) {}
            Condition(const std::string& p, ConditionType t, float v)
                : parameter(p), type(t), value(v) {
            }
        };

        std::vector<Condition> conditions;
        bool useMultipleConditions;

        AnimationTransition()
            : fromStateId(-1), toStateId(-1), duration(0.3f)
            , exitTime(0.75f), hasExitTime(true)
            , canInterrupt(true), interruptibleAfter(0.0f)
            , conditionType(ConditionType::True), conditionValue(0.0f)
            , useMultipleConditions(false) {}
    };

    struct BlendTreeNode
    {
        enum class Type
        {
            Clip,
            Blend1D,
            Blend2D,
            Additive
        };

        Type type;
        AnimationClip* clip;
        std::vector<BlendTreeNode*> children;
        std::vector<float> thresholds;
        std::vector<Vector2> positions;

        BlendTreeNode() : type(Type::Clip), clip(nullptr) {}
        ~BlendTreeNode() { for (auto* child : children) delete child; }
    };

    struct AnimationState
    {
        int id;
        std::string name;
        AnimationClip* clip;
        BlendTreeNode* blendTree;
        float speed;
        bool loop;
        int layer;
        std::vector<AnimationTransition> transitions;

        // State callbacks
        std::function<void()> onEnter;
        std::function<void()> onExit;
        std::function<void(float)> onUpdate;

        AnimationState()
            : id(-1), clip(nullptr), blendTree(nullptr), speed(1.0f), loop(true), layer(0) {}
    };

    class AnimationStateMachine
    {
    public:
        AnimationStateMachine();
        ~AnimationStateMachine();

        int CreateState(const std::string& name, AnimationClip* clip = nullptr);
        void RemoveState(int stateId);
        AnimationState* GetState(int stateId);
        AnimationState* GetState(const std::string& name);

        void AddTransition(int fromStateId, int toStateId, float duration = 0.3f);
        void SetTransitionCondition(int fromStateId, int toStateId, const std::string& parameter, AnimationTransition::ConditionType type, float value);
        void AddTransitionCondition(int fromStateId, int toStateId, const std::string& parameter, AnimationTransition::ConditionType type, float value);

        void SetStateLayer(int stateId, int layer);
        void SetLayerWeight(int layer, float weight);
        float GetLayerWeight(int layer) const;
        void SetLayerMask(int layer, const std::vector<int>& boneIndices);

        void SetCurrentState(int stateId);
        int GetCurrentStateId() const { return m_currentStateId; }
        AnimationState* GetCurrentState();

        void SetParameter(const std::string& name, float value);
        void SetParameter(const std::string& name, bool value);
        float GetParameterFloat(const std::string& name) const;
        bool GetParameterBool(const std::string& name) const;

        void Update(float deltaTime, Animator* animator, std::vector<std::shared_ptr<Mesh>>& meshes);

        bool IsTransitioning() const { return m_isTransitioning; }
        float GetTransitionProgress() const;

    private:
        std::vector<AnimationState> m_states;
        std::unordered_map<std::string, int> m_stateNames;
        int m_nextStateId;
        int m_currentStateId;
        float m_currentStateTime;
        Vector3 m_blendedRootMotionDelta;
        Quaternion m_blendedRootMotionRotation;

        // Transition state
        bool m_isTransitioning;
        AnimationTransition m_activeTransition;
        float m_transitionTime;
        int m_transitionTargetStateId;

        // Parameters
        std::unordered_map<std::string, float> m_floatParameters;
        std::unordered_map<std::string, bool> m_boolParameters;

        // Layers
        std::unordered_map<int, float> m_layerWeights;
        std::unordered_map<int, std::vector<int>> m_layerMasks;

        void BlendRootMotion(Animator* animator, AnimationClip* fromClip, AnimationClip* toClip, float blend);
        void EvaluateLayeredStates(float deltaTime, Animator* animator, std::vector<std::shared_ptr<Mesh>>& meshes);

        bool CheckTransitionConditions(const AnimationTransition& transition);
        void StartTransition(const AnimationTransition& transition);
    };

    struct AnimationLayer
    {
        int id;
        AnimationClip* clip;
        float weight;
        float timeScale;
        float currentTime;
        bool loop;
        AnimationBlendMode blendMode;
        int priority;
        bool active;

        AnimationLayer()
            : id(-1), clip(nullptr), weight(1.0f), timeScale(1.0f), currentTime(0.0f)
            , loop(true), blendMode(AnimationBlendMode::Override), priority(0), active(false) {}
    };

    struct CrossfadeInfo
    {
        AnimationClip* fromClip;
        AnimationClip* toClip;
        float duration;
        float elapsed;
        bool active;
        int targetLayer;

        CrossfadeInfo() : fromClip(nullptr), toClip(nullptr), duration(0.0f), elapsed(0.0f), active(false), targetLayer(0) {}
    };

    class AnimationClip
    {
    public:
        static std::vector<AnimationClip*> s_clips;

        AnimationClip();
        ~AnimationClip();
        AnimationClip(const AnimationClip&) = delete;
        AnimationClip& operator=(const AnimationClip&) = delete;
        AnimationClip(AnimationClip&& other) noexcept;
        AnimationClip& operator=(AnimationClip&& other) noexcept;

        void SetName(std::string_view name) { m_name = name; }
        const std::string& GetName() const { return m_name; }

        void SetDuration(float duration) { m_duration = duration; }
        float GetDuration() const { return m_duration; }

        void AddEvent(const AnimationEvent& event);
        const std::vector<AnimationEvent>& GetEvents() const { return m_events; }
        void ClearEvents() { m_events.clear(); }

        void SetRootMotionEnabled(bool enabled) { m_rootMotionEnabled = enabled; }
        bool IsRootMotionEnabled() const { return m_rootMotionEnabled; }
        void SetRootBoneIndex(int index) { m_rootBoneIndex = index; }
        int GetRootBoneIndex() const { return m_rootBoneIndex; }

        void AddChannel(const AnimationChannel& channel) { m_channels.push_back(channel); }
        const std::vector<AnimationChannel>& GetChannels() const { return m_channels; }

        void AddNodeChannel(const NodeAnimationChannel& channel) { m_nodeChannels.push_back(channel); }
        const std::vector<NodeAnimationChannel>& GetNodeChannels() const { return m_nodeChannels; }

        void SetAnimationType(AnimationType type) { m_animationType = type; }
        AnimationType GetAnimationType() const { return m_animationType; }

        void AddMorphWeightChannel(const MorphWeightChannel& channel) { m_morphWeightChannels.push_back(channel); }
        const std::vector<MorphWeightChannel>& GetMorphWeightChannels() const { return m_morphWeightChannels; }

        void Destroy();

    private:
        std::string m_name;
        float m_duration;
        std::vector<AnimationEvent> m_events;
        bool m_rootMotionEnabled;
        int m_rootBoneIndex;
        std::vector<AnimationChannel> m_channels;
        std::vector<NodeAnimationChannel> m_nodeChannels;
        std::vector<MorphWeightChannel> m_morphWeightChannels;
        AnimationType m_animationType = AnimationType::Skeletal;

        void SortEvents();
    };

    class Animator
    {
        friend class AnimationStateMachine;

    public:
        Animator();
        ~Animator();

        void SetSkeleton(Skeleton* skeleton);
        Skeleton* GetSkeleton() const { return m_skeleton; }

        void PlayAnimation(AnimationClip* clip, bool loop = true);
        void StopAnimation();
        void PauseAnimation();
        void ResumeAnimation();

        void Update(float deltaTime, std::vector<std::shared_ptr<Mesh>>& meshes);

        void SetSpeed(float speed) { m_speed = speed; }
        float GetSpeed() const { return m_speed; }

        void SetTime(float time);
        float GetTime() const { return m_currentTime; }

        bool IsPlaying() const { return m_playing && !m_paused; }
        bool IsPaused() const { return m_paused; }

        void SetLooping(bool loop) { m_loop = loop; }
        bool IsLooping() const { return m_loop; }

        void SetLocalTransforms(const std::vector<Matrix4>& transforms) { m_localTransforms = transforms; }
        const std::vector<Matrix4>& GetLocalTransforms() const { return m_localTransforms; }

        // Multi-layer animation
        int CreateLayer(const std::string& name, int priority = 0);
        void RemoveLayer(int layerIndex);
        void SetLayerWeight(int layerIndex, float weight);
        float GetLayerWeight(int layerIndex) const;
        void SetLayerBlendMode(int layerIndex, AnimationBlendMode mode);
        bool PlayAnimationOnLayer(int layerIndex, AnimationClip* clip, bool loop = true);
        void StopLayer(int layerIndex);

        // Crossfading
        void CrossfadeToAnimation(AnimationClip* clip, float duration, bool loop = true, int layerIndex = 0);
        bool IsCrossfading() const { return m_crossfade.active; }
        float GetCrossfadeProgress() const;

        // Blend trees
        void SetBlendTreeRoot(BlendTreeNode* root) { m_blendTreeRoot = root; }
        BlendTreeNode* GetBlendTreeRoot() const { return m_blendTreeRoot; }
        void SetBlendParameter(float value) { m_blendParameter = value; }
        void SetBlendParameter2D(float x, float y) { m_blendParameter = x; m_blendParameterY = y; }
        float GetBlendParameter() const { return m_blendParameter; }

        // Bone masking
        void SetBoneMask(const std::vector<int>& boneIndices, int layerIndex = 0);
        void ClearBoneMask(int layerIndex = 0);
        const std::vector<int>& GetBoneMask(int layerIndex = 0) const;

        // Animation sync
        void SyncLayerToLayer(int sourceLayer, int targetLayer);
        void SetLayerTimeScale(int layerIndex, float scale);

        // Additive animations
        void SetAdditiveReferenceClip(AnimationClip* clip) { m_additiveRefClip = clip; }
        AnimationClip* GetAdditiveReferenceClip() const { return m_additiveRefClip; }

        // Root Motion
        void SetRootMotionEnabled(bool enabled) { m_rootMotionEnabled = enabled; }
        bool IsRootMotionEnabled() const { return m_rootMotionEnabled; }
        const RootMotionData& GetRootMotion() const { return m_rootMotion; }
        void ClearRootMotion();
        void SetRootMotionScale(const Vector3& scale) { m_rootMotionScale = scale; }

        // Animation Events
        void SetEventCallback(AnimationEventCallback callback) { m_eventCallback = callback; }
        void ClearEventCallback() { m_eventCallback = nullptr; }

        // IK System
        int AddIKChain(IKSolverType type, const std::vector<int>& boneIndices);
        void RemoveIKChain(int chainIndex);
        IKChain* GetIKChain(int chainIndex);
        void SetIKTarget(int chainIndex, const Vector3& position, const Quaternion& rotation);
        void SetIKWeight(int chainIndex, float weight);
        void SetIKEnabled(int chainIndex, bool enabled);
        void SetIKPoleTarget(int chainIndex, const Vector3& poleTarget);

        // State Machine
        void SetStateMachine(AnimationStateMachine* stateMachine);
        AnimationStateMachine* GetStateMachine() const { return m_stateMachine; }
        bool HasStateMachine() const { return m_stateMachine != nullptr; }

        const std::vector<Matrix4>& GetBoneMatrices() const { return m_boneMatrices; }
        const std::vector<Matrix4>& GetFinalBoneMatrices() const
        {
            if (!m_skeleton)
                return {};

            return m_skeleton->finalMatrices;
        }

        AnimationClip* GetCurrentClip() const { return m_currentClip; }

        const std::vector<Matrix4>& GetNodeTransforms() const { return m_nodeTransforms; }
        Matrix4 GetNodeTransform(int nodeIndex) const;

    private:
        Skeleton* m_skeleton;
        AnimationClip* m_currentClip;
        std::vector<Matrix4> m_boneMatrices;
        std::vector<Matrix4> m_localTransforms;

        std::vector<Matrix4> m_nodeTransforms;
        std::unordered_map<int, Matrix4> m_animatedNodeTransforms;

        std::vector<AnimationLayer> m_layers;
        std::unordered_map<int, size_t> m_layerIdToIndex;
        std::unordered_map<std::string, int> m_layerNames;
        std::unordered_map<int, std::vector<int>> m_boneMasks;
        CrossfadeInfo m_crossfade;
        BlendTreeNode* m_blendTreeRoot;
        float m_blendParameter;
        float m_blendParameterY;
        AnimationClip* m_additiveRefClip;
        std::vector<Matrix4> m_additiveBaseTransforms;
        int m_nextLayerId;

        // Root Motion
        bool m_rootMotionEnabled;
        RootMotionData m_rootMotion;
        Vector3 m_rootMotionScale;
        Vector3 m_previousRootPosition;
        Quaternion m_previousRootRotation;

        // Animation Events
        AnimationEventCallback m_eventCallback;
        std::vector<FiredEvent> m_firedEvents;

        // IK System
        std::vector<IKChain> m_ikChains;
        std::vector<int> m_ikChainOrder;
        bool m_ikEnabled;

        // State Machine
        AnimationStateMachine* m_stateMachine;

        float m_currentTime;
        float m_speed;
        bool m_playing;
        bool m_paused;
        bool m_loop;

        struct BoneTransform
        {
            Vector3 translation;
            Quaternion rotation;
            Vector3 scale;
            bool hasTranslation = false;
            bool hasRotation = false;
            bool hasScale = false;
        };

        int GetLayerIndex(int layerId) const;
        void UpdateLayers(float deltaTime, std::vector<std::shared_ptr<Mesh>>& meshes);
        void UpdateCrossfade(float deltaTime);
        void UpdateBlendTree(float deltaTime);
        void SampleAnimationToBoneTransforms(AnimationClip* clip, float time, std::vector<BoneTransform>& transforms);
        void BlendBoneTransformsToMatrices(const std::vector<BoneTransform>& from, const std::vector<BoneTransform>& to, float weight, std::vector<Matrix4>& result);
        void BlendBoneTransforms(const std::vector<Matrix4>& from, const std::vector<Matrix4>& to, float weight, std::vector<Matrix4>& result, int layerId = -1);
        void ApplyAdditiveAnimation(const std::vector<Matrix4>& additive, std::vector<Matrix4>& result);
        void SampleAnimationToBuffer(AnimationClip* clip, float time, std::vector<Matrix4>& buffer);
        void EvaluateBlendTree(BlendTreeNode* node, float time, std::vector<Matrix4>& result);
        Matrix4 BlendMatrices(const Matrix4& a, const Matrix4& b, float t);

        // Root motion, IK, and events
        void UpdateRootMotion(AnimationClip* clip, float deltaTime);
        void ProcessAnimationEvents(AnimationClip* clip, float prevTime, float currentTime);
        Quaternion ApplyJointConstraint(const Quaternion& rotation, const JointConstraint& constraint);
        void SortIKChainsByDependency();
        bool HasIKChainDependency(int chainA, int chainB) const;
        void ApplyIK();
        void SolveTwoBoneIK(IKChain& chain);
        void SolveLookAtIK(IKChain& chain);
        void SolveFABRIK(IKChain& chain);
        void SolveCCDIK(IKChain& chain);
        Vector3 GetBoneWorldPosition(int boneIndex) const;
        void SetBoneWorldPosition(int boneIndex, const Vector3& position);

        void CalculateBoneTransforms();
        void SampleAnimation(float time);
        void SampleNodeAnimation(float time);
        void SampleMorphWeights(float time, std::vector<std::shared_ptr<Mesh>>& meshes);
        Vector3 InterpolateTranslation(const AnimationChannel& channel, float time) const;
        Quaternion InterpolateRotation(const AnimationChannel& channel, float time) const;
        Vector3 InterpolateScale(const AnimationChannel& channel, float time) const;

        Vector3 InterpolateNodeTranslation(const NodeAnimationChannel& channel, float time) const;
        Quaternion InterpolateNodeRotation(const NodeAnimationChannel& channel, float time) const;
        Vector3 InterpolateNodeScale(const NodeAnimationChannel& channel, float time) const;

        int FindKeyframeIndex(const std::vector<float>& times, float time) const;
    };
}
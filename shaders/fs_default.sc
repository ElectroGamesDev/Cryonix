$input v_worldPos, v_normal, v_tangent, v_bitangent, v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

// Material Maps
SAMPLER2D(u_AlbedoMap, 0);
SAMPLER2D(u_NormalMap, 1);
SAMPLER2D(u_MetallicMap, 2);
SAMPLER2D(u_RoughnessMap, 3);
SAMPLER2D(u_MetallicRoughnessMap, 4);
SAMPLER2D(u_AOMap, 5);
SAMPLER2D(u_EmissiveMap, 6);
SAMPLER2D(u_HeightMap, 7);
SAMPLER2D(u_OpacityMap, 8);

// Material map availability flags
uniform vec4 u_MaterialFlags0; // x=hasAlbedo, y=hasNormal, z=hasMetallic, w=hasRoughness
uniform vec4 u_MaterialFlags1; // x=hasMetallicRoughness, y=hasAO, z=hasEmissive, w=hasOpacity
uniform vec4 u_MaterialFlags2; // x=useTexCoord1ForAO, y=useTexCoord1ForEmissive, z-w=unused

// Material properties (used as tints)
uniform vec4 u_Albedo; // RGB: albedo color, A: unused
uniform vec4 u_EmissiveParams; // RGB: emissive color, A: unused
uniform vec4 u_MaterialProps; // x=metallic, y=roughness, z=ao, w=unused

// Lighting
uniform vec4 u_CameraPos; // Camera position in world space
uniform vec4 u_LightDir; // Directional light direction
uniform vec4 u_LightColor; // Directional light color and intensity
uniform vec4 u_AmbientColor; // Ambient/IBL color
uniform vec4 u_LightingControl;// x=enableLighting (0=unlit, 1=lit), y-w=unused

// Constants
#define PI 3.14159265359
#define MIN_ROUGHNESS 0.04
#define F0_DIELECTRIC 0.04

// PBR Helper Functions

// Normal Distribution Function (GGX/Trowbridge-Reitz)
float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;
   
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
   
    return nom / max(denom, 0.0000001);
}

// Geometry Function (Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
   
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
   
    return nom / max(denom, 0.0000001);
}

// Smith's method for geometry obstruction
float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
   
    return ggx1 * ggx2;
}

// Fresnel-Schlick approximation
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Fresnel with roughness for IBL
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3_splat(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

// Unpack normal from normal map
vec3 GetNormalFromMap(vec2 uv, mat3 TBN)
{
    vec3 tangentNormal = texture2D(u_NormalMap, uv).xyz * 2.0 - 1.0;
    return normalize(mul(TBN, tangentNormal));
}

// Main Shader

void main()
{
    vec2 uv0 = v_texcoord0;
    vec2 uv1 = v_texcoord1;
   
    // Check if lighting is enabled
    bool enableLighting = u_LightingControl.x > 0.5;
   
    // Sample Material Maps and Apply Tints
   
    // Albedo with tint
    vec4 albedoWithAlpha = u_Albedo;
    if (u_MaterialFlags0.x > 0.5)
    {
        vec4 albedoSample = texture2D(u_AlbedoMap, uv0);
        albedoWithAlpha.rgb *= albedoSample.rgb;
        albedoWithAlpha.a *= albedoSample.a;
    }
    vec3 albedo = albedoWithAlpha.rgb;
   
    // Metallic & Roughness with tints
    float metallic = u_MaterialProps.x;
    float roughness = u_MaterialProps.y;
   
    // Check for combined metallic-roughness map (glTF standard: B=metallic, G=roughness)
    if (u_MaterialFlags1.x > 0.5)
    {
        vec3 metallicRoughness = texture2D(u_MetallicRoughnessMap, uv0).rgb;
        metallic *= metallicRoughness.b;
        roughness *= metallicRoughness.g;
    }
    else
    {
        // Sample separate maps
        if (u_MaterialFlags0.z > 0.5)
        {
            metallic *= texture2D(u_MetallicMap, uv0).r;
        }
        if (u_MaterialFlags0.w > 0.5)
        {
            roughness *= texture2D(u_RoughnessMap, uv0).r;
        }
    }
   
    // Clamp roughness to avoid artifacts
    roughness = max(roughness, MIN_ROUGHNESS);
   
    // Ambient Occlusion with tint
    // Can use either UV set (common for lightmaps to use UV1)
    float ao = u_MaterialProps.z;
    if (u_MaterialFlags1.y > 0.5)
    {
        vec2 aoUV = (u_MaterialFlags2.x > 0.5) ? uv1 : uv0;
        ao *= texture2D(u_AOMap, aoUV).r;
    }
   
    // Emissive with tint
    // Can use either UV set
    vec3 emissive = u_EmissiveParams.rgb;
    if (u_MaterialFlags1.z > 0.5)
    {
        vec2 emissiveUV = (u_MaterialFlags2.y > 0.5) ? uv1 : uv0;
        emissive *= texture2D(u_EmissiveMap, emissiveUV).rgb;
    }
   
    // Opacity - combine albedo alpha and opacity map
    float opacity = albedoWithAlpha.a;
    if (u_MaterialFlags1.w > 0.5)
    {
        opacity *= texture2D(u_OpacityMap, uv0).r;
    }
   
    // Final Color Assembly
   
    vec3 color;
   
    if (!enableLighting)
    {
        // Unlit mode - just albedo + emissive
        color = albedo + emissive;
    }
    else
    {       
        // Normal Mapping
       
        vec3 N;
        if (u_MaterialFlags0.y > 0.5)
        {
            // Build TBN matrix
            vec3 T = normalize(v_tangent);
            vec3 B = normalize(v_bitangent);
            vec3 Nv = normalize(v_normal);
            mat3 TBN = mtxFromCols(T, B, Nv);
           
            N = GetNormalFromMap(uv0, TBN);
        }
        else
        {
            N = normalize(v_normal);
        }
       
        // Lighting Setup
       
        vec3 V = normalize(u_CameraPos.xyz - v_worldPos);
        vec3 L = normalize(-u_LightDir.xyz);
        vec3 H = normalize(V + L);
       
        float NdotV = max(dot(N, V), 0.0000001);
        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float HdotV = max(dot(H, V), 0.0);
       
        // PBR Calculation
       
        // Base reflectivity (F0) - lerp between dielectric and metallic
        vec3 F0 = vec3_splat(F0_DIELECTRIC);
        F0 = lerp(F0, albedo, metallic);
       
        // Cook-Torrance BRDF
        float D = DistributionGGX(NdotH, roughness);
        float G = GeometrySmith(NdotV, NdotL, roughness);
        vec3 F = FresnelSchlick(HdotV, F0);
       
        vec3 numerator = D * G * F;
        float denominator = 4.0 * NdotV * NdotL;
        vec3 specular = numerator / max(denominator, 0.0000001);
       
        // Energy conservation
        vec3 kS = F;
        vec3 kD = vec3_splat(1.0) - kS;
        kD *= 1.0 - metallic; // Metallic surfaces don't have diffuse
       
        // Lambertian diffuse
        vec3 diffuse = kD * albedo / PI;
       
        // Direct lighting
        vec3 radiance = u_LightColor.rgb * u_LightColor.a;
        vec3 Lo = (diffuse + specular) * radiance * NdotL;
       
        // Ambient/IBL Approximation
       
        vec3 F_ambient = FresnelSchlickRoughness(NdotV, F0, roughness);
        vec3 kS_ambient = F_ambient;
        vec3 kD_ambient = vec3_splat(1.0) - kS_ambient;
        kD_ambient *= 1.0 - metallic;
       
        vec3 ambient = albedo * (kD_ambient + kS_ambient) * u_AmbientColor.rgb * ao;
       
        // Combine lighting and emissive
       
        color = ambient + Lo + emissive;
    }
   
    // Post Processing
   
    // Tone mapping (ACES approximation)
    //color = color / (color + vec3_splat(1.0));
   
    // Gamma correction
    //color = pow(color, vec3_splat(1.0 / 2.2));
   
    gl_FragColor = vec4(color, opacity);
}
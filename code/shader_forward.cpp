#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "toon_blinn_phong_lighting.cpp"
#include "descriptor_layouts.cpp"

MATERIAL_DESCRIPTOR_LAYOUT(0)
SCENE_DESCRIPTOR_LAYOUT(1)

layout(set = 2, binding = 0) uniform sampler2D DepthBuffer;
layout(set = 2, binding = 1) uniform sampler2D PerlinNoise;
layout(set = 2, binding = 2) uniform sampler2D Distortion;
layout(set = 2, binding = 3) uniform water_inputs
{
    float Time;
} WaterInputs;

#if FORWARD_VERTEX

layout(location = 0) in vec3 InPos;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec2 InUv;

layout(location = 0) out vec3 OutWorldPos;
layout(location = 1) out vec3 OutWorldNormal;
layout(location = 2) out vec2 OutUv;
layout(location = 3) out flat uint OutInstanceId;

void main()
{
    instance_entry Entry = InstanceBuffer[gl_InstanceIndex];
    
    gl_Position = Entry.WVPTransform * vec4(InPos, 1);
    OutWorldPos = (Entry.WTransform * vec4(InPos, 1)).xyz;
    OutWorldNormal = (Entry.WTransform * vec4(InNormal, 0)).xyz;
    OutUv = InUv;
    OutInstanceId = gl_InstanceIndex;
}

#endif

#if FORWARD_FRAGMENT

layout(location = 0) in vec3 InWorldPos;
layout(location = 1) in vec3 InWorldNormal;
layout(location = 2) in vec2 InUv;
layout(location = 3) in flat uint InInstanceId;

layout(location = 0) out vec4 OutColor;

void main()
{
    instance_entry Entry = InstanceBuffer[InInstanceId];
    
    vec3 CameraPos = SceneBuffer.CameraPos;
    
    vec4 TexelColor = texture(ColorTexture, InUv);
    vec3 SurfacePos = InWorldPos;
    vec3 SurfaceNormal = normalize(InWorldNormal);
    vec3 SurfaceColor = Entry.Color.rgb; //TexelColor.rgb;
    vec3 View = normalize(CameraPos - SurfacePos);
    vec3 Color = vec3(0);

#if 0
    // NOTE: Calculate lighting for point lights
    for (int i = 0; i < SceneBuffer.NumPointLights; ++i)
    {
        point_light CurrLight = PointLights[i];
        vec3 LightDir = normalize(CurrLight.Pos - SurfacePos);
        Color += BlinnPhongLighting(View, SurfaceColor, SurfaceNormal, 32, LightDir, PointLightAttenuate(SurfacePos, CurrLight));
    }
#endif
    
    // NOTE: Calculate lighting for directional lights
    {
        Color += ToonBlinnPhongLighting(View, SurfaceColor, SurfaceNormal, Entry.SpecularPower, Entry.RimBound, Entry.RimThreshold,
                                        DirectionalLight.Dir, DirectionalLight.Color);
        Color += DirectionalLight.AmbientLight * SurfaceColor;
    }

    OutColor = vec4(Color, 1);
}

#endif

#if WATER_VERTEX

layout(location = 0) in vec3 InPos;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec2 InUv;

layout(location = 0) out vec3 OutViewPos;
layout(location = 1) out vec3 OutViewNormal;
layout(location = 2) out vec2 OutUv;
layout(location = 3) out flat uint OutInstanceId;

void main()
{
    water_entry Entry = WaterBuffer[gl_InstanceIndex];
    
    gl_Position = Entry.WVPTransform * vec4(InPos, 1);
    OutViewPos = (Entry.WVTransform * vec4(InPos, 1)).xyz;
    OutViewNormal = (Entry.WVTransform * vec4(InNormal, 0)).xyz;
    OutUv = InUv;
    OutInstanceId = gl_InstanceIndex;
}

#endif

#if WATER_FRAGMENT

layout(location = 0) in vec3 InViewPos;
layout(location = 1) in vec3 InViewNormal;
layout(location = 2) in vec2 InUv;
layout(location = 3) in flat uint InInstanceId;

layout(location = 0) out vec4 OutColor;

float LinearizeDepth(float Depth, float NearZ, float FarZ)
{
    return NearZ * FarZ / (NearZ + Depth * (FarZ - NearZ));
}

void main()
{
    // NOTE: Shader inputs
    vec4 ShallowColor = vec4(0.325, 0.807, 0.971, 0.725);
    vec4 DeepColor = vec4(0.086, 0.407, 1, 0.9);
    float MaxDepth = 1;
    float WaterNoiseCutOff = 0.777;
    float FoamDistance = 0.4;
    vec2 NoiseScrollAmount = vec2(0.003);
    float SurfaceDistortAmount = 0.27;
    float SurfaceAA = 0.005;

    // TODO: Make these inputs
    float NearZ = 0.01f;
    float FarZ = 1000.0f;

    ivec2 PixelCoords = ivec2(gl_FragCoord.xy);
    vec4 WaterColor = vec4(0);
    
    // NOTE: Calculate base water color
    float DepthValue = texelFetch(DepthBuffer, PixelCoords, 0).x;
    DepthValue = LinearizeDepth(DepthValue, NearZ, FarZ);
    float DepthDifference = DepthValue - InViewPos.z;
    WaterColor += mix(ShallowColor, DeepColor, clamp(DepthDifference / MaxDepth, 0, 1));

    // NOTE: Calculate waves
    {
        // NOTE: Shoreline effect, change the surface noise to be smaller when we are in shallow water
        float FoamDepthDifference = clamp(DepthDifference / FoamDistance, 0, 1);
        float SurfaceNoiseCutOff = FoamDepthDifference * WaterNoiseCutOff;

        // NOTE: Distorted water effect
        vec2 Distortion = SurfaceDistortAmount * (2 * texture(Distortion, InUv).xy - 1);
        
        vec2 ModifiedUv = InUv + WaterInputs.Time * NoiseScrollAmount + Distortion; 
        float NoiseSample = texture(PerlinNoise, ModifiedUv).x;
        float SurfaceNoise = smoothstep(SurfaceNoiseCutOff - SurfaceAA, SurfaceNoiseCutOff + SurfaceAA, NoiseSample);
        WaterColor += SurfaceNoise;
    }
    
    OutColor = WaterColor;
}

#endif

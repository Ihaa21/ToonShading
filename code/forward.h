#pragma once

struct forward_state
{
    vk_linear_arena RenderTargetArena;

    VkImage ColorImage;
    render_target_entry ColorEntry;
    VkImage DepthImage;
    render_target_entry DepthEntry;
    render_target ForwardRenderTarget;

    vk_pipeline* ForwardPipeline;

    VkDescriptorSetLayout WaterDescLayout;
    VkDescriptorSet WaterDescriptor;
    vk_image PerlinNoise;
    vk_image Distortion;
    VkSampler NoiseSampler;
    VkBuffer WaterInputsBuffer;
    vk_pipeline* WaterPipeline;
};

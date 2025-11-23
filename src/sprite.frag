#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable


struct CameraData {
    mat4 view;
    mat4 proj;
    float time;
};

struct Transform {
    vec3 pos;
    vec2 extent;
    float rot;
};

struct SpriteData {
    uint columns;
    uint rows;
    uint index;
};

layout (set = 0, binding = 1) readonly buffer StorageBuffer { uint dummy[]; } storage_buffers[];
layout (set = 0, binding = 2) uniform texture2D textures[];
layout (set = 0, binding = 3) uniform sampler samplers[];
layout (set = 1, binding = 0) uniform UBO { CameraData data; } camera_buffer[];

layout (location = 0) in vec2 in_tex_coords;

layout (location = 0) out vec4 out_color;

layout (push_constant) uniform PushConstants {
    uint camera_buffer_index;
    uint sampler_index;
    uint texture_index;

    uint dummy;

    Transform transform;
    SpriteData sprite_data;

    float cover;
};

void main() {
    CameraData data = camera_buffer[camera_buffer_index].data;

    vec4 image_color = texture(sampler2D(textures[texture_index], samplers[sampler_index]), in_tex_coords);
    image_color.xyz = mix(image_color.xyz, vec3(0.0), cover);
   
    out_color = vec4(image_color.xyz, image_color.a);
}
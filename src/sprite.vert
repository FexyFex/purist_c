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

layout (set = 1, binding = 0) uniform UBO { CameraData data; } camera_buffer[];

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_tex_coords;

layout (push_constant) uniform PushConstants {
    uint camera_buffer_index;
    uint sampler_index;
    uint texture_index;

    uint dummy;

    Transform transform;
    SpriteData sprite_data;

    float cover;
};

layout (location = 0) out vec2 out_tex_coords;

vec2 rotate_vert(vec2 point, float angle, vec2 pivot) {
    float r_x = pivot.x + (point.x - pivot.x) * cos(angle) - (point.y - pivot.y) * sin(angle);
    float r_y = pivot.y + (point.x - pivot.x) * sin(angle) + (point.y - pivot.y) * cos(angle);

    return vec2(r_x, r_y);
}

void main() {
    CameraData data = camera_buffer[camera_buffer_index].data;

    vec3 pos_ts = vec3((in_position.xy * transform.extent) + transform.pos.xy, transform.pos.z); // translated + scaled
    vec3 pos_tsr = vec3(rotate_vert(pos_ts.xy, transform.rot, transform.pos.xy), pos_ts.z); // translated + scaled + rotated

    gl_Position = data.proj * data.view * vec4(pos_tsr, 1.0);

    vec2 uv_extent = vec2(1.0 / float(sprite_data.columns), 1.0 / float(sprite_data.rows));
    float uv_pos_index_x = float(sprite_data.index % sprite_data.columns) * uv_extent.x;
    float uv_pos_index_y = floor(float(sprite_data.index) / sprite_data.columns) * uv_extent.y;
    vec2 uv = in_tex_coords * uv_extent + vec2(uv_pos_index_x, uv_pos_index_y);

    out_tex_coords = uv;
}
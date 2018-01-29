#ifdef GL_ES
uniform sampler2D skinning_tex;
#else
uniform samplerBuffer skinning_tex;
#endif

layout(location = 0) in vec3 i_position;
#if defined(Converts_10bit_Vector)
layout(location = 1) in vec4 i_normal_orig;
#else
layout(location = 1) in vec4 i_normal;
#endif

layout(location = 2) in vec4 i_color;
layout(location = 3) in vec2 i_uv;
layout(location = 4) in vec2 i_uv_two;

#if defined(Converts_10bit_Vector)
layout(location = 5) in vec4 i_tangent_orig;
#else
layout(location = 5) in vec4 i_tangent;
#endif

layout(location = 6) in ivec4 i_joint;
layout(location = 7) in vec4 i_weight;
layout(location = 8) in vec3 i_origin;

#if defined(Converts_10bit_Vector)
layout(location = 9) in vec4 i_rotation_orig;
#else
layout(location = 9) in vec4 i_rotation;
#endif

layout(location = 10) in vec4 i_scale;
layout(location = 11) in vec2 i_texture_trans;
layout(location = 12) in ivec2 i_misc_data;

#stk_include "utils/get_world_location.vert"

out vec3 tangent;
out vec3 bitangent;
out vec3 normal;
out vec2 uv;
out vec2 uv_two;
out vec4 color;
out float camdist;
flat out float hue_change;

void main()
{

#if defined(Converts_10bit_Vector)
    vec4 i_normal = convert10BitVector(i_normal_orig);
    vec4 i_tangent = convert10BitVector(i_tangent_orig);
    vec4 i_rotation = convert10BitVector(i_rotation_orig);
#endif

    vec4 idle_position = vec4(i_position, 1.0);
    vec4 idle_normal = vec4(i_normal.xyz, 0.0);
    vec4 idle_tangent = vec4(i_tangent.xyz, 0.0);
    vec4 skinned_position = vec4(0.0);
    vec4 skinned_normal = vec4(0.0);
    vec4 skinned_tangent = vec4(0.0);
    int skinning_offset = i_misc_data.x;

    for (int i = 0; i < 4; i++)
    {
#ifdef GL_ES
        mat4 joint_matrix = mat4(
            texelFetch(skinning_tex, ivec2
                (0 , clamp(i_joint[i] + skinning_offset, 0, MAX_BONES)), 0),
            texelFetch(skinning_tex, ivec2
                (1, clamp(i_joint[i] + skinning_offset, 0, MAX_BONES)), 0),
            texelFetch(skinning_tex, ivec2
                (2, clamp(i_joint[i] + skinning_offset, 0, MAX_BONES)), 0),
            texelFetch(skinning_tex, ivec2
                (3, clamp(i_joint[i] + skinning_offset, 0, MAX_BONES)), 0));
#else
        mat4 joint_matrix = mat4(
            texelFetch(skinning_tex,
                clamp(i_joint[i] + skinning_offset, 0, MAX_BONES) * 4),
            texelFetch(skinning_tex,
                clamp(i_joint[i] + skinning_offset, 0, MAX_BONES) * 4 + 1),
            texelFetch(skinning_tex,
                clamp(i_joint[i] + skinning_offset, 0, MAX_BONES) * 4 + 2),
            texelFetch(skinning_tex,
                clamp(i_joint[i] + skinning_offset, 0, MAX_BONES) * 4 + 3));
#endif
        skinned_position += i_weight[i] * joint_matrix * idle_position;
        skinned_normal += i_weight[i] * joint_matrix * idle_normal;
        skinned_tangent += i_weight[i] * joint_matrix * idle_tangent;
    }

    vec4 quaternion = normalize(vec4(i_rotation.xyz, i_scale.w));
    vec4 world_position = getWorldPosition(i_origin, quaternion, i_scale.xyz,
        skinned_position.xyz);
    vec3 world_normal = rotateVector(quaternion, skinned_normal.xyz);
    vec3 world_tangent = rotateVector(quaternion, skinned_tangent.xyz);

    tangent = (u_view_matrix * vec4(world_tangent, 0.0)).xyz;
    bitangent = (u_view_matrix *
       // bitangent sign
      vec4(cross(world_normal, world_tangent) * i_tangent.w, 0.0)
      ).xyz;
    normal = (u_view_matrix * vec4(world_normal, 0.0)).xyz;

    uv = vec2(i_uv.x + (i_texture_trans.x * i_normal.w),
        i_uv.y + (i_texture_trans.y * i_normal.w));
    uv_two = i_uv_two;

    color = i_color.zyxw;
    camdist = length(u_view_matrix * world_position);
    hue_change = float(i_misc_data.y) * 0.01;
    gl_Position = u_projection_view_matrix * world_position;
}

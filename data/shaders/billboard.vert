uniform mat4 color_matrix;
uniform vec3 Position;
uniform vec2 Size;

#ifdef Explicit_Attrib_Location_Usable
layout(location = 0) in vec2 Corner;
layout(location = 3) in vec2 Texcoord;
#else
in vec2 Corner;
in vec2 Texcoord;
#endif

out vec2 uv;
out vec4 vertex_color;

void main(void)
{
    uv = Texcoord;
    vec4 Center = ViewMatrix * vec4(Position, 1.);
    gl_Position = ProjectionMatrix * (Center + vec4(Size * Corner, 0., 0.));
    vertex_color = color_matrix[gl_VertexID];
}

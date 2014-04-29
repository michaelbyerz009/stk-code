uniform sampler2D ntex;
uniform sampler2D dtex;
uniform float spec;
uniform mat4 invproj;
uniform mat4 ViewMatrix;
uniform vec2 screen;

flat in vec3 center;
flat in float energy;
flat in vec3 col;

out vec4 Diffuse;
out vec4 Specular;

vec3 DecodeNormal(vec2 n);
vec3 getSpecular(vec3 normal, vec3 eyedir, vec3 lightdir, vec3 color, float roughness);

void main()
{
    vec2 texc = gl_FragCoord.xy / screen;
    float z = texture(dtex, texc).x;
    vec3 norm = normalize(DecodeNormal(2. * texture(ntex, texc).xy - 1.));
    float roughness = texture(ntex, texc).z;

    vec4 xpos = 2.0 * vec4(texc, z, 1.0) - 1.0f;
    xpos = invproj * xpos;
    xpos /= xpos.w;
    vec3 eyedir = -normalize(xpos.xyz);

    vec4 pseudocenter = ViewMatrix * vec4(center.xyz, 1.0);
    pseudocenter /= pseudocenter.w;
    vec3 light_pos = pseudocenter.xyz;
    vec3 light_col = col.xyz;
    float d = distance(light_pos, xpos.xyz);
    float att = energy * 200. / (1. + 33. * d + 33. * d * d);

    // Light Direction
    vec3 L = -normalize(xpos.xyz - light_pos);

    float NdotL = max(0., dot(norm, L));

    Diffuse = vec4(NdotL * light_col * att, 1.);
    Specular = vec4(getSpecular(norm, eyedir, L, light_col, roughness) * NdotL * att, 1.);
}

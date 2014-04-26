// From http://www.ceng.metu.edu.tr/~akyuz/files/hdrgpu.pdf

uniform sampler2D tex;
uniform sampler2D logluminancetex;
uniform float exposure = .09;
uniform float Lwhite = 1.;

in vec2 uv;
out vec4 FragColor;

vec3 getCIEYxy(vec3 rgbColor);
vec3 getRGBFromCIEXxy(vec3 YxyColor);


float delta = .0001;
float saturation = 1.;

void main()
{
    vec4 col = texture(tex, uv);
    float avgLw = textureLod(logluminancetex, uv, 10.).x;
    avgLw = max(exp(avgLw) - delta, delta);

    vec3 Cw = getCIEYxy(col.xyz);
    float Lw = Cw.y;

    /* Reinhard, for reference */
//    float L = Lw * exposure / avgLw;
//    float Ld = L * (1. + L / (Lwhite * Lwhite));
//    Ld /= (1. + L);
//    FragColor = vec4(Ld * pow(col.xyz / Lw, vec3(saturation)), 1.);

    // Uncharted2 tonemap with Auria's custom coefficients
    vec4 perChannel = (col * (6.9 * col + .5)) / (col * (5.2 * col + 1.7) + 0.06);
    perChannel = pow(perChannel, vec4(2.2));
    FragColor = vec4(perChannel.xyz, 1.);

}

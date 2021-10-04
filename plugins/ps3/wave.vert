#version 400
uniform float x_inc;
uniform float z_inc;
uniform float time;
uniform float light_a;
uniform float chaos;
uniform float detail;
uniform float speed;
uniform float scale;
uniform float sec_chaos;
uniform float sec_detail;
uniform float sec_speed;
uniform float sec_scale;
uniform vec3 wave_color;

layout (location = 0) in vec3 a_position;
//layout (location = 1) in vec3 a_normal;

// perlin noise code courtesy of @madweedfall of shadertoy
// https://www.shadertoy.com/user/madweedfall
// https://www.shadertoy.com/view/4lB3zz
float noise(int x,int y) {   
    float fx = float(x);
    float fy = float(y);
    
    return 2.0 * fract(sin(dot(vec2(fx, fy), vec2(12.9898, 78.233))) * 43758.5453) - 1.0;
}

float smoothNoise(int x, int y) {
    return noise(x, y) / 4.0 + (
        noise(x + 1, y) + noise(x - 1, y) + noise(x, y + 1) + noise(x, y - 1)
    ) / 8.0 + (
        noise(x + 1, y + 1) + noise(x + 1, y - 1) + noise(x - 1, y + 1) + noise(x - 1, y - 1)
    ) / 16.0;
}

float COSInterpolation(float x, float y, float n) {
    float r = n * 3.1415926;
    float f = (1.0 - cos(r)) * 0.5;
    return x * (1.0 - f) + y * f;
}

float InterpolationNoise(float x, float y) {
    int ix = int(x);
    int iy = int(y);
    float fracx = x - float(int(x));
    float fracy = y - float(int(y));
    
    float v1 = smoothNoise(ix, iy);
    float v2 = smoothNoise(ix + 1, iy);
    float v3 = smoothNoise(ix, iy + 1);
    float v4 = smoothNoise(ix + 1, iy + 1);
    
   	float i1 = COSInterpolation(v1, v2, fracx);
    float i2 = COSInterpolation(v3, v4, fracx);
    
    return COSInterpolation(i1, i2, fracy);
}

float PerlinNoise2D(float x, float y, float persistence, int o, int ob) {
    float sum = 0.0;
    float frequency = 0.0;
    float amplitude = 0.0;
    for (int i = ob; i < o + ob;i++) {
        frequency = pow(2.0, float(i));
        amplitude = pow(persistence, float(i));
        sum = sum + InterpolationNoise(x * frequency, y * frequency) * amplitude;
    }
    
    return sum;
}

vec3 pos(float x, float z) {
    float y = PerlinNoise2D(
        (x * detail) + (time * speed),
        (z * detail) + (time * speed),
        chaos,
        3, 4
    );
    y *= scale;

    float b = PerlinNoise2D(
        (x * sec_detail) + (time * sec_speed),
        (z * sec_detail) + (time * sec_speed),
        sec_chaos,
        3, 4
    );
    b *= sec_scale;
    return vec3(x, b + y, z);
}

out vec4 v_color;

void main() {
    vec3 p0 = pos(a_position.x        , a_position.z        );
    vec3 p1 = pos(a_position.x + x_inc, a_position.z        );
    vec3 p2 = pos(a_position.x        , a_position.z + z_inc);

    vec3 r = p1 - p0;
    vec3 f = p2 - p0;
    vec3 n = normalize(cross(f, r));

    vec3 look = vec3(0, 0, 1);
    float gfac = dot(n, look);
    gfac *= gfac;
    if (gfac < 0.3) gfac = 1.0 - (gfac / 0.3);
    else gfac = 0.0;
    gfac = min(gfac, 0.7);

    float alpha = 1.0;
    float amult = (abs(p0.z) * 2.0);
    alpha = 1.0 - smoothstep(0.3, 1.0, amult);

    vec3 l = vec3(0, sin(light_a), cos(light_a));
    float lfac = clamp(dot(n, l), 0.1, 1.0);

    vec3 diffuse = lfac * wave_color;
    vec3 highlight = gfac * vec3(1.0, 1.0, 1.0);

	v_color = vec4(diffuse + highlight, min(alpha, 0.3));
    gl_Position = vec4(p0, 1.0);
}
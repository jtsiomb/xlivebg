uniform sampler2D tex;
uniform vec2 dir;
uniform vec2 delta;
uniform float intensity;

float lookup(in vec2 uv)
{
	return texture2D(tex, uv).x * 2.0 - 1.0;
}

void main()
{
	vec2 uv = gl_TexCoord[0].st;
	float tsum = lookup(uv) +
		lookup(uv + dir * delta) +
		lookup(uv - dir * delta) +
		lookup(uv + dir * delta * 2.0) +
		lookup(uv - dir * delta * 2.0) +
		lookup(uv + dir * delta * 3.0) +
		lookup(uv - dir * delta * 3.0);

	float val = (tsum / 7.0 * intensity) * 0.5 + 0.5;

	gl_FragColor = vec4(val, val, val, 1.0);
}

uniform sampler2D tex, dest;
uniform vec2 dir;
uniform vec2 delta;
uniform float intensity;

float lookup(in vec2 uv)
{
	return texture2D(tex, uv).x * 2.0 - 1.0;
}

vec4 outcol(float val)
{
	float v = val * 0.5 + 0.5;
	return vec4(v, v, v, 1.0);
}	

void main()
{
	vec2 uv = gl_TexCoord[0].st;
	//float val = texture2D(tex, uv).x;
	/*
	float tsum = lookup(uv + dir * delta) +
		lookup(uv - dir * delta) +
		lookup(uv + dir * delta * 2.0) +
		lookup(uv - dir * delta * 2.0) +
		lookup(uv + dir * delta * 3.0) +
		lookup(uv - dir * delta * 3.0);

	float val = tsum / 6.0 * intensity * 0.5 + 0.5;
	*/

	float tsum = lookup(uv + vec2(delta.x, 0.0)) +
		lookup(uv - vec2(delta.x, 0.0)) +
		lookup(uv + vec2(0.0, delta.y)) +
		lookup(uv - vec2(0.0, delta.y));
	float val = (tsum / 2.0 - (texture2D(dest, uv).x * 2.0 - 1.0)) * 0.95;

	gl_FragColor = outcol(val);//clamp(val, -1.0, 1.0));
}

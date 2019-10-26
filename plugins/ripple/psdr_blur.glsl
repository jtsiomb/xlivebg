uniform sampler2D tex;
uniform vec2 dir;
uniform vec2 delta;
uniform float intensity;

void main()
{
	vec2 uv = gl_TexCoord[0].st;
	float texel = (texture2D(tex, uv).x +
		texture2D(tex, uv + dir * delta).x +
		texture2D(tex, uv - dir * delta).x) * intensity / 3.0;

	gl_FragColor = vec4(texel, texel, texel, 1.0);
}

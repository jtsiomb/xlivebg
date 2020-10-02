uniform sampler2D tex_img, tex_mask;
uniform sampler2D tex_ripple;

void main()
{
	vec2 uv = gl_TexCoord[0].st;
	float mask = texture2D(tex_mask, uv).x;
	float rheight = texture2D(tex_ripple, gl_TexCoord[1].st).x * 2.0 - 1.0;

	vec2 offs = vec2(dFdx(rheight), dFdy(rheight)) * 0.3 * mask;
	uv += offs;

	vec3 texel = texture2D(tex_img, uv).rgb;
	gl_FragColor.rgb = texel;
	gl_FragColor.a = step(0.0, min(uv.x, uv.y)) * (1.0 - step(1.0, max(uv.x, uv.y)));
}

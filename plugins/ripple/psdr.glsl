uniform sampler2D tex_img, tex_mask;
uniform sampler2D tex_ripple;

void main()
{
	vec2 uv = gl_TexCoord[0].st;
	float mask = texture2D(tex_mask, uv).x;
	float rheight = texture2D(tex_ripple, gl_TexCoord[1].st).x * 2.0 - 1.0;

	vec2 offs = vec2(dFdx(rheight), dFdy(rheight)) * 0.4 * mask;

	/*vec3 foo = vec3(clamp(rheight.x, 0.0, 1.0), 0.0,
			clamp(-rheight.x, 0.0, 1.0));*/

	vec3 texel = texture2D(tex_img, uv + offs).rgb;
	gl_FragColor.rgb = texel;

	//gl_FragColor.rgb = foo + texel * 0.0001;
	gl_FragColor.a = 1.0;
}

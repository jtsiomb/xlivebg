uniform sampler2D tex_img;
uniform sampler2D tex_ripple;

void main()
{
	vec2 uv = gl_TexCoord[0].st;
	vec3 rheight = texture2D(tex_ripple, gl_TexCoord[1].st).xxx;
	vec3 texel = texture2D(tex_img, uv).rgb;

	vec3 foo = vec3(clamp(rheight.x, 0.0, 1.0), 0, clamp(-rheight.x, 0.0, 1.0));

	gl_FragColor.rgb = foo + texel * 0.0001;//vec3(texel.x, rheight.x, 0.0);
	gl_FragColor.a = 1.0;
}

uniform sampler2D tex_img;
uniform sampler2D tex_ripple;

void main()
{
	vec2 uv = gl_TexCoord[0].st;
	vec3 rheight = texture2D(tex_ripple, gl_TexCoord[1].st).xxx;
	vec3 texel = texture2D(tex_img, uv).rgb;

	gl_FragColor.rgb = rheight;//mix(texel, rheight, smoothstep(0.48, 0.52, uv.x));
	gl_FragColor.a = 1.0;
}
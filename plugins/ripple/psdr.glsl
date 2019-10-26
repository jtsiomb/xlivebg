uniform sampler2D tex_img;
uniform sampler2D tex_ripple;

void main()
{
	vec2 uv = gl_TexCoord[0].st;
	vec3 rheight = texture2D(tex_ripple, uv).xxx;
	vec3 texel = texture2D(tex_img, uv).rgb;

	gl_FragColor.rgb = mix(texel, rheight, smoothstep(0.48, 0.52, uv.x));
	gl_FragColor.a = 1.0;
}

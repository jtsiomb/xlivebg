void main()
{
	gl_Position = gl_Vertex;
	gl_TexCoord[0] = gl_TextureMatrix[0] * (gl_MultiTexCoord0 * 2.0 - 1.0) * 0.5 + 0.5;
	gl_TexCoord[1] = gl_MultiTexCoord1;
}

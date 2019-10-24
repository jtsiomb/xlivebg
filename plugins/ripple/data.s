	.global ripple_vsdr
ripple_vsdr:
	.incbin "vsdr.glsl"
	.byte 0

	.global ripple_blur_psdr
ripple_blur_psdr:
	.incbin "psdr_blur.glsl"
	.byte 0

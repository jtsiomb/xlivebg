	.global ripple_vsdr
ripple_vsdr:
	.incbin "vsdr.glsl"
	.byte 0

	.global ripple_psdr
ripple_psdr:
	.incbin "psdr.glsl"
	.byte 0

	.global ripple_waves_vsdr
ripple_waves_vsdr:
	.incbin "waves.v.glsl"
	.byte 0

	.global ripple_waves_psdr
ripple_waves_psdr:
	.incbin "waves.p.glsl"
	.byte 0

	.global wave_v
wave_v:
	.incbin "wave.vert"
	.byte 0

	.global wave_f
wave_f:
	.incbin "wave.frag"
	.byte 0

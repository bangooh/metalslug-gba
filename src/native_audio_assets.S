.section .rodata.native_audio,"a",%progbits
.balign 4
.global native_audio_data
native_audio_data:
.incbin "generated/native-audio.ima"
.section .rodata.native_audio_pcm,"a",%progbits
.balign 4
.global native_audio_pcm
native_audio_pcm:
.incbin "generated/native-audio-pcm.s8"

.section .rodata.native_audio_runtime,"a",%progbits
.balign 4
.global native_audio_runtime_data
native_audio_runtime_data:
.incbin "generated/native-audio-runtime.ima"

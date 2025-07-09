from pydub import AudioSegment

audio = AudioSegment.from_file("a.aac", format="aac")

# Chuyển sampling rate thành 16000 Hz
audio = audio.set_frame_rate(16000)

audio.export("a.wav", format="wav")

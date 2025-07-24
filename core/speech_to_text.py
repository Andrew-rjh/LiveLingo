
import openai
import numpy as np
import io
import wave
from scipy.io.wavfile import write

class SpeechToText:
    def __init__(self, api_key):
        openai.api_key = api_key
        self.audio_buffer = []
        self.sample_rate = 16000 # Must match the capturer's sample rate

    def process_audio_chunk(self, chunk):
        """Receives an audio chunk and adds it to the buffer."""
        self.audio_buffer.append(chunk)

    def transcribe_buffer(self):
        """Transcribes the accumulated audio buffer and clears it."""
        if not self.audio_buffer:
            return None

        # Concatenate all chunks in the buffer
        full_audio = np.concatenate(self.audio_buffer, axis=0)
        self.audio_buffer.clear()

        # Convert to 16-bit PCM
        p_audio = (full_audio * 32767).astype(np.int16)

        # Create an in-memory WAV file
        wav_io = io.BytesIO()
        write(wav_io, self.sample_rate, p_audio)
        wav_io.seek(0)
        wav_io.name = "audio.wav"

        try:
            # Call Whisper API
            transcript = openai.Audio.transcriptions.create(
                model="whisper-1",
                file=wav_io
            )
            return transcript.text
        except Exception as e:
            print(f"Error during transcription: {e}")
            return None

# Example usage (conceptual):
if __name__ == '__main__':
    # This part is for demonstration and won't run directly
    # as it requires a valid OpenAI API key and audio data.
    
    # 1. Initialize with your API key
    # stt = SpeechToText(api_key="YOUR_OPENAI_API_KEY")
    
    # 2. In a real scenario, the audio capturer would call this method
    # dummy_chunk = np.random.randn(1024, 1) # Simulate an audio chunk
    # stt.process_audio_chunk(dummy_chunk)
    
    # 3. Periodically, you would call transcribe_buffer
    # text = stt.transcribe_buffer()
    # if text:
    #     print(f"Transcription: {text}")
    pass

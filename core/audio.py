"""Audio capture module."""

from typing import Optional

import numpy as np
import sounddevice as sd


class AudioSource:
    """Base class for audio sources."""

    def __init__(self, use_microphone: bool = True, use_system_audio: bool = False,
                 samplerate: int = 16_000, channels: int = 1, frames_per_buffer: int = 1024):
        self.use_microphone = use_microphone
        self.use_system_audio = use_system_audio
        self.sample_rate = samplerate
        self.channels = channels
        self.frames_per_buffer = frames_per_buffer
        self.stream: Optional[sd.InputStream] = None
        self.sample_width = 2  # 16-bit samples

    def start(self):
        """Start capturing audio using sounddevice."""
        if self.stream:
            return
        self.stream = sd.InputStream(
            samplerate=self.sample_rate,
            channels=self.channels,
            blocksize=self.frames_per_buffer,
            dtype="int16",
        )
        self.stream.start()

    def read(self) -> Optional[bytes]:
        """Read audio data from the stream."""
        if not self.stream:
            return None
        data, _ = self.stream.read(self.frames_per_buffer)
        return data.tobytes()

    def stop(self):
        """Stop capturing audio."""
        if not self.stream:
            return
        self.stream.stop()
        self.stream.close()
        self.stream = None

"""Audio capture module."""

from typing import Optional


class AudioSource:
    """Base class for audio sources."""

    def __init__(self, use_microphone: bool = True, use_system_audio: bool = False):
        self.use_microphone = use_microphone
        self.use_system_audio = use_system_audio
        self.stream = None

    def start(self):
        """Start capturing audio. Placeholder for actual implementation."""
        # TODO: Implement audio capture using pyaudio or sounddevice.
        pass

    def read(self) -> Optional[bytes]:
        """Read audio data from the stream."""
        # TODO: Return actual audio bytes
        return None

    def stop(self):
        """Stop capturing audio."""
        # TODO: Close audio stream
        pass

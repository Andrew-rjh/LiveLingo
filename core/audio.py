"""Audio capture module."""

from typing import Optional

import logging
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
        logging.info("Starting audio capture (system audio=%s)", self.use_system_audio)
        extra = None
        if self.use_system_audio:
            try:
                extra = sd.WasapiSettings(loopback=True)
            except AttributeError:
                logging.warning("System audio capture not supported on this platform")
        self.stream = sd.InputStream(
            samplerate=self.sample_rate,
            channels=self.channels,
            blocksize=self.frames_per_buffer,
            dtype="int16",
            extra_settings=extra,
        )
        self.stream.start()

    def read(self) -> Optional[bytes]:
        """Read audio data from the stream."""
        if not self.stream:
            return None
        data, _ = self.stream.read(self.frames_per_buffer)
        logging.debug("Read %d frames", len(data))
        return data.tobytes()

    def stop(self):
        """Stop capturing audio."""
        if not self.stream:
            return
        logging.info("Stopping audio capture")
        self.stream.stop()
        self.stream.close()
        self.stream = None

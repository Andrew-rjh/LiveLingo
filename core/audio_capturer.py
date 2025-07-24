import pyaudio
import numpy as np
import threading

class AudioCapturer:
    def __init__(self):
        self.p = pyaudio.PyAudio()
        self.stream = None
        self.is_recording = False
        self.callback = None
        self.chunk_size = 1024
        self.sample_rate = 16000
        self.channels = 1

    def get_devices(self):
        """Returns a list of available input devices, including loopback if available."""
        devices = []
        for i in range(self.p.get_device_count()):
            dev_info = self.p.get_device_info_by_index(i)
            if dev_info.get('maxInputChannels') > 0:
                devices.append(dev_info)
        return devices

    def start_capture(self, device_index=None, is_loopback=False, callback=None):
        if self.is_recording:
            print("Already recording.")
            return

        self.callback = callback
        try:
            input_device_index = device_index
            if is_loopback:
                # Find WASAPI loopback device
                wasapi_info = self.p.get_host_api_info_by_type(pyaudio.paWASAPI)
                default_speakers = self.p.get_device_info_by_index(wasapi_info['defaultOutputDevice'])
                if not default_speakers['isLoopbackDevice']:
                    for i in range(self.p.get_device_count()):
                        dev = self.p.get_device_info_by_index(i)
                        if dev['isLoopbackDevice']:
                            input_device_index = i
                            break
                    else:
                        raise RuntimeError("No loopback device found.")
                else:
                    input_device_index = default_speakers['index']

            self.stream = self.p.open(
                format=pyaudio.paInt16,
                channels=self.channels,
                rate=self.sample_rate,
                input=True,
                input_device_index=input_device_index,
                frames_per_buffer=self.chunk_size,
                stream_callback=self._pyaudio_callback
            )
            self.is_recording = True
            self.stream.start_stream()
            print(f"Started recording from device index: {input_device_index}")

        except Exception as e:
            print(f"Error starting capture: {e}")
            self.is_recording = False

    def _pyaudio_callback(self, in_data, frame_count, time_info, status):
        if self.callback:
            # Convert byte data to numpy array
            audio_data = np.frombuffer(in_data, dtype=np.int16)
            # Normalize to float32, which is what the STT expects
            audio_data_float = audio_data.astype(np.float32) / 32768.0
            self.callback(audio_data_float)
        return (in_data, pyaudio.paContinue)

    def stop_capture(self):
        if not self.is_recording or not self.stream:
            print("Not recording.")
            return
        
        self.stream.stop_stream()
        self.stream.close()
        self.is_recording = False
        self.stream = None
        print("Recording stopped.")

    def __del__(self):
        self.p.terminate()

# Example usage:
if __name__ == '__main__':
    capturer = AudioCapturer()
    print("Available devices:")
    for device in capturer.get_devices():
        print(f"- Index {device['index']}: {device['name']}")

    def my_audio_processor(data):
        print(f"Processing audio data chunk of shape: {data.shape}")

    # Example: Start capturing from default microphone
    capturer.start_capture(callback=my_audio_processor)
    
    try:
        input("Recording... Press Enter to stop.\n")
    finally:
        capturer.stop_capture()
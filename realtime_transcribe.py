import sounddevice as sd
import numpy as np
import whisper
import torch
import queue
import threading
import sys

SAMPLE_RATE = 16000
CHUNK_DURATION = 5  # seconds
LANGUAGE = "ko"
MODEL_SIZE = "base"

class AudioStream:
    def __init__(self, device, loopback=False):
        self.q = queue.Queue()
        self.stream = sd.InputStream(
            device=device,
            samplerate=SAMPLE_RATE,
            channels=1,
            dtype='float32',
            blocksize=int(SAMPLE_RATE * 0.5),
            callback=self.callback,
            latency='low',
            extra_settings=sd.WasapiSettings(loopback=loopback) if sys.platform.startswith('win') else None
        )

    def callback(self, indata, frames, time, status):
        if status:
            print(status, file=sys.stderr)
        self.q.put(indata.copy())

    def start(self):
        self.stream.start()

    def stop(self):
        self.stream.stop()
        self.stream.close()


def transcribe_loop(queues):
    model = whisper.load_model(MODEL_SIZE)
    buffer = np.empty((0, 1), dtype=np.float32)
    chunk_samples = int(CHUNK_DURATION * SAMPLE_RATE)
    while True:
        for q in queues:
            try:
                data = q.get(timeout=0.1)
                buffer = np.concatenate((buffer, data))
            except queue.Empty:
                pass
        if len(buffer) >= chunk_samples:
            chunk = buffer[:chunk_samples]
            buffer = buffer[chunk_samples:]
            audio = whisper.pad_or_trim(chunk.flatten())
            mel = whisper.log_mel_spectrogram(audio, model.device)
            options = dict(language=LANGUAGE, without_timestamps=True)
            result = model.decode(mel, options)
            text = result.text.strip()
            if text:
                print(text)


def run():
    try:
        devices = sd.query_devices()
        default_input = sd.default.device[0]
        default_output = sd.default.device[1]
        mic_stream = AudioStream(default_input)
        sys_stream = AudioStream(default_output, loopback=True)
        mic_stream.start()
        sys_stream.start()

        t = threading.Thread(target=transcribe_loop, args=([mic_stream.q, sys_stream.q],))
        t.daemon = True
        t.start()
        print("Recording... Press Ctrl+C to stop.")
        while True:
            t.join(0.5)
    except KeyboardInterrupt:
        print("Stopping...")
    finally:
        mic_stream.stop()
        sys_stream.stop()

def main():
    run()


if __name__ == "__main__":
    main()

"""Main application entry point."""

import sys
import os
from typing import Optional

from PyQt5 import QtCore, QtWidgets
import speech_recognition as sr

from core.audio import AudioSource
from core.llm import LLMClient
from ui.main_window import MainWindow
from ui.overlay import Overlay


class Transcriber(QtCore.QThread):
    """Background thread to transcribe audio and update the overlay."""

    text_ready = QtCore.pyqtSignal(str)

    def __init__(self, audio: AudioSource, llm: LLMClient):
        super().__init__()
        self.audio = audio
        self.llm = llm
        self.recognizer = sr.Recognizer()
        self.running = True
        self.buffer = b""

    def run(self):
        self.audio.start()
        while self.running:
            data = self.audio.read()
            if data:
                self.buffer += data
            frame_size = self.audio.sample_rate * self.audio.sample_width * 3
            if len(self.buffer) >= frame_size:
                audio_data = sr.AudioData(self.buffer, self.audio.sample_rate, self.audio.sample_width)
                self.buffer = b""
                try:
                    text = self.recognizer.recognize_google(audio_data)
                except sr.UnknownValueError:
                    continue
                translation = self.llm.translate(text)
                self.text_ready.emit(translation)

    def stop(self):
        self.running = False
        self.audio.stop()


def main():
    app = QtWidgets.QApplication(sys.argv)

    main_window = MainWindow()
    overlay = Overlay()
    overlay.show()

    api_key = os.getenv("OPENAI_API_KEY", "")
    audio_source = AudioSource()
    llm = LLMClient(api_key)
    transcriber = Transcriber(audio_source, llm)
    transcriber.text_ready.connect(overlay.update_text)
    transcriber.start()

    main_window.show()

    exit_code = app.exec_()
    transcriber.stop()
    transcriber.wait()
    sys.exit(exit_code)


if __name__ == "__main__":
    main()

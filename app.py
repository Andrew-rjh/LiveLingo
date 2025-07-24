import sys
from PySide6.QtWidgets import QApplication
from PySide6.QtCore import QTimer
from ui.main_window import MainWindow
from ui.overlay_window import OverlayWindow
from core.speech_to_text import SpeechToText
from core.llm_handler import LLMHandler
import config

class LiveLingoApp:
    def __init__(self):
        if not config.OPENAI_API_KEY:
            raise ValueError("OpenAI API key is not set in config.py")

        self.app = QApplication(sys.argv)
        self.main_window = MainWindow()
        self.overlay_window = OverlayWindow()
        self.stt = SpeechToText(api_key=config.OPENAI_API_KEY)
        self.llm_handler = LLMHandler(api_key=config.OPENAI_API_KEY)

        self.transcription_timer = QTimer()
        self.transcription_timer.timeout.connect(self.process_audio)

        self.main_window.capture_button.clicked.connect(self.handle_capture_toggle)

    def handle_capture_toggle(self):
        if not self.main_window.audio_capturer.is_recording:
            self.start_processing()
        else:
            self.stop_processing()

    def start_processing(self):
        self.main_window.audio_capturer.callback = self.stt.process_audio_chunk
        self.transcription_timer.start(5000) # Process every 5 seconds
        print("Audio processing started.")

    def stop_processing(self):
        self.transcription_timer.stop()
        self.process_audio() # Process any remaining audio
        print("Audio processing stopped.")

    def process_audio(self):
        original_text = self.stt.transcribe_buffer()
        if not original_text:
            return

        print(f"Transcribed: {original_text}")
        self.overlay_window.label.setText(original_text) # Always show original text

        # LLM processing (translation/summarization) will still happen in the background
        # but its output will not directly update the overlay anymore.
        if self.main_window.translate_radio.isChecked():
            target_lang = self.main_window.lang_combo.currentText()
            print(f"Translating to {target_lang}...")
            processed_text = self.llm_handler.translate(original_text, target_lang)
            if processed_text:
                print(f"LLM Translation: {processed_text}")
        elif self.main_window.summarize_radio.isChecked():
            print("Summarizing topic...")
            processed_text = self.llm_handler.summarize_topic(original_text)
            if processed_text:
                print(f"LLM Summary: {processed_text}")

    def run(self):
        self.main_window.show()
        self.overlay_window.show()
        sys.exit(self.app.exec())

if __name__ == "__main__":
    try:
        livelingo = LiveLingoApp()
        livelingo.run()
    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        # In a real app, you'd show a user-friendly dialog.
        sys.exit(1)
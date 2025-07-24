import sys
from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget,
                             QVBoxLayout, QHBoxLayout, QComboBox, 
                             QPushButton, QCheckBox, QLabel, QRadioButton, QGroupBox)
from core.audio_capturer import AudioCapturer

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("LiveLingo Settings")
        
        self.audio_capturer = AudioCapturer()

        # --- Main Layout ---
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)

        # --- Audio Settings Group ---
        audio_group = QGroupBox("Audio Settings")
        audio_layout = QVBoxLayout(audio_group)
        main_layout.addWidget(audio_group)

        self.device_combo = QComboBox()
        self.populate_devices()
        self.loopback_checkbox = QCheckBox("Capture System Sound")
        self.loopback_checkbox.stateChanged.connect(self.toggle_device_selection)
        
        device_layout = QHBoxLayout()
        device_layout.addWidget(QLabel("Audio Device:"))
        device_layout.addWidget(self.device_combo)
        audio_layout.addLayout(device_layout)
        audio_layout.addWidget(self.loopback_checkbox)

        # --- Function Settings Group ---
        function_group = QGroupBox("Function Settings")
        function_layout = QVBoxLayout(function_group)
        main_layout.addWidget(function_group)

        self.translate_radio = QRadioButton("Translate")
        self.summarize_radio = QRadioButton("Summarize Topic")
        self.translate_radio.setChecked(True)
        self.translate_radio.toggled.connect(self.toggle_language_selection)

        self.lang_combo = QComboBox()
        self.lang_combo.addItems(["Korean", "English", "Japanese", "Chinese", "Spanish", "French"])
        
        function_layout.addWidget(self.translate_radio)
        function_layout.addWidget(self.summarize_radio)
        function_layout.addWidget(self.lang_combo)

        # --- Control Button ---
        self.capture_button = QPushButton("Start Capture")
        self.capture_button.clicked.connect(self.toggle_capture)
        main_layout.addWidget(self.capture_button)

    def populate_devices(self):
        self.device_combo.clear()
        try:
            devices = self.audio_capturer.get_devices()
            for device in devices:
                self.device_combo.addItem(device['name'], userData=device['index'])
        except Exception as e:
            print(f"Could not load audio devices: {e}")
            self.device_combo.addItem("No devices found")
            self.device_combo.setEnabled(False)

    def toggle_device_selection(self, state):
        self.device_combo.setEnabled(not state)

    def toggle_language_selection(self, checked):
        self.lang_combo.setEnabled(checked)

    def toggle_capture(self):
        if self.audio_capturer.is_recording:
            self.audio_capturer.stop_capture()
            self.capture_button.setText("Start Capture")
        else:
            is_loopback = self.loopback_checkbox.isChecked()
            selected_device_index = self.device_combo.currentData()
            
            self.audio_capturer.start_capture(
                device_index=selected_device_index,
                is_loopback=is_loopback
            )
            self.capture_button.setText("Stop Capture")

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())
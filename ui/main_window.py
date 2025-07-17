"""Main configuration window."""

from PyQt5 import QtWidgets


class MainWindow(QtWidgets.QWidget):
    """Main application window for settings."""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("LiveLingo Settings")
        self.setup_ui()

    def setup_ui(self):
        layout = QtWidgets.QVBoxLayout()
        self.microphone_check = QtWidgets.QCheckBox("Use Microphone")
        self.system_audio_check = QtWidgets.QCheckBox("Use System Audio")
        layout.addWidget(self.microphone_check)
        layout.addWidget(self.system_audio_check)
        self.setLayout(layout)

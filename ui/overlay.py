"""Overlay window that stays on top of other applications."""

from PyQt5 import QtCore, QtWidgets


class Overlay(QtWidgets.QWidget):
    """A simple overlay window that displays text."""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("LiveLingo Overlay")
        self.setWindowFlags(self.windowFlags() | QtCore.Qt.WindowStaysOnTopHint)
        self.label = QtWidgets.QLabel("Waiting for input…")
        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(self.label)
        self.setLayout(layout)

    def update_text(self, text: str):
        self.label.setText(text)

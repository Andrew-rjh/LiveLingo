"""Main application entry point."""

import sys

from PyQt5 import QtWidgets

from core.audio import AudioSource
from core.llm import LLMClient
from ui.main_window import MainWindow
from ui.overlay import Overlay


def main():
    app = QtWidgets.QApplication(sys.argv)

    main_window = MainWindow()
    overlay = Overlay()
    overlay.show()

    main_window.show()

    sys.exit(app.exec_())


if __name__ == "__main__":
    main()

# LiveLingo

A simple prototype demonstrating real-time translation and conversation topic assistance using a configurable audio source and overlay window.

## Features

- Capture audio from the microphone using ``sounddevice``.
- Convert speech to text with ``SpeechRecognition`` and the Google API.
- Send recognized text to an LLM for translation or topic summarization via the OpenAI API.
- Always-on-top overlay window showing results.
- Basic settings window to toggle microphone or system audio input.

## Requirements

- Python 3.8+
- PyQt5
- sounddevice
- numpy
- openai
- SpeechRecognition

Install dependencies with:

```bash
pip install -r requirements.txt
```

## Running

```bash
python app.py
```

This launches both the overlay and settings window.

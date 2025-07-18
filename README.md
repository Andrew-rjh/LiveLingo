# LiveLingo

A simple prototype demonstrating real-time translation and conversation topic assistance using a configurable audio source and overlay window.

## Features

- Capture system audio using ``sounddevice`` (WASAPI loopback on Windows).
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

Set your OpenAI API key in the ``OPENAI_API_KEY`` environment variable before
running the app:

```bash
export OPENAI_API_KEY=your_key_here
```

## Running

```bash
python app.py
```

This launches both the overlay and settings window.

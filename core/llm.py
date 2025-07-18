"""LLM integration module."""

from typing import Any

import openai


class LLMClient:
    """Interface to an LLM for translation or conversation topic suggestions."""

    def __init__(self, api_key: str, model: str = "gpt-3.5-turbo"):
        self.model = model
        openai.api_key = api_key

    def _call(self, prompt: str) -> str:
        """Internal helper to send a prompt to the LLM."""
        response = openai.ChatCompletion.create(
            model=self.model,
            messages=[{"role": "user", "content": prompt}],
        )
        return response["choices"][0]["message"]["content"].strip()

    def translate(self, text: str, target_lang: str = "en") -> str:
        """Translate given text to the target language using the LLM."""
        prompt = f"Translate the following text to {target_lang}:\n{text}"
        return self._call(prompt)

    def summarize_topic(self, text: str) -> str:
        """Return conversation topic suggestion for the given text."""
        prompt = (
            "Provide a short summary of the main topic in the following conversation:"\
            f"\n{text}"
        )
        return self._call(prompt)

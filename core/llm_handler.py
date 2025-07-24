import openai

class LLMHandler:
    """Interface to an LLM for translation or conversation topic suggestions."""

    def __init__(self, api_key: str, model: str = "gpt-4-turbo"):
        self.model = model
        self.client = openai.OpenAI(api_key=api_key)

    def _call(self, prompt: str) -> str:
        """Internal helper to send a prompt to the LLM."""
        try:
            response = self.client.chat.completions.create(
                model=self.model,
                messages=[{"role": "user", "content": prompt}],
            )
            return response.choices[0].message.content.strip()
        except Exception as e:
            print(f"Error calling LLM: {e}")
            return ""

    def translate(self, text: str, target_lang: str = "English") -> str:
        """Translate given text to the target language using the LLM."""
        prompt = (
            f"Translate the following text to {target_lang}. "
            "Only return the translated text, without any additional explanations or phrases like 'Here is the translation.':\n\n"
            f'"{text}"'
        )
        return self._call(prompt)

    def summarize_topic(self, text: str) -> str:
        """Return conversation topic suggestion for the given text."""
        prompt = (
            "Provide a short, one-sentence summary of the main topic in the following conversation:"
            f'\n\"{text}"'
        )
        return self._call(prompt)

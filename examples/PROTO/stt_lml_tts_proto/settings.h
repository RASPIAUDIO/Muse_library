#ifndef MUSE_SETTINGS_H
#define MUSE_SETTINGS_H

// Central configuration for Muse AI integration
// Move sensitive values out of source control for production use.

namespace MuseAISettings {
  // OpenAI API key
  inline const char OPENAI_API_KEY[] = "sk-YOUR ENTER API KEY HERE";

  // Model identifiers
  inline const char MODEL_LLM[] = "gpt-4o-mini";
  inline const char MODEL_TTS[] = "tts-1";
  inline const char MODEL_STT[] = "gpt-4o-mini-transcribe";

  // OpenAI endpoints
  inline const char URL_CHAT[] = "https://api.openai.com/v1/chat/completions";
  inline const char URL_TTS[]  = "https://api.openai.com/v1/audio/speech";
  inline const char URL_STT[]  = "https://api.openai.com/v1/audio/transcriptions";
}

#endif // MUSE_SETTINGS_H


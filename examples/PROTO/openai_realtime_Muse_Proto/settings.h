#ifndef MUSE_SETTINGS_H
#define MUSE_SETTINGS_H

// Central configuration for Muse AI integration
// Move sensitive values out of source control for production use.


/*──────────────────────── CLOUD END-POINT ─────────────────────*/
#define MODEL "gpt-4o-mini-realtime-preview-2024-12-17"
#define WS_URL "wss://api.openai.com/v1/realtime?model=" MODEL


/*────────────────────── TLS root (Google) ─────────────────────*/
static const char CA_GTS_ROOT_R4[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDejCCAmKgAwIBAgIQf+UwvzMTQ77dghYQST2KGzANBgkqhkiG9w0BAQsFADBX
MQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTEQMA4GA1UE
CxMHUm9vdCBDQTEbMBkGA1UEAxMSR2xvYmFsU2lnbiBSb290IENBMB4XDTIzMTEx
NTAzNDMyMVoXDTI4MDEyODAwMDA0MlowRzELMAkGA1UEBhMCVVMxIjAgBgNVBAoT
GUdvb2dsZSBUcnVzdCBTZXJ2aWNlcyBMTEMxFDASBgNVBAMTC0dUUyBSb290IFI0
MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE83Rzp2iLYK5DuDXFgTB7S0md+8Fhzube
Rr1r1WEYNa5A3XP3iZEwWus87oV8okB2O6nGuEfYKueSkWpz6bFyOZ8pn6KY019e
WIZlD6GEZQbR3IvJx3PIjGov5cSr0R2Ko4H/MIH8MA4GA1UdDwEB/wQEAwIBhjAd
BgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwDwYDVR0TAQH/BAUwAwEB/zAd
BgNVHQ4EFgQUgEzW63T/STaj1dj8tT7FavCUHYwwHwYDVR0jBBgwFoAUYHtmGkUN
l8qJUC99BM00qP/8/UswNgYIKwYBBQUHAQEEKjAoMCYGCCsGAQUFBzAChhhodHRw
Oi8vaS5wa2kuZ29vZy9nc3IxLmNydDAtBgNVHR8EJjAkMCKgIKAehhxodHRwOi8v
Yy5wa2kuZ29vZy9yL2dzcjEuY3JsMBMGA1UdIAQMMAowCAYGZ4EMAQIBMA0GCSqG
SIb3DQEBCwUAA4IBAQAYQrsPBtYDh5bjP2OBDwmkoWhIDDkic574y04tfzHpn+cJ
aodI2D4SseesQ6bDrarZ7C30ddLibZatoKiws3UL9xnELz4ct92vID24FfVbiI1h
Y+SW6FoVHkNeWIP0GCbaM4C6uVdF5dTUsMVs/ZbzNnIdCp5Gxmx5ejvEau8otR/C
skGN+hr/W5GvT1tMBjgWKZ1i4//emhA1JG1BbPzoLJQvyEotc03lXjTaCzv8mEbe
p8RqZ7a2CPsgRbuvTPBwcOMBBmuFeU88+FSBX6+7iP0il8b4Z0QFqIwwMHfs/L6K
1vepuoxtGzi4CZ68zJpiq1UvSqTbFJjtbD4seiMHl
-----END CERTIFICATE-----
)EOF";


namespace MuseAISettings {
// OpenAI API key
inline const char OPENAI_API_KEY[] = "sk-YOUR ENTER API KEY HERE";

// Model identifiers
inline const char MODEL_LLM[] = "gpt-4o-mini";
inline const char MODEL_TTS[] = "tts-1";
inline const char MODEL_STT[] = "gpt-4o-mini-transcribe";

// OpenAI endpoints
inline const char URL_CHAT[] = "https://api.openai.com/v1/chat/completions";
inline const char URL_TTS[] = "https://api.openai.com/v1/audio/speech";
inline const char URL_STT[] = "https://api.openai.com/v1/audio/transcriptions";
}

#endif  // MUSE_SETTINGS_H

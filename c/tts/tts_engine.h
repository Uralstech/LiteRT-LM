// Copyright 2026 The ODML Authors.
// Copyright 2026 URAV ADVANCED LEARNING SYSTEMS PRIVATE LIMITED
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_ODML_LITERT_LM_C_TTS_ENGINE_H_
#define THIRD_PARTY_ODML_LITERT_LM_C_TTS_ENGINE_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// For Windows, __declspec( dllexport ) is required to export function in .dll.
// https://learn.microsoft.com/en-us/cpp/cpp/using-dllimport-and-dllexport-in-cpp-classes?view=msvc-170
//
// _WIN32 is defined as 1 when the compilation target is 32-bit ARM, 64-bit ARM,
// x86, x64, or ARM64EC. Otherwise, undefined.
// https://learn.microsoft.com/en-us/cpp/preprocessor/predefined-macros
#if defined(_WIN32)
#define LITERT_LM_C_TTS_API_EXPORT __declspec(dllexport)
#else
// Ensure symbols are exported when building the shared library with
// -fvisibility=hidden.
#define LITERT_LM_C_TTS_API_EXPORT __attribute__((visibility("default")))
#endif

typedef enum {
    kLiteRtOmniTtsModelTypeUnspecified = 0,
    // Kokoro-82M.
    kLiteRtOmniTtsModelTypeKokoro = 1,
    // Qwen3-TTS.
    kLiteRtOmniTtsModelTypeQwen3Tts = 2,
} LiteRtOmniTtsModelType;

typedef struct LiteRtOmniTtsEngineSettings LiteRtOmniTtsEngineSettings;
typedef struct LiteRtOmniTtsEngine LiteRtOmniTtsEngine;
typedef struct LiteRtOmniTtsSession LiteRtOmniTtsSession;
typedef struct LiteRtOmniAudioOutput LiteRtOmniAudioOutput;

LITERT_LM_C_TTS_API_EXPORT
LiteRtOmniTtsEngineSettings* litert_omni_tts_engine_settings_create(LiteRtOmniTtsModelType model_type);

LITERT_LM_C_TTS_API_EXPORT
void litert_omni_tts_engine_settings_delete(LiteRtOmniTtsEngineSettings* settings);

LITERT_LM_C_TTS_API_EXPORT
void litert_omni_tts_engine_settings_set_model_folder(LiteRtOmniTtsEngineSettings* settings, const char* model_folder);

LITERT_LM_C_TTS_API_EXPORT
void litert_omni_tts_engine_settings_set_cache_dir(LiteRtOmniTtsEngineSettings* settings, const char* cache_dir);

LITERT_LM_C_TTS_API_EXPORT
void litert_omni_tts_engine_settings_set_backend(LiteRtOmniTtsEngineSettings* settings, const char* backend_str);

LITERT_LM_C_TTS_API_EXPORT
void litert_omni_tts_engine_settings_set_num_threads(LiteRtOmniTtsEngineSettings* settings, int num_threads);

LITERT_LM_C_TTS_API_EXPORT
void litert_omni_tts_engine_settings_set_max_frames(LiteRtOmniTtsEngineSettings* settings, int max_frames);

LITERT_LM_C_TTS_API_EXPORT
LiteRtOmniTtsEngine* litert_omni_tts_engine_create(const LiteRtOmniTtsEngineSettings* settings);

LITERT_LM_C_TTS_API_EXPORT
void litert_omni_tts_engine_delete(LiteRtOmniTtsEngine* engine);

LITERT_LM_C_TTS_API_EXPORT
LiteRtOmniTtsSession* litert_omni_tts_engine_create_session(LiteRtOmniTtsEngine* engine);

LITERT_LM_C_TTS_API_EXPORT
void litert_omni_tts_session_delete(LiteRtOmniTtsSession* session);

LITERT_LM_C_TTS_API_EXPORT
LiteRtOmniAudioOutput* litert_omni_tts_session_synthesize(LiteRtOmniTtsSession* session, const char* text);

LITERT_LM_C_TTS_API_EXPORT
void litert_omni_audio_output_delete(LiteRtOmniAudioOutput* audio);

LITERT_LM_C_TTS_API_EXPORT
const float* litert_omni_audio_output_get_pcm_samples(const LiteRtOmniAudioOutput* audio);

LITERT_LM_C_TTS_API_EXPORT
size_t litert_omni_audio_output_get_num_pcm_samples(const LiteRtOmniAudioOutput* audio);

LITERT_LM_C_TTS_API_EXPORT
int litert_omni_audio_output_get_sample_rate_hz(const LiteRtOmniAudioOutput* audio);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // THIRD_PARTY_ODML_LITERT_LM_C_TTS_ENGINE_H_

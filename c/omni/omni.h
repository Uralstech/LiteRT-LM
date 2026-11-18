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

#ifndef THIRD_PARTY_ODML_LITERT_LM_C_OMNI_OMNI_H_
#define THIRD_PARTY_ODML_LITERT_LM_C_OMNI_OMNI_H_

#include <stddef.h>

#include "c/api_export.h"
#include "c/error_reporter.h"

#ifdef __cplusplus
extern "C" {
#endif

// Model type values are ABI-stable.
typedef enum LiteRtOmniTtsModelType {
  kLiteRtOmniTtsModelTypeUnspecified = 0,
  kLiteRtOmniTtsModelTypeKokoro = 1,
  kLiteRtOmniTtsModelTypeQwen3Tts = 2,
} LiteRtOmniTtsModelType;

// Speech-recognition model type values are ABI-stable.
typedef enum LiteRtOmniAsrModelType {
  kLiteRtOmniAsrModelTypeUnspecified = 0,
  kLiteRtOmniAsrModelTypeParakeetTdt = 1,
  kLiteRtOmniAsrModelTypeParakeetCtc = 2,
  kLiteRtOmniAsrModelTypeWhisper = 3,
  kLiteRtOmniAsrModelTypeQwen3Asr = 4,
  kLiteRtOmniAsrModelTypeMoonshine = 5,
  kLiteRtOmniAsrModelTypeTinyGemma = 6,
} LiteRtOmniAsrModelType;

// Describes the result of a successful incremental TTS or ASR operation.
// Functions return LiteRtLmStatusCode for API success/failure and write one of
// these values to out_stream_status on success.
typedef enum LiteRtOmniStreamStatus {
  kLiteRtOmniStreamStatusInputAccepted = 0,
  kLiteRtOmniStreamStatusOutputAvailable = 1,
  kLiteRtOmniStreamStatusNeedMoreInput = 2,
  kLiteRtOmniStreamStatusEndOfStream = 3,
} LiteRtOmniStreamStatus;

typedef struct LiteRtOmniTtsEngineSettings LiteRtOmniTtsEngineSettings;
typedef struct LiteRtOmniTtsEngine LiteRtOmniTtsEngine;
typedef struct LiteRtOmniTtsSession LiteRtOmniTtsSession;
typedef struct LiteRtOmniAudioOutput LiteRtOmniAudioOutput;

typedef struct LiteRtOmniAsrEngineSettings LiteRtOmniAsrEngineSettings;
typedef struct LiteRtOmniAsrEngine LiteRtOmniAsrEngine;
typedef struct LiteRtOmniAsrSession LiteRtOmniAsrSession;
typedef struct LiteRtOmniTextOutput LiteRtOmniTextOutput;

// TTS settings. A model folder may be a .litertlm package or a model directory.
// Settings are copied when the engine is created and may then be deleted.
LITERT_LM_C_API_EXPORT
LiteRtLmStatusCode litert_omni_tts_engine_settings_create(
    LiteRtOmniTtsModelType model_type,
    LiteRtOmniTtsEngineSettings** out_settings);
LITERT_LM_C_API_EXPORT void
litert_omni_tts_engine_settings_delete(LiteRtOmniTtsEngineSettings* settings);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_tts_engine_settings_set_model_folder(
    LiteRtOmniTtsEngineSettings* settings, const char* model_folder);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_tts_engine_settings_set_cache_dir(
    LiteRtOmniTtsEngineSettings* settings, const char* cache_dir);
// Accepts "cpu", "gpu", or "npu" (case-insensitive). GPU execution depends
// on the model, device, and target build supporting the selected backend.
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_tts_engine_settings_set_backend(
    LiteRtOmniTtsEngineSettings* settings, const char* backend_str);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_tts_engine_settings_set_num_threads(
    LiteRtOmniTtsEngineSettings* settings, int num_threads);
// Requests low GPU scheduling priority where supported. The kernel batch size
// is passed as a GPU runtime hint; zero preserves the runtime default. These
// settings can improve responsiveness, but do not impose a GPU utilization
// limit.
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_tts_engine_settings_set_gpu_context_low_priority(
    LiteRtOmniTtsEngineSettings* settings, bool low_priority);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_tts_engine_settings_set_hint_kernel_batch_size(
    LiteRtOmniTtsEngineSettings* settings, int batch_size);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_tts_engine_settings_set_max_frames(
    LiteRtOmniTtsEngineSettings* settings, int max_frames);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_tts_engine_settings_set_language(
    LiteRtOmniTtsEngineSettings* settings, const char* language);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_tts_engine_settings_set_voice(
    LiteRtOmniTtsEngineSettings* settings, const char* voice);

LITERT_LM_C_API_EXPORT LiteRtLmStatusCode litert_omni_tts_engine_create(
    const LiteRtOmniTtsEngineSettings* settings,
    LiteRtOmniTtsEngine** out_engine);
// Delete every session created from `engine` before deleting the engine.
LITERT_LM_C_API_EXPORT void
litert_omni_tts_engine_delete(LiteRtOmniTtsEngine* engine);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_tts_engine_create_session(LiteRtOmniTtsEngine* engine,
                                      LiteRtOmniTtsSession** out_session);
LITERT_LM_C_API_EXPORT void
litert_omni_tts_session_delete(LiteRtOmniTtsSession* session);
// Starts a new TTS stream, resetting any prior stream state. Stream calls are
// synchronous and a session must not be used concurrently.
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_tts_session_stream_begin(
    LiteRtOmniTtsSession* session,
    LiteRtOmniStreamStatus* out_stream_status);
// Queues the next text chunk. Text may be pushed while synthesis proceeds.
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_tts_session_stream_push_text(
    LiteRtOmniTtsSession* session, const char* text_chunk,
    LiteRtOmniStreamStatus* out_stream_status);
// Marks the end of text input. No more chunks may be pushed until begin().
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_tts_session_stream_finish(
    LiteRtOmniTtsSession* session,
    LiteRtOmniStreamStatus* out_stream_status);
// Synchronously produces the next PCM chunk. `out_audio` is NULL unless
// OutputAvailable is written to out_stream_status. NeedMoreInput means the
// pipeline needs another text chunk; EndOfStream means output is drained.
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_tts_session_stream_process_next(
    LiteRtOmniTtsSession* session,
    LiteRtOmniStreamStatus* out_stream_status,
    LiteRtOmniAudioOutput** out_audio);
// Synchronously synthesizes text. Delete the output with
// litert_omni_audio_output_delete. The session must not be used concurrently.
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_tts_session_synthesize(LiteRtOmniTtsSession* session,
                                   const char* text,
                                   LiteRtOmniAudioOutput** out_audio);

LITERT_LM_C_API_EXPORT void
litert_omni_audio_output_delete(LiteRtOmniAudioOutput* audio);
// Returned PCM is owned by `audio` and remains valid until it is deleted.
// Returns NULL and zero samples for a valid output containing no samples.
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_audio_output_get_pcm_samples(const LiteRtOmniAudioOutput* audio,
                                         const float** out_pcm_samples);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_audio_output_get_num_pcm_samples(
    const LiteRtOmniAudioOutput* audio, size_t* out_num_pcm_samples);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_audio_output_get_sample_rate_hz(
    const LiteRtOmniAudioOutput* audio, int* out_sample_rate_hz);

// ASR input audio is supplied as mono float PCM; samples are resampled by Omni
// as needed. Settings are copied when the engine is created and may then be
// deleted.
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_asr_engine_settings_create(
    LiteRtOmniAsrModelType model_type,
    LiteRtOmniAsrEngineSettings** out_settings);
LITERT_LM_C_API_EXPORT void
litert_omni_asr_engine_settings_delete(LiteRtOmniAsrEngineSettings* settings);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_asr_engine_settings_set_model_folder(
    LiteRtOmniAsrEngineSettings* settings, const char* model_folder);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_asr_engine_settings_set_cache_dir(
    LiteRtOmniAsrEngineSettings* settings, const char* cache_dir);
// Accepts "cpu", "gpu", or "npu" (case-insensitive). GPU execution depends
// on the model, device, and target build supporting the selected backend.
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_asr_engine_settings_set_backend(
    LiteRtOmniAsrEngineSettings* settings, const char* backend_str);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_asr_engine_settings_set_num_threads(
    LiteRtOmniAsrEngineSettings* settings, int num_threads);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_asr_engine_settings_set_gpu_context_low_priority(
    LiteRtOmniAsrEngineSettings* settings, bool low_priority);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_asr_engine_settings_set_hint_kernel_batch_size(
    LiteRtOmniAsrEngineSettings* settings, int batch_size);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_asr_engine_settings_set_overlap_ratio(
    LiteRtOmniAsrEngineSettings* settings, float overlap_ratio);

LITERT_LM_C_API_EXPORT LiteRtLmStatusCode litert_omni_asr_engine_create(
    const LiteRtOmniAsrEngineSettings* settings,
    LiteRtOmniAsrEngine** out_engine);
// Delete every session created from `engine` before deleting the engine.
LITERT_LM_C_API_EXPORT void
litert_omni_asr_engine_delete(LiteRtOmniAsrEngine* engine);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_asr_engine_create_session(LiteRtOmniAsrEngine* engine,
                                      LiteRtOmniAsrSession** out_session);
LITERT_LM_C_API_EXPORT void
litert_omni_asr_session_delete(LiteRtOmniAsrSession* session);
// Starts a new mono audio stream at the given sample rate, resetting prior
// stream state. The sample rate cannot change before finish().
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_asr_session_stream_begin(
    LiteRtOmniAsrSession* session, int sample_rate_hz,
    LiteRtOmniStreamStatus* out_stream_status);
// Queues more mono float PCM samples. Samples are copied before return. A zero
// sample count is allowed and does not require pcm_samples to be non-NULL.
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_asr_session_stream_push_audio(
    LiteRtOmniAsrSession* session, const float* pcm_samples,
    size_t num_pcm_samples, LiteRtOmniStreamStatus* out_stream_status);
// Marks the end of audio input. No more samples may be pushed until begin().
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_asr_session_stream_finish(
    LiteRtOmniAsrSession* session,
    LiteRtOmniStreamStatus* out_stream_status);
// Synchronously produces the next partial or final text chunk. Each output
// exposes confirmed and unconfirmed text and a joined transcript snapshot.
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_asr_session_stream_process_next(
    LiteRtOmniAsrSession* session,
    LiteRtOmniStreamStatus* out_stream_status,
    LiteRtOmniTextOutput** out_text);
// Synchronously transcribes mono float PCM at `sample_rate_hz`. Input samples
// are copied before this function returns. The session must not be used
// concurrently.
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_asr_session_transcribe(LiteRtOmniAsrSession* session,
                                   const float* pcm_samples,
                                   size_t num_pcm_samples,
                                   int sample_rate_hz,
                                   LiteRtOmniTextOutput** out_text);

LITERT_LM_C_API_EXPORT void
litert_omni_text_output_delete(LiteRtOmniTextOutput* text);
// Returned UTF-8 strings are owned by `text` and remain valid until deletion.
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_text_output_get_transcript(const LiteRtOmniTextOutput* text,
                                       const char** out_transcript);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_text_output_get_confirmed_text(const LiteRtOmniTextOutput* text,
                                           const char** out_confirmed_text);
LITERT_LM_C_API_EXPORT LiteRtLmStatusCode
litert_omni_text_output_get_unconfirmed_text(
    const LiteRtOmniTextOutput* text, const char** out_unconfirmed_text);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // THIRD_PARTY_ODML_LITERT_LM_C_OMNI_OMNI_H_

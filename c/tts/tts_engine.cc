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

#include "c/tts/tts_engine.h"

#include "absl/log/absl_log.h"  // from @com_google_absl
#include "absl/status/statusor.h"  // from @com_google_absl
#include "c/tts/tts_engine_internal.h"

using ::litert::omni::tts::TtsEngineSettings;
using ::litert::omni::tts::ModelType;
using ::litert::omni::tts::TtsEngine;
using ::litert::omni::tts::TtsSession;
using ::litert::omni::AudioOutput;

LiteRtOmniTtsEngineSettings* litert_omni_tts_engine_settings_create(LiteRtOmniTtsModelType model_type) {

    auto* settings = new LiteRtOmniTtsEngineSettings;
    settings->settings = std::make_unique<TtsEngineSettings>();
    settings->settings->model_type = static_cast<ModelType>(model_type);

    return settings;
}

void litert_omni_tts_engine_settings_delete(LiteRtOmniTtsEngineSettings* settings) {
    delete settings;
}

void litert_omni_tts_engine_settings_set_model_folder(LiteRtOmniTtsEngineSettings* settings, const char* model_folder) {

    if (settings && settings->settings && model_folder) {
        settings->settings->model_folder = model_folder;
    }
}

void litert_omni_tts_engine_settings_set_cache_dir(LiteRtOmniTtsEngineSettings* settings, const char* cache_dir) {

    if (settings && settings->settings) {
        settings->settings->cache_dir = cache_dir ? cache_dir : "";
    }
}

void litert_omni_tts_engine_settings_set_backend(LiteRtOmniTtsEngineSettings* settings, const char* backend_str) {

    if (!settings || !settings->settings) {
        return;
    }

    auto backend = litert::lm::GetBackendFromString(backend_str);
    if (!backend.ok()) {
      ABSL_LOG(ERROR) << "Failed to parse backend: " << backend.status();
      return;
    }

    settings->settings->backend = *backend;
}

void litert_omni_tts_engine_settings_set_num_threads(LiteRtOmniTtsEngineSettings* settings, int num_threads) {

    if (settings && settings->settings) {
        settings->settings->num_threads = num_threads;
    }
}

void litert_omni_tts_engine_settings_set_max_frames(LiteRtOmniTtsEngineSettings* settings, int max_frames) {

    if (settings && settings->settings) {
        settings->settings->max_frames = max_frames;
    }
}

LiteRtOmniTtsEngine* litert_omni_tts_engine_create(const LiteRtOmniTtsEngineSettings* settings) {

    if (!settings || !settings->settings) {
        return nullptr;
    }

    absl::StatusOr<std::unique_ptr<TtsEngine>> engine =
            TtsEngine::Create(*settings->settings);

    if (!engine.ok()) {
        ABSL_LOG(ERROR) << "Failed to create engine: " << engine.status();
        return nullptr;
    }

    auto* c_engine = new LiteRtOmniTtsEngine;
    c_engine->engine = std::move(*engine);
    return c_engine;
}

void litert_omni_tts_engine_delete(LiteRtOmniTtsEngine* engine) {
    delete engine;
}

LiteRtOmniTtsSession* litert_omni_tts_engine_create_session(LiteRtOmniTtsEngine* engine) {

    if (!engine || !engine->engine) {
        return nullptr;
    }

    absl::StatusOr<std::unique_ptr<TtsSession>> session =
            engine->engine->CreateSession();

    if (!session.ok()) {
        ABSL_LOG(ERROR) << "Failed to create session: " << session.status();
        return nullptr;
    }

    auto* c_session = new LiteRtOmniTtsSession;
    c_session->session = std::move(*session);
    return c_session;
}

void litert_omni_tts_session_delete(LiteRtOmniTtsSession* session) {
    delete session;
}

LiteRtOmniAudioOutput* litert_omni_tts_session_synthesize(LiteRtOmniTtsSession* session, const char* text) {

    if (!session || !session->session) {
        return nullptr;
    }

    if (!text) {
        ABSL_LOG(ERROR) << "Text is null.";
        return nullptr;
    }

    absl::StatusOr<AudioOutput> audio =
        session->session->Synthesize(text);

    if (!audio.ok()) {
        ABSL_LOG(ERROR) << "Failed to synthesize audio: " << audio.status();
        return nullptr;
    }

    auto* c_audio = new LiteRtOmniAudioOutput;
    c_audio->audio =
            std::make_unique<AudioOutput>(
                    std::move(*audio));

    return c_audio;
}

void litert_omni_audio_output_delete(LiteRtOmniAudioOutput* audio) {
    delete audio;
}

const float* litert_omni_audio_output_get_pcm_samples(const LiteRtOmniAudioOutput* audio) {
    if (!audio || !audio->audio) {
        return nullptr;
    }

    return audio->audio->pcm_samples.data();
}

size_t litert_omni_audio_output_get_num_pcm_samples(const LiteRtOmniAudioOutput* audio) {
    if (!audio || !audio->audio) {
        return 0;
    }

    return audio->audio->pcm_samples.size();
}

int litert_omni_audio_output_get_sample_rate_hz(const LiteRtOmniAudioOutput* audio) {
    if (!audio || !audio->audio) {
        return -1;
    }

    return audio->audio->sample_rate_hz;
}
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

#ifndef THIRD_PARTY_ODML_LITERT_LM_C_TTS_ENGINE_INTERNAL_H_
#define THIRD_PARTY_ODML_LITERT_LM_C_TTS_ENGINE_INTERNAL_H_

#include <memory>

#include "c/tts/tts_engine.h"
#include "omni/tts/tts_engine.h"
#include "omni/base/io_types.h"

struct LiteRtOmniTtsEngineSettings {
    std::unique_ptr<litert::omni::tts::TtsEngineSettings> settings;
};

struct LiteRtOmniTtsEngine {
    std::unique_ptr<litert::omni::tts::TtsEngine> engine;
};

struct LiteRtOmniTtsSession {
    std::unique_ptr<litert::omni::tts::TtsSession> session;
};

struct LiteRtOmniAudioOutput {
    std::unique_ptr<litert::omni::AudioOutput> audio;
};

#endif  // THIRD_PARTY_ODML_LITERT_LM_C_TTS_ENGINE_INTERNAL_H_
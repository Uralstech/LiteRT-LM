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

#include "c/omni/omni.h"

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "c/error_reporter_internal.h"
#include "omni/base/io_types.h"
#include "omni/base/text_utils.h"
#include "omni/omni_engine.h"
#include "omni/omni_session.h"
#include "runtime/proto/speech2text_session_config.pb.h"
#include "runtime/proto/text2speech_metadata.pb.h"
#include "runtime/proto/text2speech_session_config.pb.h"

namespace {

using ::litert::omni::OmniEngine;
using ::litert::omni::OmniSession;
using ::litert::omni::PushInputSource;
using ::litert::lm::c::ReturnError;

struct EngineSettings {
  std::string model_name;
  std::string model_folder;
  std::string cache_dir;
  OmniEngine::Settings::Backend backend = OmniEngine::Settings::Backend::kCpu;
  int num_threads = 4;
  bool gpu_context_low_priority = false;
  int hint_kernel_batch_size = 0;
  int max_frames = 0;
  float overlap_ratio = 0.75f;
  std::string language;
  std::string voice;
};

absl::Status ParseBackend(absl::string_view value,
                          OmniEngine::Settings::Backend& backend) {
  std::string normalized(value);
  for (char& c : normalized) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  if (normalized == "cpu") {
    backend = OmniEngine::Settings::Backend::kCpu;
  } else if (normalized == "gpu") {
    backend = OmniEngine::Settings::Backend::kGpu;
  } else if (normalized == "npu") {
    backend = OmniEngine::Settings::Backend::kNpu;
  } else {
    return absl::InvalidArgumentError("backend must be cpu, gpu, or npu.");
  }
  return absl::OkStatus();
}

OmniEngine::Settings ToOmniSettings(const EngineSettings& input) {
  OmniEngine::Settings settings;
  settings.model_path = input.model_folder;
  settings.cache_dir = input.cache_dir;
  settings.backend = input.backend;
  settings.num_threads = input.num_threads;
  settings.gpu_context_low_priority = input.gpu_context_low_priority;
  settings.hint_kernel_batch_size = input.hint_kernel_batch_size;
  return settings;
}

absl::Status ValidateEngineSettings(const EngineSettings& settings,
                                    absl::string_view engine_name) {
  if (settings.model_name.empty()) {
    return absl::InvalidArgumentError(
        std::string(engine_name.data(), engine_name.size()) +
        " model type is invalid.");
  }
  if (settings.model_folder.empty()) {
    return absl::InvalidArgumentError(
        std::string(engine_name.data(), engine_name.size()) +
        " model folder must not be empty.");
  }
  if (settings.num_threads <= 0) {
    return absl::InvalidArgumentError(
        std::string(engine_name.data(), engine_name.size()) +
        " number of threads must be positive.");
  }
  if (settings.max_frames < 0) {
    return absl::InvalidArgumentError(
        "TTS max frames must be non-negative.");
  }
  if (settings.hint_kernel_batch_size < 0) {
    return absl::InvalidArgumentError(
        "GPU hint kernel batch size must be non-negative.");
  }
  if (!std::isfinite(settings.overlap_ratio) ||
      settings.overlap_ratio < 0.0f || settings.overlap_ratio >= 1.0f) {
    return absl::InvalidArgumentError(
        "ASR overlap ratio must be finite and in the range [0, 1).");
  }
  return absl::OkStatus();
}

std::string ModelName(LiteRtOmniTtsModelType model_type) {
  switch (model_type) {
    case kLiteRtOmniTtsModelTypeKokoro:
      return "kokoro";
    case kLiteRtOmniTtsModelTypeQwen3Tts:
      return "qwen3-tts";
    case kLiteRtOmniTtsModelTypeUnspecified:
    default:
      return "";
  }
}

std::string ModelName(LiteRtOmniAsrModelType model_type) {
  switch (model_type) {
    case kLiteRtOmniAsrModelTypeParakeetTdt:
      return "parakeet-tdt-0.6b-v3";
    case kLiteRtOmniAsrModelTypeParakeetCtc:
      return "parakeet-ctc-0.6b";
    case kLiteRtOmniAsrModelTypeWhisper:
      return "whisper-tiny";
    case kLiteRtOmniAsrModelTypeQwen3Asr:
      return "qwen3-asr-0.6b";
    case kLiteRtOmniAsrModelTypeMoonshine:
      return "moonshine-tiny";
    case kLiteRtOmniAsrModelTypeTinyGemma:
      return "tinygemma-asr";
    case kLiteRtOmniAsrModelTypeUnspecified:
    default:
      return "";
  }
}

void AppendTextChunk(std::string& target, absl::string_view chunk) {
  if (chunk.empty()) return;
  if (!target.empty() && target.back() != ' ' && chunk.front() != ' ') {
    size_t last_start = target.size() - 1;
    while (last_start > 0 &&
           (static_cast<unsigned char>(target[last_start]) & 0xC0) == 0x80) {
      --last_start;
    }
    const char32_t last = litert::omni::DecodeUtf8CodePoint(target, last_start);
    size_t first_pos = 0;
    const char32_t first = litert::omni::DecodeUtf8CodePoint(chunk, first_pos);
    const bool starts_with_punctuation = litert::omni::IsPunctuation(first);
    const bool ends_with_open_punctuation =
        last == '(' || last == '[' || last == '{' || last == 0x2018 ||
        last == 0x201C || last == 0x00AB || last == 0x2039;
    const bool adjacent_cjk =
        litert::omni::IsCjkPunctuation(last) ||
        litert::omni::IsCjkPunctuation(first) ||
        (litert::omni::IsCjkCharacter(last) &&
         litert::omni::IsCjkCharacter(first));
    if (!starts_with_punctuation && !ends_with_open_punctuation &&
        !adjacent_cjk) {
      target.push_back(' ');
    }
  }
  target.append(chunk.data(), chunk.size());
}

void CollectAudio(const OmniSession::AudioOutput& audio,
                  std::vector<float>& samples, int& sample_rate_hz) {
  samples.insert(samples.end(), audio.pcm_samples.begin(),
                 audio.pcm_samples.end());
  if (audio.sample_rate_hz > 0) sample_rate_hz = audio.sample_rate_hz;
}

void CollectAudio(const OmniSession::Output& output,
                  std::vector<float>& samples, int& sample_rate_hz) {
  const auto* audio = std::get_if<OmniSession::AudioOutput>(&output);
  if (audio != nullptr) CollectAudio(*audio, samples, sample_rate_hz);
}

#define LITERT_OMNI_C_RETURN_IF_ERROR(expr)                               \
  do {                                                                     \
    const LiteRtLmStatusCode litert_omni_status_ = (expr);                \
    if (litert_omni_status_ != kLiteRtLmStatusOk) {                        \
      return litert_omni_status_;                                          \
    }                                                                      \
  } while (false)

}  // namespace

struct LiteRtOmniTtsEngineSettings {
  EngineSettings settings;
};

struct LiteRtOmniTtsEngine {
  std::unique_ptr<OmniEngine> engine;
  EngineSettings settings;
};

struct LiteRtOmniTtsSession {
  std::unique_ptr<OmniSession> session;
  PushInputSource* input_source = nullptr;  // Owned by `session`.
  bool stream_started = false;
  bool stream_finished = false;
  bool stream_flushed = false;
};

struct LiteRtOmniAudioOutput {
  std::vector<float> pcm_samples;
  int sample_rate_hz = 24000;
};

struct LiteRtOmniAsrEngineSettings {
  EngineSettings settings;
};

struct LiteRtOmniAsrEngine {
  std::unique_ptr<OmniEngine> engine;
};

struct LiteRtOmniAsrSession {
  std::unique_ptr<OmniSession> session;
  PushInputSource* input_source = nullptr;  // Owned by `session`.
  bool stream_started = false;
  bool stream_finished = false;
  bool stream_flushed = false;
};

struct LiteRtOmniTextOutput {
  std::string confirmed_text;
  std::string unconfirmed_text;
  std::string transcript;
};

namespace {

LiteRtOmniTextOutput* MakeTextOutput(const OmniSession::TextOutput& output) {
  auto result = std::make_unique<LiteRtOmniTextOutput>();
  result->confirmed_text = output.confirmed_text;
  result->unconfirmed_text = output.unconfirmed_text;
  AppendTextChunk(result->transcript, output.confirmed_text);
  AppendTextChunk(result->transcript, output.unconfirmed_text);
  return result.release();
}

LiteRtLmStatusCode StreamError(absl::StatusCode code,
                               absl::string_view message) {
  return ReturnError(code, message);
}

LiteRtLmStatusCode FlushTtsStream(LiteRtOmniTtsSession* session,
                                  LiteRtOmniStreamStatus* out_stream_status,
                                  LiteRtOmniAudioOutput** out_audio) {
  if (session->stream_flushed) {
    *out_stream_status = kLiteRtOmniStreamStatusEndOfStream;
    return kLiteRtLmStatusOk;
  }
  session->stream_flushed = true;
  auto tail = session->session->Flush();
  if (!tail.ok()) {
    if (absl::IsOutOfRange(tail.status()) || absl::IsNotFound(tail.status())) {
      *out_stream_status = kLiteRtOmniStreamStatusEndOfStream;
      return kLiteRtLmStatusOk;
    }
    ABSL_LOG(ERROR) << "TTS stream flush failed: " << tail.status();
    return litert::lm::c::ToCStatus(tail.status());
  }
  if (std::get_if<OmniSession::AudioOutput>(&*tail) != nullptr) {
    auto result = std::make_unique<LiteRtOmniAudioOutput>();
    CollectAudio(*tail, result->pcm_samples, result->sample_rate_hz);
    *out_audio = result.release();
    *out_stream_status = kLiteRtOmniStreamStatusOutputAvailable;
    return kLiteRtLmStatusOk;
  }
  if (std::holds_alternative<OmniSession::EndOfOutput>(*tail) ||
      std::holds_alternative<OmniSession::EndOfOutputTurn>(*tail)) {
    *out_stream_status = kLiteRtOmniStreamStatusEndOfStream;
    return kLiteRtLmStatusOk;
  }
  return StreamError(absl::StatusCode::kInternal,
                     "Unexpected output type while flushing TTS stream.");
}

LiteRtLmStatusCode ProcessTtsStreamNext(
    LiteRtOmniTtsSession* session,
    LiteRtOmniStreamStatus* out_stream_status,
    LiteRtOmniAudioOutput** out_audio) {
  *out_audio = nullptr;
  if (session == nullptr || session->session == nullptr ||
      session->input_source == nullptr || !session->stream_started) {
    return StreamError(absl::StatusCode::kFailedPrecondition,
                       "TTS stream has not been started.");
  }
  if (session->stream_flushed) {
    *out_stream_status = kLiteRtOmniStreamStatusEndOfStream;
    return kLiteRtLmStatusOk;
  }

  auto output = session->session->ProcessNext();
  if (output.ok()) {
    if (std::holds_alternative<OmniSession::EndOfOutput>(*output) ||
        std::holds_alternative<OmniSession::EndOfOutputTurn>(*output)) {
      session->stream_flushed = true;
      *out_stream_status = kLiteRtOmniStreamStatusEndOfStream;
      return kLiteRtLmStatusOk;
    }
    if (std::get_if<OmniSession::AudioOutput>(&*output) == nullptr) {
      return StreamError(absl::StatusCode::kInternal,
                         "Unexpected output type while processing TTS stream.");
    }
    auto result = std::make_unique<LiteRtOmniAudioOutput>();
    CollectAudio(*output, result->pcm_samples, result->sample_rate_hz);
    *out_audio = result.release();
    *out_stream_status = kLiteRtOmniStreamStatusOutputAvailable;
    return kLiteRtLmStatusOk;
  }

  if (absl::IsNotFound(output.status())) {
    if (session->stream_finished) {
      return FlushTtsStream(session, out_stream_status, out_audio);
    }
    *out_stream_status = kLiteRtOmniStreamStatusNeedMoreInput;
    return kLiteRtLmStatusOk;
  }
  if (absl::IsOutOfRange(output.status()) && session->stream_finished) {
    return FlushTtsStream(session, out_stream_status, out_audio);
  }
  ABSL_LOG(ERROR) << "TTS stream processing failed: " << output.status();
  return litert::lm::c::ToCStatus(output.status());
}

LiteRtLmStatusCode FlushAsrStream(LiteRtOmniAsrSession* session,
                                  LiteRtOmniStreamStatus* out_stream_status,
                                  LiteRtOmniTextOutput** out_text) {
  if (session->stream_flushed) {
    *out_stream_status = kLiteRtOmniStreamStatusEndOfStream;
    return kLiteRtLmStatusOk;
  }
  session->stream_flushed = true;
  auto tail = session->session->Flush();
  if (!tail.ok()) {
    if (absl::IsOutOfRange(tail.status()) || absl::IsNotFound(tail.status())) {
      *out_stream_status = kLiteRtOmniStreamStatusEndOfStream;
      return kLiteRtLmStatusOk;
    }
    ABSL_LOG(ERROR) << "ASR stream flush failed: " << tail.status();
    return litert::lm::c::ToCStatus(tail.status());
  }
  if (const auto* text = std::get_if<OmniSession::TextOutput>(&*tail)) {
    *out_text = MakeTextOutput(*text);
    *out_stream_status = kLiteRtOmniStreamStatusOutputAvailable;
    return kLiteRtLmStatusOk;
  }
  if (std::holds_alternative<OmniSession::EndOfOutput>(*tail) ||
      std::holds_alternative<OmniSession::EndOfOutputTurn>(*tail)) {
    *out_stream_status = kLiteRtOmniStreamStatusEndOfStream;
    return kLiteRtLmStatusOk;
  }
  return StreamError(absl::StatusCode::kInternal,
                     "Unexpected output type while flushing ASR stream.");
}

LiteRtLmStatusCode ProcessAsrStreamNext(
    LiteRtOmniAsrSession* session,
    LiteRtOmniStreamStatus* out_stream_status,
    LiteRtOmniTextOutput** out_text) {
  *out_text = nullptr;
  if (session == nullptr || session->session == nullptr ||
      session->input_source == nullptr || !session->stream_started) {
    return StreamError(absl::StatusCode::kFailedPrecondition,
                       "ASR stream has not been started.");
  }
  if (session->stream_flushed) {
    *out_stream_status = kLiteRtOmniStreamStatusEndOfStream;
    return kLiteRtLmStatusOk;
  }

  auto output = session->session->ProcessNext();
  if (output.ok()) {
    if (std::holds_alternative<OmniSession::EndOfOutput>(*output) ||
        std::holds_alternative<OmniSession::EndOfOutputTurn>(*output)) {
      session->stream_flushed = true;
      *out_stream_status = kLiteRtOmniStreamStatusEndOfStream;
      return kLiteRtLmStatusOk;
    }
    const auto* text = std::get_if<OmniSession::TextOutput>(&*output);
    if (text == nullptr) {
      return StreamError(absl::StatusCode::kInternal,
                         "Unexpected output type while processing ASR stream.");
    }
    *out_text = MakeTextOutput(*text);
    *out_stream_status = kLiteRtOmniStreamStatusOutputAvailable;
    return kLiteRtLmStatusOk;
  }

  if (absl::IsNotFound(output.status())) {
    if (session->stream_finished) {
      return FlushAsrStream(session, out_stream_status, out_text);
    }
    *out_stream_status = kLiteRtOmniStreamStatusNeedMoreInput;
    return kLiteRtLmStatusOk;
  }
  if (absl::IsOutOfRange(output.status()) && session->stream_finished) {
    return FlushAsrStream(session, out_stream_status, out_text);
  }
  ABSL_LOG(ERROR) << "ASR stream processing failed: " << output.status();
  return litert::lm::c::ToCStatus(output.status());
}

}  // namespace

extern "C" {

LiteRtLmStatusCode litert_omni_tts_engine_settings_create(
    LiteRtOmniTtsModelType model_type,
    LiteRtOmniTtsEngineSettings** out_settings) {
  LITERT_LM_C_RETURN_IF_NULL(out_settings);
  *out_settings = nullptr;
  const std::string model_name = ModelName(model_type);
  if (model_name.empty()) {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "Invalid TTS model type.");
  }
  auto settings = std::make_unique<LiteRtOmniTtsEngineSettings>();
  settings->settings.model_name = model_name;
  *out_settings = settings.release();
  return kLiteRtLmStatusOk;
}

void litert_omni_tts_engine_settings_delete(
    LiteRtOmniTtsEngineSettings* settings) {
  delete settings;
}

LiteRtLmStatusCode litert_omni_tts_engine_settings_set_model_folder(
    LiteRtOmniTtsEngineSettings* settings, const char* model_folder) {
  LITERT_LM_C_RETURN_IF_NULL(settings);
  LITERT_LM_C_RETURN_IF_NULL(model_folder);
  settings->settings.model_folder = model_folder;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_tts_engine_settings_set_cache_dir(
    LiteRtOmniTtsEngineSettings* settings, const char* cache_dir) {
  LITERT_LM_C_RETURN_IF_NULL(settings);
  LITERT_LM_C_RETURN_IF_NULL(cache_dir);
  settings->settings.cache_dir = cache_dir;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_tts_engine_settings_set_backend(
    LiteRtOmniTtsEngineSettings* settings, const char* backend_str) {
  LITERT_LM_C_RETURN_IF_NULL(settings);
  LITERT_LM_C_RETURN_IF_NULL(backend_str);
  LITERT_LM_C_RETURN_IF_ERROR(
      ParseBackend(backend_str, settings->settings.backend));
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_tts_engine_settings_set_num_threads(
    LiteRtOmniTtsEngineSettings* settings, int num_threads) {
  LITERT_LM_C_RETURN_IF_NULL(settings);
  if (num_threads <= 0) {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "Number of threads must be positive.");
  }
  settings->settings.num_threads = num_threads;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode
litert_omni_tts_engine_settings_set_gpu_context_low_priority(
    LiteRtOmniTtsEngineSettings* settings, bool low_priority) {
  LITERT_LM_C_RETURN_IF_NULL(settings);
  settings->settings.gpu_context_low_priority = low_priority;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_tts_engine_settings_set_hint_kernel_batch_size(
    LiteRtOmniTtsEngineSettings* settings, int batch_size) {
  LITERT_LM_C_RETURN_IF_NULL(settings);
  if (batch_size < 0) {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "GPU hint kernel batch size must be non-negative.");
  }
  settings->settings.hint_kernel_batch_size = batch_size;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_tts_engine_settings_set_max_frames(
    LiteRtOmniTtsEngineSettings* settings, int max_frames) {
  LITERT_LM_C_RETURN_IF_NULL(settings);
  if (max_frames < 0) {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "TTS max frames must be non-negative.");
  }
  settings->settings.max_frames = max_frames;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_tts_engine_settings_set_language(
    LiteRtOmniTtsEngineSettings* settings, const char* language) {
  LITERT_LM_C_RETURN_IF_NULL(settings);
  LITERT_LM_C_RETURN_IF_NULL(language);
  settings->settings.language = language;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_tts_engine_settings_set_voice(
    LiteRtOmniTtsEngineSettings* settings, const char* voice) {
  LITERT_LM_C_RETURN_IF_NULL(settings);
  LITERT_LM_C_RETURN_IF_NULL(voice);
  settings->settings.voice = voice;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_tts_engine_create(
    const LiteRtOmniTtsEngineSettings* settings,
    LiteRtOmniTtsEngine** out_engine) {
  LITERT_LM_C_RETURN_IF_NULL(out_engine);
  *out_engine = nullptr;
  LITERT_LM_C_RETURN_IF_NULL(settings);
  LITERT_LM_C_RETURN_IF_ERROR(
      ValidateEngineSettings(settings->settings, "TTS"));

  auto effective = settings->settings;
  litert::lm::proto::Text2SpeechSessionConfig session_config;
  if (!effective.language.empty()) session_config.set_language(effective.language);
  if (!effective.voice.empty()) session_config.set_voice(effective.voice);
  litert::lm::proto::Text2SpeechMetadata metadata;
  if (effective.model_name == "qwen3-tts" && effective.max_frames > 0) {
    metadata.mutable_tts_model_type()
        ->mutable_qwen3_tts()
        ->set_max_frames(effective.max_frames);
  }

  auto engine = OmniEngine::Create(effective.model_name,
                                   ToOmniSettings(effective), &session_config,
                                   &metadata);
  if (!engine.ok()) {
    ABSL_LOG(ERROR) << "TTS engine creation failed: " << engine.status();
    return litert::lm::c::ToCStatus(engine.status());
  }
  auto result = std::make_unique<LiteRtOmniTtsEngine>();
  result->engine = std::move(*engine);
  result->settings = std::move(effective);
  *out_engine = result.release();
  return kLiteRtLmStatusOk;
}

void litert_omni_tts_engine_delete(LiteRtOmniTtsEngine* engine) {
  delete engine;
}

LiteRtLmStatusCode litert_omni_tts_engine_create_session(
    LiteRtOmniTtsEngine* engine, LiteRtOmniTtsSession** out_session) {
  LITERT_LM_C_RETURN_IF_NULL(out_session);
  *out_session = nullptr;
  if (engine == nullptr || engine->engine == nullptr) {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "Invalid TTS engine.");
  }
  auto input_source = std::make_unique<PushInputSource>();
  PushInputSource* input_source_ptr = input_source.get();
  auto session = engine->engine->CreateSession(std::move(input_source));
  if (!session.ok()) {
    ABSL_LOG(ERROR) << "TTS session creation failed: " << session.status();
    return litert::lm::c::ToCStatus(session.status());
  }
  auto result = std::make_unique<LiteRtOmniTtsSession>();
  result->session = std::move(*session);
  result->input_source = input_source_ptr;
  *out_session = result.release();
  return kLiteRtLmStatusOk;
}

void litert_omni_tts_session_delete(LiteRtOmniTtsSession* session) {
  delete session;
}

LiteRtLmStatusCode litert_omni_tts_session_stream_begin(
    LiteRtOmniTtsSession* session,
    LiteRtOmniStreamStatus* out_stream_status) {
  LITERT_LM_C_RETURN_IF_NULL(out_stream_status);
  if (session == nullptr || session->session == nullptr ||
      session->input_source == nullptr) {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "Invalid TTS session.");
  }
  session->session->Reset();
  session->stream_started = true;
  session->stream_finished = false;
  session->stream_flushed = false;
  *out_stream_status = kLiteRtOmniStreamStatusInputAccepted;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_tts_session_stream_push_text(
    LiteRtOmniTtsSession* session, const char* text_chunk,
    LiteRtOmniStreamStatus* out_stream_status) {
  LITERT_LM_C_RETURN_IF_NULL(out_stream_status);
  LITERT_LM_C_RETURN_IF_NULL(text_chunk);
  if (session == nullptr || session->session == nullptr ||
      session->input_source == nullptr || !session->stream_started ||
      session->stream_finished) {
    return ReturnError(absl::StatusCode::kFailedPrecondition,
                       "Invalid TTS stream state for text input.");
  }
  if (text_chunk[0] != '\0') {
    const absl::Status status = session->input_source->PushInput(
        OmniSession::TextInput{.text = text_chunk});
    LITERT_LM_C_RETURN_IF_ERROR(status);
  }
  *out_stream_status = kLiteRtOmniStreamStatusInputAccepted;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_tts_session_stream_finish(
    LiteRtOmniTtsSession* session,
    LiteRtOmniStreamStatus* out_stream_status) {
  LITERT_LM_C_RETURN_IF_NULL(out_stream_status);
  if (session == nullptr || session->session == nullptr ||
      session->input_source == nullptr || !session->stream_started ||
      session->stream_finished) {
    return ReturnError(absl::StatusCode::kFailedPrecondition,
                       "Invalid TTS stream state for finish.");
  }
  session->input_source->Finish();
  session->stream_finished = true;
  *out_stream_status = kLiteRtOmniStreamStatusInputAccepted;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_tts_session_stream_process_next(
    LiteRtOmniTtsSession* session,
    LiteRtOmniStreamStatus* out_stream_status,
    LiteRtOmniAudioOutput** out_audio) {
  if (out_audio != nullptr) *out_audio = nullptr;
  LITERT_LM_C_RETURN_IF_NULL(out_stream_status);
  LITERT_LM_C_RETURN_IF_NULL(out_audio);
  return ProcessTtsStreamNext(session, out_stream_status, out_audio);
}

LiteRtLmStatusCode litert_omni_tts_session_synthesize(
    LiteRtOmniTtsSession* session, const char* text,
    LiteRtOmniAudioOutput** out_audio) {
  LITERT_LM_C_RETURN_IF_NULL(out_audio);
  *out_audio = nullptr;
  LITERT_LM_C_RETURN_IF_NULL(text);
  if (session == nullptr || session->session == nullptr) {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "Invalid TTS session.");
  }
  if (text[0] == '\0') {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "TTS text must not be empty.");
  }

  LiteRtOmniStreamStatus stream_status;
  LITERT_OMNI_C_RETURN_IF_ERROR(
      litert_omni_tts_session_stream_begin(session, &stream_status));
  LITERT_OMNI_C_RETURN_IF_ERROR(litert_omni_tts_session_stream_push_text(
      session, text, &stream_status));
  LITERT_OMNI_C_RETURN_IF_ERROR(
      litert_omni_tts_session_stream_finish(session, &stream_status));

  auto result = std::make_unique<LiteRtOmniAudioOutput>();
  while (true) {
    LiteRtOmniAudioOutput* chunk = nullptr;
    LITERT_OMNI_C_RETURN_IF_ERROR(ProcessTtsStreamNext(
        session, &stream_status, &chunk));
    if (stream_status == kLiteRtOmniStreamStatusOutputAvailable) {
      if (chunk == nullptr) {
        return StreamError(absl::StatusCode::kInternal,
                           "TTS stream reported output without an audio chunk.");
      }
      if (!result->pcm_samples.empty() &&
          result->sample_rate_hz != chunk->sample_rate_hz) {
        delete chunk;
        return StreamError(absl::StatusCode::kInternal,
                           "TTS output sample rate changed mid-stream.");
      }
      if (result->pcm_samples.empty()) {
        result->sample_rate_hz = chunk->sample_rate_hz;
      }
      result->pcm_samples.insert(result->pcm_samples.end(),
                                 chunk->pcm_samples.begin(),
                                 chunk->pcm_samples.end());
      delete chunk;
      continue;
    }
    delete chunk;
    if (stream_status == kLiteRtOmniStreamStatusEndOfStream) break;
    if (stream_status == kLiteRtOmniStreamStatusNeedMoreInput) {
      return StreamError(absl::StatusCode::kInternal,
                         "TTS stream requested input after finish.");
    }
    return StreamError(absl::StatusCode::kInternal,
                       "Unexpected TTS stream status.");
  }
  *out_audio = result.release();
  return kLiteRtLmStatusOk;
}

void litert_omni_audio_output_delete(LiteRtOmniAudioOutput* audio) {
  delete audio;
}

LiteRtLmStatusCode litert_omni_audio_output_get_pcm_samples(
    const LiteRtOmniAudioOutput* audio, const float** out_pcm_samples) {
  LITERT_LM_C_RETURN_IF_NULL(out_pcm_samples);
  *out_pcm_samples = nullptr;
  LITERT_LM_C_RETURN_IF_NULL(audio);
  if (!audio->pcm_samples.empty()) *out_pcm_samples = audio->pcm_samples.data();
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_audio_output_get_num_pcm_samples(
    const LiteRtOmniAudioOutput* audio, size_t* out_num_pcm_samples) {
  LITERT_LM_C_RETURN_IF_NULL(out_num_pcm_samples);
  LITERT_LM_C_RETURN_IF_NULL(audio);
  *out_num_pcm_samples = audio->pcm_samples.size();
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_audio_output_get_sample_rate_hz(
    const LiteRtOmniAudioOutput* audio, int* out_sample_rate_hz) {
  LITERT_LM_C_RETURN_IF_NULL(out_sample_rate_hz);
  LITERT_LM_C_RETURN_IF_NULL(audio);
  *out_sample_rate_hz = audio->sample_rate_hz;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_asr_engine_settings_create(
    LiteRtOmniAsrModelType model_type,
    LiteRtOmniAsrEngineSettings** out_settings) {
  LITERT_LM_C_RETURN_IF_NULL(out_settings);
  *out_settings = nullptr;
  const std::string model_name = ModelName(model_type);
  if (model_name.empty()) {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "Invalid ASR model type.");
  }
  auto settings = std::make_unique<LiteRtOmniAsrEngineSettings>();
  settings->settings.model_name = model_name;
  *out_settings = settings.release();
  return kLiteRtLmStatusOk;
}

void litert_omni_asr_engine_settings_delete(
    LiteRtOmniAsrEngineSettings* settings) {
  delete settings;
}

LiteRtLmStatusCode litert_omni_asr_engine_settings_set_model_folder(
    LiteRtOmniAsrEngineSettings* settings, const char* model_folder) {
  LITERT_LM_C_RETURN_IF_NULL(settings);
  LITERT_LM_C_RETURN_IF_NULL(model_folder);
  settings->settings.model_folder = model_folder;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_asr_engine_settings_set_cache_dir(
    LiteRtOmniAsrEngineSettings* settings, const char* cache_dir) {
  LITERT_LM_C_RETURN_IF_NULL(settings);
  LITERT_LM_C_RETURN_IF_NULL(cache_dir);
  settings->settings.cache_dir = cache_dir;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_asr_engine_settings_set_backend(
    LiteRtOmniAsrEngineSettings* settings, const char* backend_str) {
  LITERT_LM_C_RETURN_IF_NULL(settings);
  LITERT_LM_C_RETURN_IF_NULL(backend_str);
  LITERT_LM_C_RETURN_IF_ERROR(
      ParseBackend(backend_str, settings->settings.backend));
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_asr_engine_settings_set_num_threads(
    LiteRtOmniAsrEngineSettings* settings, int num_threads) {
  LITERT_LM_C_RETURN_IF_NULL(settings);
  if (num_threads <= 0) {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "Number of threads must be positive.");
  }
  settings->settings.num_threads = num_threads;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode
litert_omni_asr_engine_settings_set_gpu_context_low_priority(
    LiteRtOmniAsrEngineSettings* settings, bool low_priority) {
  LITERT_LM_C_RETURN_IF_NULL(settings);
  settings->settings.gpu_context_low_priority = low_priority;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_asr_engine_settings_set_hint_kernel_batch_size(
    LiteRtOmniAsrEngineSettings* settings, int batch_size) {
  LITERT_LM_C_RETURN_IF_NULL(settings);
  if (batch_size < 0) {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "GPU hint kernel batch size must be non-negative.");
  }
  settings->settings.hint_kernel_batch_size = batch_size;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_asr_engine_settings_set_overlap_ratio(
    LiteRtOmniAsrEngineSettings* settings, float overlap_ratio) {
  LITERT_LM_C_RETURN_IF_NULL(settings);
  if (!std::isfinite(overlap_ratio) || overlap_ratio < 0.0f ||
      overlap_ratio >= 1.0f) {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "ASR overlap ratio must be finite and in [0, 1).");
  }
  settings->settings.overlap_ratio = overlap_ratio;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_asr_engine_create(
    const LiteRtOmniAsrEngineSettings* settings,
    LiteRtOmniAsrEngine** out_engine) {
  LITERT_LM_C_RETURN_IF_NULL(out_engine);
  *out_engine = nullptr;
  LITERT_LM_C_RETURN_IF_NULL(settings);
  LITERT_LM_C_RETURN_IF_ERROR(
      ValidateEngineSettings(settings->settings, "ASR"));

  litert::lm::proto::Speech2TextSessionConfig session_config;
  session_config.set_overlap_ratio(settings->settings.overlap_ratio);
  auto engine = OmniEngine::Create(settings->settings.model_name,
                                   ToOmniSettings(settings->settings),
                                   &session_config);
  if (!engine.ok()) {
    ABSL_LOG(ERROR) << "ASR engine creation failed: " << engine.status();
    return litert::lm::c::ToCStatus(engine.status());
  }
  auto result = std::make_unique<LiteRtOmniAsrEngine>();
  result->engine = std::move(*engine);
  *out_engine = result.release();
  return kLiteRtLmStatusOk;
}

void litert_omni_asr_engine_delete(LiteRtOmniAsrEngine* engine) {
  delete engine;
}

LiteRtLmStatusCode litert_omni_asr_engine_create_session(
    LiteRtOmniAsrEngine* engine, LiteRtOmniAsrSession** out_session) {
  LITERT_LM_C_RETURN_IF_NULL(out_session);
  *out_session = nullptr;
  if (engine == nullptr || engine->engine == nullptr) {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "Invalid ASR engine.");
  }
  auto input_source = std::make_unique<PushInputSource>();
  PushInputSource* input_source_ptr = input_source.get();
  auto session = engine->engine->CreateSession(std::move(input_source));
  if (!session.ok()) {
    ABSL_LOG(ERROR) << "ASR session creation failed: " << session.status();
    return litert::lm::c::ToCStatus(session.status());
  }
  auto result = std::make_unique<LiteRtOmniAsrSession>();
  result->session = std::move(*session);
  result->input_source = input_source_ptr;
  *out_session = result.release();
  return kLiteRtLmStatusOk;
}

void litert_omni_asr_session_delete(LiteRtOmniAsrSession* session) {
  delete session;
}

LiteRtLmStatusCode litert_omni_asr_session_stream_begin(
    LiteRtOmniAsrSession* session, int sample_rate_hz,
    LiteRtOmniStreamStatus* out_stream_status) {
  LITERT_LM_C_RETURN_IF_NULL(out_stream_status);
  if (session == nullptr || session->session == nullptr ||
      session->input_source == nullptr) {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "Invalid ASR session.");
  }
  if (sample_rate_hz <= 0) {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "ASR sample rate must be positive.");
  }
  session->session->Reset();
  const absl::Status status = session->input_source->PushInput(
      OmniSession::AudioInputMetadata{
          .sample_rate_hz = sample_rate_hz, .num_channels = 1});
  LITERT_LM_C_RETURN_IF_ERROR(status);
  session->stream_started = true;
  session->stream_finished = false;
  session->stream_flushed = false;
  *out_stream_status = kLiteRtOmniStreamStatusInputAccepted;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_asr_session_stream_push_audio(
    LiteRtOmniAsrSession* session, const float* pcm_samples,
    size_t num_pcm_samples, LiteRtOmniStreamStatus* out_stream_status) {
  LITERT_LM_C_RETURN_IF_NULL(out_stream_status);
  if (session == nullptr || session->session == nullptr ||
      session->input_source == nullptr || !session->stream_started ||
      session->stream_finished) {
    return ReturnError(absl::StatusCode::kFailedPrecondition,
                       "Invalid ASR stream state for audio input.");
  }
  if (num_pcm_samples > 0 && pcm_samples == nullptr) {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "PCM samples must not be NULL when the sample count is positive.");
  }
  if (num_pcm_samples > 0) {
    const absl::Status status = session->input_source->PushInput(
        OmniSession::AudioInput{
            .pcm_samples = std::vector<float>(pcm_samples,
                                              pcm_samples + num_pcm_samples)});
    LITERT_LM_C_RETURN_IF_ERROR(status);
  }
  *out_stream_status = kLiteRtOmniStreamStatusInputAccepted;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_asr_session_stream_finish(
    LiteRtOmniAsrSession* session,
    LiteRtOmniStreamStatus* out_stream_status) {
  LITERT_LM_C_RETURN_IF_NULL(out_stream_status);
  if (session == nullptr || session->session == nullptr ||
      session->input_source == nullptr || !session->stream_started ||
      session->stream_finished) {
    return ReturnError(absl::StatusCode::kFailedPrecondition,
                       "Invalid ASR stream state for finish.");
  }
  session->input_source->Finish();
  session->stream_finished = true;
  *out_stream_status = kLiteRtOmniStreamStatusInputAccepted;
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_asr_session_stream_process_next(
    LiteRtOmniAsrSession* session,
    LiteRtOmniStreamStatus* out_stream_status,
    LiteRtOmniTextOutput** out_text) {
  if (out_text != nullptr) *out_text = nullptr;
  LITERT_LM_C_RETURN_IF_NULL(out_stream_status);
  LITERT_LM_C_RETURN_IF_NULL(out_text);
  return ProcessAsrStreamNext(session, out_stream_status, out_text);
}

LiteRtLmStatusCode litert_omni_asr_session_transcribe(
    LiteRtOmniAsrSession* session, const float* pcm_samples,
    size_t num_pcm_samples, int sample_rate_hz,
    LiteRtOmniTextOutput** out_text) {
  LITERT_LM_C_RETURN_IF_NULL(out_text);
  *out_text = nullptr;
  if (session == nullptr || session->session == nullptr) {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "Invalid ASR session.");
  }
  if (num_pcm_samples == 0 || pcm_samples == nullptr) {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "ASR PCM input must contain at least one sample.");
  }
  if (sample_rate_hz <= 0) {
    return ReturnError(absl::StatusCode::kInvalidArgument,
                       "ASR sample rate must be positive.");
  }

  LiteRtOmniStreamStatus stream_status;
  LITERT_OMNI_C_RETURN_IF_ERROR(litert_omni_asr_session_stream_begin(
      session, sample_rate_hz, &stream_status));
  LITERT_OMNI_C_RETURN_IF_ERROR(litert_omni_asr_session_stream_push_audio(
      session, pcm_samples, num_pcm_samples, &stream_status));
  LITERT_OMNI_C_RETURN_IF_ERROR(
      litert_omni_asr_session_stream_finish(session, &stream_status));

  auto result = std::make_unique<LiteRtOmniTextOutput>();
  while (true) {
    LiteRtOmniTextOutput* chunk = nullptr;
    LITERT_OMNI_C_RETURN_IF_ERROR(ProcessAsrStreamNext(
        session, &stream_status, &chunk));
    if (stream_status == kLiteRtOmniStreamStatusOutputAvailable) {
      if (chunk == nullptr) {
        return StreamError(absl::StatusCode::kInternal,
                           "ASR stream reported output without a text chunk.");
      }
      AppendTextChunk(result->confirmed_text, chunk->confirmed_text);
      result->unconfirmed_text = std::move(chunk->unconfirmed_text);
      delete chunk;
      continue;
    }
    delete chunk;
    if (stream_status == kLiteRtOmniStreamStatusEndOfStream) break;
    if (stream_status == kLiteRtOmniStreamStatusNeedMoreInput) {
      return StreamError(absl::StatusCode::kInternal,
                         "ASR stream requested input after finish.");
    }
    return StreamError(absl::StatusCode::kInternal,
                       "Unexpected ASR stream status.");
  }

  AppendTextChunk(result->confirmed_text, result->unconfirmed_text);
  result->transcript = result->confirmed_text;
  result->unconfirmed_text.clear();
  *out_text = result.release();
  return kLiteRtLmStatusOk;
}

void litert_omni_text_output_delete(LiteRtOmniTextOutput* text) {
  delete text;
}

LiteRtLmStatusCode litert_omni_text_output_get_transcript(
    const LiteRtOmniTextOutput* text, const char** out_transcript) {
  LITERT_LM_C_RETURN_IF_NULL(out_transcript);
  *out_transcript = nullptr;
  LITERT_LM_C_RETURN_IF_NULL(text);
  *out_transcript = text->transcript.c_str();
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_text_output_get_confirmed_text(
    const LiteRtOmniTextOutput* text, const char** out_confirmed_text) {
  LITERT_LM_C_RETURN_IF_NULL(out_confirmed_text);
  *out_confirmed_text = nullptr;
  LITERT_LM_C_RETURN_IF_NULL(text);
  *out_confirmed_text = text->confirmed_text.c_str();
  return kLiteRtLmStatusOk;
}

LiteRtLmStatusCode litert_omni_text_output_get_unconfirmed_text(
    const LiteRtOmniTextOutput* text, const char** out_unconfirmed_text) {
  LITERT_LM_C_RETURN_IF_NULL(out_unconfirmed_text);
  *out_unconfirmed_text = nullptr;
  LITERT_LM_C_RETURN_IF_NULL(text);
  *out_unconfirmed_text = text->unconfirmed_text.c_str();
  return kLiteRtLmStatusOk;
}

}  // extern "C"

#undef LITERT_OMNI_C_RETURN_IF_ERROR

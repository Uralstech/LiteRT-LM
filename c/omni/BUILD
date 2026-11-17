# Copyright 2026 The ODML Authors.
# Copyright 2026 URAV ADVANCED LEARNING SYSTEMS PRIVATE LIMITED
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "omni_headers",
    hdrs = ["omni.h"],
    deps = [
        "//c:api_export_headers",
        "//c:error_reporter_headers",
    ],
)

OMNI_MODEL_DEPS = [
    "//omni:omni_engine",
    "//omni:omni_registry",
    "//omni:omni_session",
    "//omni/base:text_utils",
    "//omni/speech2text:moonshine",  # build_cleaner: keep
    "//omni/speech2text:parakeet_ctc",  # build_cleaner: keep
    "//omni/speech2text:parakeet_tdt",  # build_cleaner: keep
    "//omni/speech2text:qwen3_asr",  # build_cleaner: keep
    "//omni/speech2text:tinygemma_asr",  # build_cleaner: keep
    "//omni/speech2text:whisper",  # build_cleaner: keep
    "//omni/text2speech:kokoro",  # build_cleaner: keep
    "//omni/text2speech:qwen3_tts",  # build_cleaner: keep
    "//runtime/proto:speech2text_session_config_cc_proto",
    "//runtime/proto:text2speech_metadata_cc_proto",
    "//runtime/proto:text2speech_session_config_cc_proto",
    "//runtime/executor:default_static_gpu_accelerator",
]

cc_library(
    name = "omni_c_api",
    srcs = ["omni.cc"],
    copts = ["-fvisibility=hidden"],
    alwayslink = True,
    deps = [
        ":omni_headers",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "//c:error_reporter_internal",
        "//omni/base:io_types",
    ] + OMNI_MODEL_DEPS,
)

# Standalone Omni C API library. The combined //c:litert-lm-omni target links
# this same API alongside the regular LiteRT-LM C API.
cc_binary(
    name = "litert-omni-tts",
    features = ["-legacy_whole_archive"],
    linkopts = select({
        "@platforms//os:macos": [
            "-Wl,-exported_symbol,_litert_omni_*",
            "-Wl,-exported_symbol,_litert_lm_get_last_error_message",
            "-Wl,-exported_symbol,_litert_lm_clear_last_error",
            "-Wl,-dead_strip",
        ],
        "@platforms//os:ios": [
            "-Wl,-exported_symbol,_litert_omni_*",
            "-Wl,-exported_symbol,_litert_lm_get_last_error_message",
            "-Wl,-exported_symbol,_litert_lm_clear_last_error",
            "-Wl,-dead_strip",
        ],
        "@platforms//os:windows": [],
        "//conditions:default": [
            "-Wl,--no-undefined",
            "-Wl,--strip-all",
            "-Wl,--gc-sections",
            "-Wl,-Bsymbolic",
            "-Wl,-rpath,$$ORIGIN",
        ],
    }),
    linkshared = 1,
    deps = [":omni_c_api", "//c:error_reporter_impl"],
)

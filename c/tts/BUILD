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

package(
    default_hdrs_check = "strict",
    default_visibility = [
        "//visibility:public",
    ],
)

licenses(["notice"])

exports_files([
    "tts_engine.cc",
    "tts_engine.h",
])

ENGINE_COMMON_DEPS = [
    "@com_google_absl//absl/base:log_severity",
    "@com_google_absl//absl/log:absl_log",
    "@com_google_absl//absl/status:statusor",
]

cc_library(
    name = "tts_engine_headers",
    hdrs = ["tts_engine.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "tts_engine_internal",
    hdrs = ["tts_engine_internal.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":tts_engine_headers",
        "//omni/tts:tts_engine",
    ],
)

cc_library(
    name = "tts_engine",
    srcs = ["tts_engine.cc"],
    hdrs = ["tts_engine.h"],
    visibility = ["//visibility:public"],
    deps = ENGINE_COMMON_DEPS + [
        ":tts_engine_internal",
        "//omni/tts:tts_engine",
    ],
    alwayslink = True,
)

cc_binary(
    name = "litert-omni-tts",
    srcs = [
        "tts_engine.cc",
    ],
    copts = ["-fvisibility=hidden"],
    features = ["-legacy_whole_archive"],
    linkopts = select({
        "@platforms//os:macos": [],
        "@platforms//os:ios": [],
        "@platforms//os:windows": [],
        "//conditions:default": [
            # Report unresolved symbol references as errors during linking.
            "-Wl,--no-undefined",
            # Strip all symbols to reduce library size (debug info is removed).
            "-Wl,--strip-all",
            # Remove unused sections/functions to further reduce library size.
            "-Wl,--gc-sections",
            # Ensure symbols are resolved internally first.
            "-Wl,-Bsymbolic",
            # Update the rpath
            "-Wl,-rpath,$$ORIGIN",
        ],
    }),
    linkshared = 1,
    deps = ENGINE_COMMON_DEPS + [
        ":tts_engine_internal",
        ":tts_engine_headers",
        "//omni/tts:tts_engine",
    ] + select({
        "//conditions:default": [],
    }),
)

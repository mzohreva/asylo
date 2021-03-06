#
# Copyright 2018 Asylo authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

licenses(["notice"])  # Apache v2.0

# BUILD file needed to make bzl files available to load.

exports_files([
    "asylo.bzl",
    "asylo_deps.bzl",
    "copts.bzl",
    "dlopen_enclave.bzl",
    "installation_path.bzl",
    "sgx_deps.bzl",
])

load(
    "//asylo/bazel:asylo.bzl",
    "ASYLO_ALL_BACKEND_TAGS",
    "cc_enclave_test",
    "embed_enclaves",
)
load("//asylo/bazel:copts.bzl", "ASYLO_DEFAULT_COPTS")

# This tests that gtest can work inside an enclave.
cc_enclave_test(
    name = "gtest_test",
    srcs = ["gtest_test.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    deps = [
        "@com_google_googletest//:gtest",
    ],
)

# Used to pass configuration from test_shim_loader to test_shim_enclave.
proto_library(
    name = "test_shim_enclave_proto",
    srcs = ["test_shim_enclave.proto"],
    deps = ["//asylo:enclave_proto"],
)

cc_proto_library(
    name = "test_shim_enclave_cc_proto",
    deps = [":test_shim_enclave_proto"],
)

# Used by the cc_enclave_test rule to create enclaves that run gtest test
# suites. Should not typically be used directly.
cc_library(
    name = "test_shim_enclave",
    testonly = 1,
    srcs = ["test_shim_enclave.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    tags = ASYLO_ALL_BACKEND_TAGS,
    visibility = ["//visibility:public"],
    deps = [
        ":test_shim_enclave_cc_proto",
        "//asylo/test/util:enclave_test_application",
        "//asylo/test/util:test_flags",
        "@com_google_absl//absl/flags:flag",
        "@com_google_googletest//:gtest",
    ],
    alwayslink = 1,
)

# Used by the cc_enclave_test rule to execute gtest test suites inside enclaves.
# Should not typically be used directly.
cc_binary(
    name = "test_shim_loader",
    testonly = 1,
    srcs = ["test_shim_loader.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    linkstatic = True,
    visibility = ["//visibility:public"],
    deps = [
        ":test_shim_enclave_cc_proto",
        "//asylo:enclave_cc_proto",
        "//asylo:enclave_client",
        "//asylo/platform/primitives/sgx:loader_cc_proto",
        "//asylo/test/util:test_flags",
        "//asylo/util:logging",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_googletest//:gtest",
    ],
)

EMBED_ENCLAVES_TEST_SECTION = "foo_section"

# A rule to embed an "enclave" (really a text file) in
# embed_enclaves_test_binary_plain.
embed_enclaves(
    name = "embed_enclaves_test_binary",
    elf_file = "//asylo/test/util:test_binary",
    enclaves = {EMBED_ENCLAVES_TEST_SECTION: "//asylo/test/util:sample_text"},
    executable = 1,
)

# A test of embed_enclaves.
sh_test(
    name = "embed_enclaves_test",
    srcs = ["embed_enclaves_test.sh"],
    args = [
        "$(OBJCOPY)",
        "$(location :embed_enclaves_test_binary)",
        EMBED_ENCLAVES_TEST_SECTION,
        "$(location //asylo/test/util:sample_text)",
    ],
    data = [
        ":embed_enclaves_test_binary",
        "//asylo/test/util:sample_text",
    ],
    toolchains = ["@bazel_tools//tools/cpp:current_cc_toolchain"],
    deps = ["//asylo/test/util:shell_testing"],
)

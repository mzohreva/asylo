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

load(
    "//asylo/bazel:asylo.bzl",
    "ASYLO_ALL_BACKEND_TAGS",
    cc_test = "cc_test_and_cc_enclave_test",
    "sgx_enclave_test",
)
load("//asylo/bazel:copts.bzl", "ASYLO_DEFAULT_COPTS")
load("@linux_sgx//:sgx_sdk.bzl", "sgx")

licenses(["notice"])

# asylo POSIX interface.

package(
    default_visibility = ["//asylo:implementation"],
)

# Note: this target cannot be a cc_library because the toolchain itself depends
# on these headers, which creates a circular dependency when cc_library adds a
# dependency on the toolchain.
# Note: the toolchain is entrusted to properly resolve and disambiguate included
# system headers in favor of the ones defined here.
filegroup(
    name = "posix_headers",
    srcs = glob(["include/**/*.h"]),
    visibility = ["//visibility:public"],
)

# Generic target exposing trusted POSIX components selected by build
# configuration.
cc_library(
    name = "trusted_posix",
    copts = ASYLO_DEFAULT_COPTS,
    tags = ASYLO_ALL_BACKEND_TAGS,
    deps = select(
        {
            "@com_google_asylo//asylo": [":posix"],
        },
        no_match_error = "Must be built in the Asylo toolchain",
    ),
)

# A backend agnostic POSIX runtime implementation.
# This can be used by backends requiring posix calls that are platform
# independent, or as a dependency by higher level library like posix, to
# separate backend dependent and backend independent posix calls provided by
# posix library. Expected to be removed if/when all posix calls in posix library
# become backend independent.
cc_library(
    name = "backend_agnostic_posix",
    srcs = [
        "sched.cc",
    ],
    hdrs = [":posix_headers"],
    copts = ASYLO_DEFAULT_COPTS,
    linkstatic = 1,
    tags = ASYLO_ALL_BACKEND_TAGS,
    deps = [
        "//asylo/platform/host_call",
        "//asylo/platform/posix/sockets:backend_agnostic_sockets",
    ],
    alwayslink = 1,
)

cc_library(
    name = "pthread_impl",
    hdrs = ["pthread_impl.h"],
    copts = ASYLO_DEFAULT_COPTS,
    deps = ["//asylo/util:logging"],
)

# POSIX runtime implementation.
_POSIX_SGX_HDRS = ["//asylo/third_party/intel:posix_sgx_headers"]

_POSIX_SGX_DEPS = ["@linux_sgx//:trts"]

cc_library(
    name = "posix",
    srcs = [
        "dirent.cc",
        "dlfcn.cc",
        "endian.cc",
        "epoll.cc",
        "errno.cc",
        "eventfd.cc",
        "file.cc",
        "grp.cc",
        "if.cc",
        "ifaddrs.cc",
        "inotify.cc",
        "ioctl.cc",
        "malloc_lock.cc",
        "mman.cc",
        "nl_types.cc",
        "poll.cc",
        "pthread.cc",
        "pwd.cc",
        "resource.cc",
        "select.cc",
        "signal.cc",
        "stat.cc",
        "statfs.cc",
        "stdio.cc",
        "stdlib.cc",
        "syslog.cc",
        "termios.cc",
        "time.cc",
        "uio.cc",
        "unistd.cc",
        "utime.cc",
        "utsname.cc",
        "wait.cc",
    ],
    hdrs = [
        ":posix_headers",
    ] + select(
        {
            "@linux_sgx//:sgx_hw": _POSIX_SGX_HDRS,
            "@linux_sgx//:sgx_sim": _POSIX_SGX_HDRS,
            "//conditions:default": [],
        },
    ),
    copts = ASYLO_DEFAULT_COPTS,
    linkstatic = 1,
    tags = ASYLO_ALL_BACKEND_TAGS,
    visibility = ["//visibility:private"],
    deps = [
        "@com_google_absl//absl/synchronization",
        "//asylo/util:logging",
        "//asylo/platform/host_call",
        "//asylo/platform/common:time_util",
        "//asylo/platform/core:shared_name",
        "//asylo/platform/core:trusted_core",
        "//asylo/platform/posix/io:io_manager",
        "//asylo/platform/posix/sockets",
        "//asylo/platform/posix/signal:signal_manager",
        "//asylo/platform/posix/threading:thread_manager",
        "//asylo/platform/system",
        "//asylo/util:status",
    ] + select(
        {
            "@linux_sgx//:sgx_hw": _POSIX_SGX_DEPS,
            "@linux_sgx//:sgx_sim": _POSIX_SGX_DEPS,
            "//conditions:default": [],
        },
    ) + select(
        {
            "@com_google_asylo//asylo": [
                # Only trusted architectures should build.
                ":pthread_impl",
                ":backend_agnostic_posix",
                "//asylo/platform/primitives:trusted_backend",
                "//asylo/platform/primitives:trusted_primitives",
                "//asylo/platform/arch:trusted_arch",
            ],
        },
        no_match_error = "Must be built within the Asylo toolchain.",
    ),
    alwayslink = 1,
)

# Test byteswap.h posix extension inside an enclave.
cc_test(
    name = "bswap_test",
    srcs = ["bswap_test.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    deps = [
        "//asylo/test/util:test_main",
        "@com_google_googletest//:gtest",
    ],
)

# Test for endian.h functions.
cc_test(
    name = "endian_test",
    srcs = ["endian_test.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    deps = [
        "//asylo/test/util:test_main",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest",
    ],
)

# Test for errno inside an enclave.
cc_test(
    name = "errno_test",
    srcs = ["errno_test.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    deps = [
        "//asylo/platform/storage/utils:fd_closer",
        "//asylo/test/util:test_flags",
        "//asylo/test/util:test_main",
        "@com_google_absl//absl/flags:flag",
        "@com_google_googletest//:gtest",
    ],
)

# For testing large pipe sizes, we increase the size of heap, since the
# primitives implementation of enc_untrusted_pipe2() duplicates the data
# during serialization.
sgx.enclave_configuration(
    name = "pipe_test_config",
    heap_max_size = "0x200000",
)

cc_test(
    name = "pipe_test",
    srcs = ["pipe_test.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    enclave_test_config = ":pipe_test_config",
    deps = [
        "//asylo/test/util:test_main",
        "//asylo/util:cleanup",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "select_test",
    srcs = ["select_test.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    deps = [
        "//asylo/platform/storage/utils:fd_closer",
        "//asylo/test/util:status_matchers",
        "//asylo/test/util:test_main",
        "//asylo/util:status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
    ],
)

sgx.unsigned_enclave(
    name = "fork_test_unsigned.so",
    srcs = ["fork_test_enclave.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    deps = [
        "//asylo/platform/primitives:trusted_runtime",
        "//asylo/test/util:enclave_test_application",
        "//asylo/util:logging",
        "//asylo/util:status",
    ],
)

sgx.debug_enclave(
    name = "fork_test.so",
    unsigned = "fork_test_unsigned.so",
)

sgx_enclave_test(
    name = "fork_test",
    srcs = ["fork_test_driver.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    enclaves = {"enclave": "fork_test.so"},
    test_args = ["--enclave_path='{enclave}'"],
    deps = [
        "//asylo:enclave_cc_proto",
        "//asylo:enclave_client",
        "//asylo/platform/primitives/sgx:loader_cc_proto",
        "//asylo/test/util:enclave_assertion_authority_configs",
        "//asylo/test/util:enclave_test",
        "//asylo/test/util:status_matchers",
        "//asylo/test/util:test_main",
        "//asylo/util:status",
        "@com_google_absl//absl/flags:flag",
        "@com_google_googletest//:gtest",
    ],
)

proto_library(
    name = "fork_security_test_proto",
    srcs = ["fork_security_test.proto"],
    deps = ["//asylo:enclave_proto"],
)

cc_proto_library(
    name = "fork_security_test_cc_proto",
    deps = [":fork_security_test_proto"],
)

sgx.unsigned_enclave(
    name = "fork_security_test_unsigned.so",
    srcs = ["fork_security_test_enclave.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    deps = [
        ":fork_security_test_cc_proto",
        "//asylo/platform/primitives:trusted_backend",
        "//asylo/platform/primitives/sgx:trusted_fork",
        "//asylo/test/util:enclave_test_application",
    ],
)

sgx.debug_enclave(
    name = "fork_security_test.so",
    unsigned = "fork_security_test_unsigned.so",
)

sgx_enclave_test(
    name = "fork_security_test",
    srcs = ["fork_security_test_driver.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    enclaves = {"enclave": "fork_security_test.so"},
    test_args = ["--enclave_path='{enclave}'"],
    deps = [
        ":fork_security_test_cc_proto",
        "//asylo:enclave_cc_proto",
        "//asylo:enclave_client",
        "//asylo/platform/common:memory",
        "//asylo/platform/core:untrusted_core",
        "//asylo/platform/primitives/sgx:fork_cc_proto",
        "//asylo/platform/primitives/sgx:loader_cc_proto",
        "//asylo/platform/primitives/sgx:untrusted_sgx",
        "//asylo/test/util:status_matchers",
        "//asylo/test/util:test_main",
        "//asylo/util:status",
        "@boringssl//:crypto",
        "@com_google_absl//absl/flags:flag",
        "@com_google_googletest//:gtest",
    ],
)

# A protobuf used by syscalls test. The input contains the target syscall to
# test, and the output contains the output of the syscall inside enclave.
proto_library(
    name = "syscalls_test_proto",
    srcs = ["syscalls_test.proto"],
    deps = ["//asylo:enclave_proto"],
)

cc_proto_library(
    name = "syscalls_test_cc_proto",
    deps = ["syscalls_test_proto"],
)

# SGX enclave used to test different syscalls.
sgx.unsigned_enclave(
    name = "syscalls_test_unsigned.so",
    srcs = ["syscalls_test_enclave.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    deps = [
        ":syscalls_test_cc_proto",
        "//asylo/platform/host_call:serializer_functions",
        "//asylo/platform/primitives:trusted_primitives",
        "//asylo/platform/primitives/util:message_reader_writer",
        "//asylo/platform/primitives/util:status_conversions",
        "//asylo/platform/storage/utils:fd_closer",
        "//asylo/test/util:enclave_test_application",
        "//asylo/util:logging",
        "//asylo/util:status",
        "@boringssl//:crypto",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
    ],
)

sgx.debug_enclave(
    name = "syscalls_test.so",
    unsigned = "syscalls_test_unsigned.so",
)

sgx_enclave_test(
    name = "syscalls_test",
    srcs = ["syscalls_test_driver.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    enclaves = {"enclave": ":syscalls_test.so"},
    test_args = ["--enclave_path='{enclave}'"],
    deps = [
        ":syscalls_test_cc_proto",
        "//asylo:enclave_client",
        "//asylo/platform/host_call:serializer_functions",
        "//asylo/platform/primitives/util:message_reader_writer",
        "//asylo/test/util:enclave_test",
        "//asylo/test/util:status_matchers",
        "//asylo/test/util:test_main",
        "//asylo/util:logging",
        "//asylo/util:status",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "times_test",
    srcs = ["times_test.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    deps = [
        "//asylo/test/util:test_main",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "syslog_test",
    srcs = ["syslog_test.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    deps = [
        "//asylo/test/util:test_main",
        "@com_google_googletest//:gtest",
    ],
)

proto_library(
    name = "in6_macro_test_proto",
    srcs = ["in6_macro_test.proto"],
    deps = ["//asylo:enclave_proto"],
)

cc_proto_library(
    name = "in6_macro_test_cc_proto",
    deps = ["in6_macro_test_proto"],
)

sgx.unsigned_enclave(
    name = "in6_macro_test_unsigned.so",
    srcs = ["in6_macro_test_enclave.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    deps = [
        ":in6_macro_test_cc_proto",
        "//asylo:enclave_runtime",
        "//asylo/util:status",
    ],
)

sgx.debug_enclave(
    name = "in6_macro_test.so",
    unsigned = "in6_macro_test_unsigned.so",
)

sgx_enclave_test(
    name = "in6_macro_test",
    srcs = ["in6_macro_test_loader.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    enclaves = {"enclave": ":in6_macro_test.so"},
    test_args = ["--enclave_path='{enclave}'"],
    deps = [
        ":in6_macro_test_cc_proto",
        "//asylo/test/util:enclave_test",
        "//asylo/test/util:status_matchers",
        "//asylo/test/util:test_main",
        "//asylo/util:status",
        "@com_google_googletest//:gtest",
    ],
)

# Testing the enclave clock vs the posix clock.
proto_library(
    name = "clock_time_test_proto",
    srcs = ["clock_time_test.proto"],
    deps = ["//asylo:enclave_proto"],
)

cc_proto_library(
    name = "clock_time_test_cc_proto",
    deps = ["clock_time_test_proto"],
)

sgx.unsigned_enclave(
    name = "clock_time_test_unsigned.so",
    srcs = ["clock_time_test_enclave.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    deps = [
        ":clock_time_test_cc_proto",
        ":posix",
        "//asylo:enclave_runtime",
        "//asylo/util:status",
    ],
)

sgx.debug_enclave(
    name = "clock_time_test.so",
    unsigned = "clock_time_test_unsigned.so",
)

sgx_enclave_test(
    name = "clock_time_test",
    srcs = ["clock_time_test_loader.cc"],
    copts = ASYLO_DEFAULT_COPTS,
    enclaves = {"enclave": "clock_time_test.so"},
    test_args = ["--enclave_path='{enclave}'"],
    deps = [
        ":clock_time_test_cc_proto",
        "//asylo/test/util:enclave_test",
        "//asylo/test/util:status_matchers",
        "//asylo/test/util:test_main",
        "//asylo/util:status",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest",
    ],
)

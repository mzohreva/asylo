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

load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
load("@com_google_asylo_backend_provider//:enclave_info.bzl", "asylo_backend")

licenses(["notice"])

# BUILD file needed make enclave_info.bzl available to load.

exports_files(["enclave_info.bzl", "transitions.bzl"])

package_group(
    name = "implementation",
    packages = ["//..."],
)

# Placeholder label for "no Asylo backend selected". At least transitionally,
# we cannot set a real backend label as default.
asylo_backend(
    name = "none",
    visibility = ["//visibility:public"],
)

# A configurable build setting that selects which Asylo backend is in use. This
# allows enclaves to be defined in more backend-agnostic ways, and to use
# conditional compilation in the cases that are platform-specific.
# The flag can be set with a rule (e.g., transitions.backend_binary), or from
# the top level with a flag like so:
#
#   --@com_google_asylo_backend_provider//:backend=@linux_sgx//:asylo_sgx_sim
label_flag(
    name = "backend",
    # The default backend is "none" to avoid a default of asylo_sgx_sim causing
    # ambiguous matches with --define=SGX_SIM=1 while those are still in use.
    # When they are removed, we'll likely default to the SGX simulator backend,
    # since a non-backend default causes most cc_library definitions to
    # awkwardly fail. We may want to revisit this when platforms on each rule
    # are fully supported.
    build_setting_default = "@com_google_asylo_backend_provider//:none",
    visibility = ["//visibility:public"],
)

bzl_library(
    name = "enclave_info_bzl",
    srcs = ["enclave_info.bzl"],
    visibility = ["//visibility:public"],
)

bzl_library(
    name = "transitions_bzl",
    srcs = ["transitions.bzl"],
    visibility = ["//visibility:public"],
)

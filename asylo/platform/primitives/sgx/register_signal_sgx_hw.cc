/*
 *
 * Copyright 2018 Asylo authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "asylo/platform/primitives/trusted_runtime.h"

#include "asylo/platform/primitives/sgx/trusted_sgx.h"

extern "C" int enc_register_signal(int signum, const sigset_t mask, int flags,
                                   const char *enclave_name) {
  return asylo::primitives::RegisterSignalHandler(
      signum, /*bridge_sigaction=*/nullptr, mask, flags, enclave_name);
}

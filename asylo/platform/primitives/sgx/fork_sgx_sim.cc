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

#include "asylo/platform/primitives/sgx/fork_internal.h"

#include <stdlib.h>

#include "asylo/platform/primitives/sgx/trusted_sgx.h"
#include "asylo/platform/primitives/trusted_runtime.h"
#include "asylo/util/status.h"

namespace asylo {

bool IsSecureForkSupported() { return false; }

Status TakeSnapshotForFork(SnapshotLayout *snapshot_layout) {
  // Only supported in the SGX hardware backend.
  abort();
}

Status RestoreForFork(const char *input, size_t input_len) {
  // Only supported in the SGX hardware backend.
  abort();
}

Status TransferSecureSnapshotKey(
    const ForkHandshakeConfig &fork_handshake_config) {
  // Only supported in the SGX hardware backend.
  abort();
}

void SaveThreadLayoutForSnapshot() {
  // Only supported in the SGX hardware backend.
  abort();
}

void SetForkRequested() {
  // Only supported in the SGX hardware backend.
  abort();
}

pid_t enc_fork(const char *enclave_name) {
  return asylo::primitives::InvokeFork(
      enclave_name, /*restore_snapshot=*/false);
}

}  // namespace asylo

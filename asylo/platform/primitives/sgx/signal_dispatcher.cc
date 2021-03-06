/*
 *
 * Copyright 2019 Asylo authors
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

#include "asylo/platform/primitives/sgx/signal_dispatcher.h"

#include <signal.h>

#include "absl/strings/str_cat.h"
#include "asylo/enclave.pb.h"
#include "asylo/platform/primitives/sgx/untrusted_sgx.h"
#include "asylo/util/status.h"
#include "asylo/util/status_macros.h"
#include "asylo/util/statusor.h"

namespace asylo {
namespace primitives {

EnclaveSignalDispatcher *EnclaveSignalDispatcher::GetInstance() {
  static EnclaveSignalDispatcher *instance = new EnclaveSignalDispatcher();
  return instance;
}

SgxEnclaveClient *EnclaveSignalDispatcher::GetClientForSignal(
    int signum) const {
  absl::MutexLock lock(&signal_enclave_map_lock_);
  auto it = signal_to_client_map_.find(signum);
  if (it == signal_to_client_map_.end()) {
    return nullptr;
  }
  return it->second;
}

const SgxEnclaveClient *EnclaveSignalDispatcher::RegisterSignal(
    int signum, SgxEnclaveClient *client) {
  // Block all signals when registering a signal handler to avoid deadlock.
  sigset_t mask, oldmask;
  sigfillset(&mask);
  sigprocmask(SIG_SETMASK, &mask, &oldmask);
  SgxEnclaveClient *old_client = nullptr;
  {
    absl::MutexLock lock(&signal_enclave_map_lock_);
    // If this signal is registered by another enclave, deregister it first.
    auto client_iterator = signal_to_client_map_.find(signum);
    if (client_iterator != signal_to_client_map_.end()) {
      old_client = client_iterator->second;
    }
    signal_to_client_map_[signum] = client;
  }
  // Set the signal mask back to the original one to unblock the signals.
  sigprocmask(SIG_SETMASK, &oldmask, nullptr);
  return old_client;
}

Status EnclaveSignalDispatcher::DeregisterAllSignalsForClient(
    SgxEnclaveClient *client) {
  sigset_t mask, oldmask;
  sigfillset(&mask);
  sigprocmask(SIG_SETMASK, &mask, &oldmask);
  Status status = Status::OkStatus();
  {
    absl::MutexLock lock(&signal_enclave_map_lock_);
    // If this enclave has registered any signals, deregister them and set the
    // signal handler to the default one.
    for (auto iterator = signal_to_client_map_.begin();
         iterator != signal_to_client_map_.end();) {
      if (iterator->second == client) {
        if (signal(iterator->first, SIG_DFL) == SIG_ERR) {
          status = Status(
              error::GoogleError::INVALID_ARGUMENT,
              absl::StrCat(
                  "Failed to deregister one or more handlers for signal: ",
                  iterator->first));
        }
        auto saved_iterator = iterator;
        ++iterator;
        signal_to_client_map_.erase(saved_iterator);
      } else {
        ++iterator;
      }
    }
  }
  sigprocmask(SIG_SETMASK, &oldmask, nullptr);
  return status;
}

int EnclaveSignalDispatcher::EnterEnclaveAndHandleSignal(int signum,
                                                         siginfo_t *info,
                                                         void *ucontext) {
  SgxEnclaveClient *client = GetClientForSignal(signum);
  if (!client) {
    return -1;
  }
  EnclaveSignal enclave_signal;
  enclave_signal.set_signum(signum);
  enclave_signal.set_code(info->si_code);
  return client->EnterAndHandleSignal(enclave_signal);
}

}  // namespace primitives
}  // namespace asylo

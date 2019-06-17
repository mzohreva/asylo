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

#include "asylo/trusted_application.h"

namespace asylo {

class FailFinalizeEnclave : public asylo::TrustedApplication {
 public:
  asylo::Status Finalize(const asylo::EnclaveFinal& enclave_final) override {
    // Finalize returns a non-OK status.
    return asylo::Status(asylo::error::GoogleError::INTERNAL, "Non-ok status");
  }
};

TrustedApplication* BuildTrustedApplication() {
  return new FailFinalizeEnclave;
}

}  // namespace asylo

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

#ifndef ASYLO_IDENTITY_SGX_PCK_CERTIFICATES_H_
#define ASYLO_IDENTITY_SGX_PCK_CERTIFICATES_H_

#include "absl/types/optional.h"
#include "asylo/crypto/asn1.h"
#include "asylo/identity/sgx/machine_configuration.pb.h"
#include "asylo/identity/sgx/pck_certificates.pb.h"
#include "asylo/identity/sgx/platform_provisioning.pb.h"
#include "asylo/identity/sgx/tcb.pb.h"
#include "asylo/util/status.h"
#include "asylo/util/statusor.h"

namespace asylo {
namespace sgx {

// The data embedded in the SGX-specific X.509 extensions for PCK certificates.
struct SgxExtensions {

  // The PPID of the platform.
  Ppid ppid;

  // The TCB specified in the PCK certificate.
  Tcb tcb;

  // The CPU SVN specified in the PCK certificate.
  CpuSvn cpu_svn;

  // The PCE ID of the platform.
  PceId pce_id;

  // The FMSPC of the platform.
  Fmspc fmspc;

  // The SGX type of the platform.
  SgxType sgx_type;
};

// Returns the top-level OBJECT IDENTIFIER for SGX extensions.
const ObjectId &GetSgxExtensionsOid();

// Reads the SGX-specific extension data in |extensions_asn1|.
StatusOr<SgxExtensions> ReadSgxExtensions(const Asn1Value &extensions_asn1);

// Writes the SGX-specific extension data in |extensions| to an Asn1Value. This
// is only intended to be used for testing.
StatusOr<Asn1Value> WriteSgxExtensions(const SgxExtensions &extensions);

// Validates a PckCertificates message. Returns an OK status if and only if the
// message is valid.
//
// A PckCertificates message is valid if and only if:
//
//   * Each of its |certs| satisfies the following:
//
//     * Its |tcb_level|, |tcbm|, and |cert| fields are all present.
//     * All of those fields are valid.
//     * The PCE SVNs in its |tcb_level| and |tcbm| are the same.
//
//   * There are no two distinct, unequal |certs| with identical |tcb_level|s or
//     |tcbm|s.
Status ValidatePckCertificates(const PckCertificates &pck_certificates);

}  // namespace sgx
}  // namespace asylo

#endif  // ASYLO_IDENTITY_SGX_PCK_CERTIFICATES_H_

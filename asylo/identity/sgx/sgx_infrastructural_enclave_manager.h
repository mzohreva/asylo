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

#ifndef ASYLO_IDENTITY_SGX_SGX_INFRASTRUCTURAL_ENCLAVE_MANAGER_H_
#define ASYLO_IDENTITY_SGX_SGX_INFRASTRUCTURAL_ENCLAVE_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "asylo/client.h"
#include "asylo/crypto/certificate.pb.h"
#include "asylo/crypto/keys.pb.h"
#include "asylo/identity/sealed_secret.pb.h"
#include "asylo/identity/sgx/intel_architectural_enclave_interface.h"
#include "asylo/identity/sgx/machine_configuration.pb.h"
#include "asylo/identity/sgx/platform_provisioning.pb.h"
#include "asylo/identity/sgx/remote_assertion_generator_enclave.pb.h"
#include "asylo/identity/sgx/sgx_identity.pb.h"
#include "asylo/util/status.h"
#include "asylo/util/statusor.h"

namespace asylo {

// Manages infrastructural enclaves on an SGX node. This includes the Assertion
// Generator Enclave and Intel architectural enclaves.
//
// This class provides a method corresponding to each entry-point exposed by the
// managed enclaves.
//
// Note that this class has no relation to asylo::EnclaveManager.
class SgxInfrastructuralEnclaveManager {
 public:
  // Creates a manager that invokes Intel architectural enclaves via
  // |intel_ae_interface| and invokes the Assertion Generator Enclave via
  // |assertion_generator_enclave|. |assertion_generator_enclave| must be valid
  // for the lifetime of the manager.
  SgxInfrastructuralEnclaveManager(
      std::unique_ptr<sgx::IntelArchitecturalEnclaveInterface>
          intel_ae_interface,
      EnclaveClient *assertion_generator_enclave);

  virtual ~SgxInfrastructuralEnclaveManager() = default;

  /////////////////////////////////////////////
  //    Assertion Generator Enclave (AGE)    //
  /////////////////////////////////////////////

  // Requests the AGE to generate a new attestation key, and a CSR for each CA
  // listed in |target_certificate_authorities|.
  virtual Status AgeGenerateKeyAndCsr(
      const sgx::TargetInfoProto &pce_target_info, sgx::ReportProto *report,
      std::string *pce_sign_report_payload,
      sgx::TargetedCertificateSigningRequest *targeted_csr);

  // Requests the AGE to generate a hardware REPORT for the PCE's GetPceInfo
  // protocol. The REPORT is targeted at |pce_target_info| and is bound to
  // |ppid_encryption_key|.
  virtual StatusOr<sgx::ReportProto> AgeGeneratePceInfoSgxHardwareReport(
      const sgx::TargetInfoProto &pce_target_info,
      const asylo::AsymmetricEncryptionKeyProto &ppid_encryption_key);

  // Updates the AGE's attestation key certificates to the provided
  // |cert_chains|. On success, returns a sealed secret containing the AGE's
  // attestation key and associated certificates.
  virtual StatusOr<SealedSecret> AgeUpdateCerts(
      const std::vector<asylo::CertificateChain> &cert_chains);

  // Starts the AGE's remote assertion generator server with the enclave's
  // current state.
  virtual Status AgeStartServer();

  // Starts the AGE's remote assertion generator server using the provided
  // |secret|.
  virtual Status AgeStartServer(const asylo::SealedSecret &secret);

  // Retrieves the AGE's SgxIdentity.
  virtual StatusOr<SgxIdentity> AgeGetSgxIdentity();

  ////////////////////////////////////////////////////
  //    Provisioning Certification Enclave (PCE)    //
  ////////////////////////////////////////////////////

  // Sets |pce_target_info| to a TargetInfoProto describing the PCE and
  // |pce_svn| to a PceSvn containing the PCE's ISV SVN value.
  virtual Status PceGetTargetInfo(sgx::TargetInfoProto *pce_target_info,
                                  sgx::PceSvn *pce_svn);

  // Retrieves information about the PCE, including its ISV SVN, PCE ID, the PCK
  // signature scheme, and the PPID encrypted with |ppidek|.
  //
  // The given |report_proto| must contain a REPORT that is bound to |ppidek|.
  // See IntelArchitecturalEnclaveInterface::GetPceInfo for more information on
  // the requirements around the REPORT.
  virtual Status PceGetInfo(const sgx::ReportProto &report_proto,
                            const asylo::AsymmetricEncryptionKeyProto &ppidek,
                            sgx::PceSvn *pce_svn, sgx::PceId *pce_id,
                            asylo::SignatureScheme *pck_signature_scheme,
                            std::string *encrypted_ppid);

  // Requests the PCE to sign the given REPORT contained in |report_proto| using
  // the Provisioning Certification Key (PCK) derived from |pck_target_pce_svn|
  // and |pck_target_cpu_svn|. On success, returns the PCK signature.
  virtual StatusOr<Signature> PceSignReport(
      const sgx::PceSvn &pck_target_pce_svn,
      const sgx::CpuSvn &pck_target_cpu_svn,
      const sgx::ReportProto &report_proto);

 protected:
  // This constructor is required for a mock object.
  SgxInfrastructuralEnclaveManager() = default;

 private:
  // Used to invoke operations on Intel architectural enclaves.
  std::unique_ptr<sgx::IntelArchitecturalEnclaveInterface> intel_ae_interface_;

  // Used to invoke operations on the Assertion Generator Enclave.
  EnclaveClient *assertion_generator_enclave_;
};

}  // namespace asylo

#endif  // ASYLO_IDENTITY_SGX_SGX_INFRASTRUCTURAL_ENCLAVE_MANAGER_H_

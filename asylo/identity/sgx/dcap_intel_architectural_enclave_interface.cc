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

#include "asylo/identity/sgx/dcap_intel_architectural_enclave_interface.h"

#include <memory>
#include <vector>

#include "absl/strings/str_cat.h"
#include "asylo/crypto/algorithms.pb.h"
#include "asylo/identity/sgx/intel_architectural_enclave_interface.h"
#include "asylo/identity/sgx/pce_util.h"
#include "asylo/util/status.h"
#include "asylo/util/status_macros.h"
#include "asylo/util/statusor.h"
#include "include/sgx_report.h"
#include "QuoteGeneration/pce_wrapper/inc/sgx_pce.h"
#include "QuoteGeneration/quote_wrapper/common/inc/sgx_ql_lib_common.h"
#include "QuoteGeneration/quote_wrapper/ql/inc/sgx_dcap_ql_wrapper.h"

namespace asylo {
namespace sgx {
namespace {

Status PceErrorToStatus(sgx_pce_error_t pce_error) {
  switch (pce_error) {
    case SGX_PCE_SUCCESS:
      return Status::OkStatus();
    case SGX_PCE_UNEXPECTED:
      return Status(error::GoogleError::INTERNAL, "Unexpected error");
    case SGX_PCE_OUT_OF_EPC:
      return Status(error::GoogleError::INTERNAL,
                    "Not enough EPC to load the PCE");
    case SGX_PCE_INTERFACE_UNAVAILABLE:
      return Status(error::GoogleError::INTERNAL, "Interface unavailable");
    case SGX_PCE_CRYPTO_ERROR:
      return Status(error::GoogleError::INTERNAL,
                    "Evaluation of REPORT.REPORTDATA failed");
    case SGX_PCE_INVALID_PARAMETER:
      return Status(error::GoogleError::INVALID_ARGUMENT, "Invalid parameter");
    case SGX_PCE_INVALID_REPORT:
      return Status(error::GoogleError::INVALID_ARGUMENT, "Invalid report");
    case SGX_PCE_INVALID_TCB:
      return Status(error::GoogleError::INVALID_ARGUMENT, "Invalid TCB");
    case SGX_PCE_INVALID_PRIVILEGE:
      return Status(error::GoogleError::PERMISSION_DENIED,
                    "Report must have the PROVISION_KEY attribute bit set");
    default:
      return Status(error::GoogleError::UNKNOWN, "Unknown error");
  }
}

Status Quote3ErrorToStatus(quote3_error_t quote3_error) {
  switch (quote3_error) {
    case SGX_QL_SUCCESS:
      return Status::OkStatus();
    case SGX_QL_ERROR_UNEXPECTED:
      return Status(error::GoogleError::INTERNAL, "Unexpected error");
    case SGX_QL_ERROR_INVALID_PARAMETER:
      return Status(error::GoogleError::INVALID_ARGUMENT, "Invalid parameter");
    case SGX_QL_ERROR_OUT_OF_MEMORY:
      return Status(error::GoogleError::RESOURCE_EXHAUSTED, "Out of memory");
    case SGX_QL_ERROR_ECDSA_ID_MISMATCH:
      return Status(error::GoogleError::INTERNAL,
                    "Unexpected ID in the ECDSA key blob");
    case SGX_QL_PATHNAME_BUFFER_OVERFLOW_ERROR:
      return Status(error::GoogleError::OUT_OF_RANGE,
                    "Pathname buffer overflow");
    case SGX_QL_FILE_ACCESS_ERROR:
      return Status(error::GoogleError::INTERNAL, "File access error");
    case SGX_QL_ERROR_STORED_KEY:
      return Status(error::GoogleError::INTERNAL, "Invalid cached ECDSA key");
    case SGX_QL_ERROR_PUB_KEY_ID_MISMATCH:
      return Status(error::GoogleError::INTERNAL,
                    "Cached ECDSA key ID does not match request");
    case SGX_QL_ERROR_INVALID_PCE_SIG_SCHEME:
      return Status(error::GoogleError::INTERNAL,
                    "The signature scheme supported by the PCE is not "
                    "supported by the QE");
    case SGX_QL_ATT_KEY_BLOB_ERROR:
      return Status(error::GoogleError::INTERNAL, "Attestation key blob error");
    case SGX_QL_UNSUPPORTED_ATT_KEY_ID:
      return Status(error::GoogleError::INTERNAL, "Invalid attestation key ID");
    case SGX_QL_UNSUPPORTED_LOADING_POLICY:
      return Status(error::GoogleError::INTERNAL,
                    "Unsupported enclave loading policy");
    case SGX_QL_INTERFACE_UNAVAILABLE:
      return Status(error::GoogleError::INTERNAL,
                    "Unable to load the quoting enclave");
    case SGX_QL_PLATFORM_LIB_UNAVAILABLE:
      return Status(
          error::GoogleError::INTERNAL,
          "Unable to load the platform quote provider library (not fatal)");
    case SGX_QL_ATT_KEY_NOT_INITIALIZED:
      return Status(error::GoogleError::FAILED_PRECONDITION,
                    "Attestation key not initialized");
    case SGX_QL_ATT_KEY_CERT_DATA_INVALID:
      return Status(error::GoogleError::INTERNAL,
                    "Invalid attestation key certification retrieved from "
                    "platform quote provider library");
    case SGX_QL_NO_PLATFORM_CERT_DATA:
      return Status(error::GoogleError::INTERNAL,
                    "No certification for the platform could be found");
    case SGX_QL_OUT_OF_EPC:
      return Status(error::GoogleError::RESOURCE_EXHAUSTED,
                    "Insufficient EPC memory to load an enclave");
    case SGX_QL_ERROR_REPORT:
      return Status(error::GoogleError::INTERNAL,
                    "An error occurred validating the report");
    case SGX_QL_ENCLAVE_LOST:
      return Status(error::GoogleError::INTERNAL,
                    "The enclave was lost due to power transition or fork()");
    case SGX_QL_INVALID_REPORT:
      return Status(error::GoogleError::INVALID_ARGUMENT,
                    "The application enclave's report failed validation");
    case SGX_QL_ENCLAVE_LOAD_ERROR:
      return Status(error::GoogleError::INTERNAL, "Unable to load an enclave");
    case SGX_QL_UNABLE_TO_GENERATE_QE_REPORT:
      return Status(
          error::GoogleError::INTERNAL,
          "Unable to generate QE report targeting the application enclave");
    case SGX_QL_KEY_CERTIFCATION_ERROR:
      return Status(
          error::GoogleError::INTERNAL,
          "The platform quote provider library returned an invalid TCB");
    case SGX_QL_NETWORK_ERROR:
      return Status(error::GoogleError::INTERNAL,
                    "Network error getting PCK certificates");
    case SGX_QL_MESSAGE_ERROR:
      return Status(error::GoogleError::INTERNAL,
                    "Protocol error getting PCK certificates");
    case SGX_QL_ERROR_INVALID_PRIVILEGE:
      return Status(error::GoogleError::PERMISSION_DENIED,
                    "Invalid permission");
    default:
      return Status(error::GoogleError::UNKNOWN, "Unknown error");
  }
}

// The wrapper functions here are never unit tested, and should thus be simple
// pass-throughs with no additional functionality beyond what Intel's API
// provides. All additional functionality must be put into
// DcapIntelArchitecturalEnclaveInterface so that it may be unit tested.
class DefaultDcapLibraryInterface
    : public DcapIntelArchitecturalEnclaveInterface::DcapLibraryInterface {
 public:
  ~DefaultDcapLibraryInterface() override = default;

  quote3_error_t qe_set_enclave_dirpath(const char *dirpath) const override {
    return sgx_qe_set_enclave_dirpath(dirpath);
  }

  sgx_pce_error_t pce_get_target(sgx_target_info_t *p_pce_target,
                                 sgx_isv_svn_t *p_pce_isv_svn) const override {
    return sgx_pce_get_target(p_pce_target, p_pce_isv_svn);
  };

  sgx_pce_error_t get_pce_info(const sgx_report_t *p_report,
                               const uint8_t *p_pek, uint32_t pek_size,
                               uint8_t crypto_suite, uint8_t *p_encrypted_ppid,
                               uint32_t encrypted_ppid_size,
                               uint32_t *p_encrypted_ppid_out_size,
                               sgx_isv_svn_t *p_pce_isvsvn, uint16_t *p_pce_id,
                               uint8_t *p_signature_scheme) const override {
    return sgx_get_pce_info(p_report, p_pek, pek_size, crypto_suite,
                            p_encrypted_ppid, encrypted_ppid_size,
                            p_encrypted_ppid_out_size, p_pce_isvsvn, p_pce_id,
                            p_signature_scheme);
  }

  sgx_pce_error_t pce_sign_report(
      const sgx_isv_svn_t *isv_svn, const sgx_cpu_svn_t *cpu_svn,
      const sgx_report_t *p_report, uint8_t *p_signature,
      uint32_t signature_buf_size,
      uint32_t *p_signature_out_size) const override {
    return sgx_pce_sign_report(isv_svn, cpu_svn, p_report, p_signature,
                               signature_buf_size, p_signature_out_size);
  }

  quote3_error_t qe_get_target_info(
      sgx_target_info_t *p_qe_target_info) const override {
    return sgx_qe_get_target_info(p_qe_target_info);
  }

  quote3_error_t qe_get_quote_size(uint32_t *p_quote_size) const override {
    return sgx_qe_get_quote_size(p_quote_size);
  }

  quote3_error_t qe_get_quote(const sgx_report_t *p_app_report,
                              uint32_t quote_size,
                              uint8_t *p_quote) const override {
    return sgx_qe_get_quote(p_app_report, quote_size, p_quote);
  }
};

}  // namespace

DcapIntelArchitecturalEnclaveInterface::DcapIntelArchitecturalEnclaveInterface()
    : DcapIntelArchitecturalEnclaveInterface(
          absl::make_unique<DefaultDcapLibraryInterface>()) {}

DcapIntelArchitecturalEnclaveInterface::DcapIntelArchitecturalEnclaveInterface(
    std::unique_ptr<DcapLibraryInterface> dcap_library)
    : dcap_library_(std::move(dcap_library)) {}

Status DcapIntelArchitecturalEnclaveInterface::SetEnclaveDir(
    const std::string &path) {
  return Quote3ErrorToStatus(
      dcap_library_->qe_set_enclave_dirpath(path.c_str()));
}

Status DcapIntelArchitecturalEnclaveInterface::GetPceTargetinfo(
    Targetinfo *targetinfo, uint16_t *pce_svn) {
  static_assert(
      sizeof(Targetinfo) == sizeof(sgx_target_info_t),
      "Targetinfo struct is not the same size as sgx_target_info_t struct");

  sgx_pce_error_t result = dcap_library_->pce_get_target(
      reinterpret_cast<sgx_target_info_t *>(targetinfo), pce_svn);

  return PceErrorToStatus(result);
}

Status DcapIntelArchitecturalEnclaveInterface::GetPceInfo(
    const Report &report, absl::Span<const uint8_t> ppid_encryption_key,
    AsymmetricEncryptionScheme ppid_encryption_scheme,
    std::string *ppid_encrypted, uint16_t *pce_svn, uint16_t *pce_id,
    SignatureScheme *signature_scheme) {
  static_assert(sizeof(Report) == sizeof(sgx_report_t),
                "Report struct is not the same size as sgx_report_t struct");

  absl::optional<uint8_t> crypto_suite =
      AsymmetricEncryptionSchemeToPceCryptoSuite(ppid_encryption_scheme);
  if (!crypto_suite.has_value()) {
    return Status(
        error::GoogleError::INVALID_ARGUMENT,
        absl::StrCat("Invalid ppid_encryption_scheme: ",
                     AsymmetricEncryptionScheme_Name(ppid_encryption_scheme)));
  }

  uint32_t max_ppid_out_size;
  ASYLO_ASSIGN_OR_RETURN(max_ppid_out_size,
                         GetEncryptedDataSize(ppid_encryption_scheme));

  std::vector<uint8_t> ppid_encrypted_tmp(max_ppid_out_size);
  uint32_t encrypted_ppid_out_size = 0;
  uint8_t pce_signature_scheme;
  sgx_pce_error_t result = dcap_library_->get_pce_info(
      reinterpret_cast<const sgx_report_t *>(&report),
      ppid_encryption_key.data(), ppid_encryption_key.size(),
      crypto_suite.value(), ppid_encrypted_tmp.data(),
      ppid_encrypted_tmp.size(), &encrypted_ppid_out_size, pce_svn, pce_id,
      &pce_signature_scheme);
  if (result == SGX_PCE_SUCCESS) {
    ppid_encrypted->assign(
        ppid_encrypted_tmp.begin(),
        ppid_encrypted_tmp.begin() + encrypted_ppid_out_size);
    *signature_scheme =
        PceSignatureSchemeToSignatureScheme(pce_signature_scheme);
  }

  return PceErrorToStatus(result);
}

Status DcapIntelArchitecturalEnclaveInterface::PceSignReport(
    const Report &report, uint16_t target_pce_svn,
    UnsafeBytes<kCpusvnSize> target_cpu_svn, std::string *signature) {
  static_assert(sizeof(target_cpu_svn) == sizeof(sgx_cpu_svn_t),
                "target_cpusvn is not the same size as sgx_cpu_svn_t struct");

  std::vector<uint8_t> signature_tmp(kEcdsaP256SignatureSize);
  uint32_t signature_out_size = 0;
  sgx_pce_error_t result = dcap_library_->pce_sign_report(
      &target_pce_svn, reinterpret_cast<sgx_cpu_svn_t *>(&target_cpu_svn),
      reinterpret_cast<const sgx_report_t *>(&report), signature_tmp.data(),
      signature_tmp.size(), &signature_out_size);
  if (result == SGX_PCE_SUCCESS) {
    signature->assign(signature_tmp.begin(),
                      signature_tmp.begin() + signature_out_size);
  }

  return PceErrorToStatus(result);
}

StatusOr<Targetinfo> DcapIntelArchitecturalEnclaveInterface::GetQeTargetinfo() {
  Targetinfo target_info;
  quote3_error_t result = dcap_library_->qe_get_target_info(
      reinterpret_cast<sgx_target_info_t *>(&target_info));
  if (result == SGX_QL_SUCCESS) {
    return target_info;
  }
  return Quote3ErrorToStatus(result);
}

StatusOr<std::vector<uint8_t>>
DcapIntelArchitecturalEnclaveInterface::GetQeQuote(const Report &report) {
  uint32_t quote_size;
  quote3_error_t result = dcap_library_->qe_get_quote_size(&quote_size);
  if (result != SGX_QL_SUCCESS) {
    return Quote3ErrorToStatus(result);
  }

  std::vector<uint8_t> quote(quote_size);
  result = dcap_library_->qe_get_quote(
      reinterpret_cast<const sgx_report_t *>(&report), quote_size,
      quote.data());
  if (result != SGX_QL_SUCCESS) {
    return Quote3ErrorToStatus(result);
  }

  return quote;
}

}  // namespace sgx
}  // namespace asylo

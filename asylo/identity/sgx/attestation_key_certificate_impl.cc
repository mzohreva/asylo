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

#include "asylo/identity/sgx/attestation_key_certificate_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "asylo/crypto/algorithms.pb.h"
#include "asylo/crypto/ecdsa_p256_sha256_signing_key.h"
#include "asylo/crypto/keys.pb.h"
#include "asylo/crypto/signing_key.h"
#include "asylo/crypto/util/byte_container_view.h"
#include "asylo/crypto/util/trivial_object_util.h"
#include "asylo/identity/additional_authenticated_data_generator.h"
#include "asylo/identity/sgx/attestation_key.pb.h"
#include "asylo/identity/sgx/identity_key_management_structs.h"
#include "asylo/identity/sgx/platform_provisioning.h"
#include "asylo/identity/sgx/remote_assertion_generator_constants.h"
#include "asylo/identity/sgx/sgx_identity_util_internal.h"
#include "asylo/util/status_macros.h"
#include "asylo/util/statusor.h"

namespace asylo {
namespace sgx {
namespace {

constexpr SignatureScheme kExpectedSignatureScheme = ECDSA_P256_SHA256;

StatusOr<std::unique_ptr<VerifyingKey>> CreateVerifyingKey(
    const CertificateInterface &issuer_cert, SignatureScheme signature_scheme) {
  std::string issuer_subject_key;
  ASYLO_ASSIGN_OR_RETURN(issuer_subject_key, issuer_cert.SubjectKeyDer());
  switch (signature_scheme) {
    case ECDSA_P256_SHA256:
      return EcdsaP256Sha256VerifyingKey::CreateFromDer(issuer_subject_key);
    case UNKNOWN_SIGNATURE_SCHEME:
      break;
  }
  return Status(error::GoogleError::INVALID_ARGUMENT,
                absl::StrCat("Signature scheme unsupported: ",
                             SignatureScheme_Name(signature_scheme)));
}

StatusOr<std::string> VerifyingKeyToDer(
    AsymmetricSigningKeyProto asymmetric_key) {
  if (asymmetric_key.key_type() != AsymmetricSigningKeyProto::VERIFYING_KEY) {
    return Status(
        error::GoogleError::INVALID_ARGUMENT,
        absl::StrFormat(
            "Key type of the attestation public key (%s) does not match "
            "expected key type (VERIFYING_KEY)",
            AsymmetricSigningKeyProto_KeyType_Name(asymmetric_key.key_type())));
  }

  switch (asymmetric_key.encoding()) {
    case ASYMMETRIC_KEY_DER:
      return asymmetric_key.key();
    case ASYMMETRIC_KEY_PEM:
      switch (asymmetric_key.signature_scheme()) {
        case ECDSA_P256_SHA256: {
          std::unique_ptr<VerifyingKey> verifying_key;
          ASYLO_ASSIGN_OR_RETURN(
              verifying_key,
              EcdsaP256Sha256VerifyingKey::CreateFromPem(asymmetric_key.key()));
          return verifying_key->SerializeToDer();
        }
        case UNKNOWN_SIGNATURE_SCHEME:
          return Status(
              error::GoogleError::INVALID_ARGUMENT,
              "Could not DER encode a key with an unknown signature scheme");
      }
    case UNKNOWN_ASYMMETRIC_KEY_ENCODING:
      break;
  }
  return Status(error::GoogleError::INTERNAL,
                "Asymmetric key encoding unknown");
}

}  // namespace

StatusOr<std::unique_ptr<AttestationKeyCertificateImpl>>
AttestationKeyCertificateImpl::Create(const Certificate &certificate) {
  if (certificate.format() != Certificate::SGX_ATTESTATION_KEY_CERTIFICATE) {
    return Status(
        error::GoogleError::INVALID_ARGUMENT,
        absl::StrFormat(
            "The certificate format (%s) does not match the expected format "
            "(SGX_ATTESTATION_KEY_CERTIFICATE)",
            Certificate_CertificateFormat_Name(certificate.format())));
  }

  AttestationKeyCertificate attestation_key_cert;
  if (!attestation_key_cert.ParseFromString(certificate.data())) {
    return Status(
        error::GoogleError::INVALID_ARGUMENT,
        "Could not parse Attestation Key Certificate from certificate data");
  }

  if (!attestation_key_cert.has_pce_sign_report_payload() ||
      !attestation_key_cert.has_report() ||
      !attestation_key_cert.has_signature()) {
    return Status(error::GoogleError::INVALID_ARGUMENT,
                  "Attestation Key Certificate is missing required data");
  }

  PceSignReportPayload pce_sign_report_payload;
  if (!pce_sign_report_payload.ParseFromString(
          attestation_key_cert.pce_sign_report_payload())) {
    return Status(
        error::GoogleError::INTERNAL,
        "Could not parse the serialized PceSignReportPayload message");
  }

  if (pce_sign_report_payload.version() != kPceSignReportPayloadVersion) {
    return Status(
        error::GoogleError::INVALID_ARGUMENT,
        absl::StrFormat(
            "PceSignReportPayload version (%s) does not match the expected "
            "version (%s)",
            pce_sign_report_payload.version(), kPceSignReportPayloadVersion));
  }

  const AttestationPublicKey &public_key =
      pce_sign_report_payload.attestation_public_key();

  // Check that the signature scheme of the signature is consistent with the
  // signature scheme expected of the current PCE implementation.
  const Signature &signature = attestation_key_cert.signature();
  if (signature.signature_scheme() != kExpectedSignatureScheme) {
    return Status(
        error::GoogleError::INVALID_ARGUMENT,
        absl::StrFormat(
            "Signature scheme of signature (%s) does not match the signature "
            "scheme used by the PCE implementation (%s)",
            SignatureScheme_Name(signature.signature_scheme()),
            SignatureScheme_Name(kExpectedSignatureScheme)));
  }

  // Check the purpose and version values of the AttestationPublicKey.
  if (public_key.version() != kAttestationPublicKeyVersion) {
    return Status(
        error::GoogleError::INVALID_ARGUMENT,
        absl::StrFormat("Version of attestation public key (%s) does "
                        "not match the expected version (%s)",
                        public_key.version(), kAttestationPublicKeyVersion));
  }
  if (public_key.purpose() != kAttestationPublicKeyPurpose) {
    return Status(
        error::GoogleError::INVALID_ARGUMENT,
        absl::StrFormat("Purpose of the attestation public key (%s) "
                        "does not match the expected purpose (%s)",
                        public_key.purpose(), kAttestationPublicKeyPurpose));
  }

  // Verify the report data matches the generated AAD.
  std::unique_ptr<AdditionalAuthenticatedDataGenerator> data_generator;
  ASYLO_ASSIGN_OR_RETURN(
      data_generator,
      AdditionalAuthenticatedDataGenerator::CreatePceSignReportAadGenerator());
  std::string expected_aad;
  ASYLO_ASSIGN_OR_RETURN(
      expected_aad,
      data_generator->Generate(attestation_key_cert.pce_sign_report_payload()));
  Report report;
  ASYLO_ASSIGN_OR_RETURN(report, ConvertReportProtoToHardwareReport(
                                     attestation_key_cert.report()));
  std::string report_data =
      ConvertTrivialObjectToBinaryString(report.body.reportdata.data);
  if (report_data != expected_aad) {
    return Status(
        error::GoogleError::INVALID_ARGUMENT,
        absl::StrFormat("Additional authenticated data generated from the PCE "
                        "Sign Report payload (%s) should be consistent with "
                        "the REPORTDATA section (%s)",
                        absl::BytesToHexString(expected_aad),
                        absl::BytesToHexString(report_data)));
  }

  return absl::WrapUnique<AttestationKeyCertificateImpl>(
      new AttestationKeyCertificateImpl(std::move(attestation_key_cert),
                                        std::move(pce_sign_report_payload)
                                            .attestation_public_key()
                                            .attestation_public_key(),
                                        report));
}

StatusOr<SgxIdentity> AttestationKeyCertificateImpl::GetAssertedSgxIdentity()
    const {
  SgxIdentity sgx_identity;
  ASYLO_RETURN_IF_ERROR(
      ParseIdentityFromHardwareReport(report_, &sgx_identity));
  return sgx_identity;
}

Status AttestationKeyCertificateImpl::Verify(
    const CertificateInterface &issuer_certificate,
    const VerificationConfig &config) const {
  // Verify the signature.
  std::unique_ptr<VerifyingKey> verifying_key;
  ASYLO_ASSIGN_OR_RETURN(
      verifying_key,
      CreateVerifyingKey(issuer_certificate,
                         attestation_key_cert_.signature().signature_scheme()));
  return verifying_key->Verify(
      ByteContainerView(attestation_key_cert_.report().value()),
      attestation_key_cert_.signature());
}

StatusOr<std::string> AttestationKeyCertificateImpl::SubjectKeyDer() const {
  return VerifyingKeyToDer(subject_key_);
}

absl::optional<bool> AttestationKeyCertificateImpl::IsCa() const {
  return false;
}

absl::optional<int64_t> AttestationKeyCertificateImpl::CertPathLength() const {
  return absl::nullopt;
}

absl::optional<KeyUsageInformation> AttestationKeyCertificateImpl::KeyUsage()
    const {
  return absl::nullopt;
}

AttestationKeyCertificateImpl::AttestationKeyCertificateImpl(
    AttestationKeyCertificate attestation_key_cert,
    AsymmetricSigningKeyProto subject_key, Report report)
    : attestation_key_cert_(std::move(attestation_key_cert)),
      subject_key_(std::move(subject_key)),
      report_(report) {}

}  // namespace sgx
}  // namespace asylo

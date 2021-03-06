/*
 *
 * Copyright 2017 Asylo authors
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

#include "asylo/identity/sgx/local_secret_sealer_helpers.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "asylo/crypto/algorithms.pb.h"
#include "asylo/crypto/sha256_hash.h"
#include "asylo/crypto/util/byte_container_util.h"
#include "asylo/crypto/util/bytes.h"
#include "asylo/crypto/util/trivial_object_util.h"
#include "asylo/identity/identity.pb.h"
#include "asylo/identity/identity_acl.pb.h"
#include "asylo/identity/sealed_secret.pb.h"
#include "asylo/identity/sgx/hardware_interface.h"
#include "asylo/identity/sgx/local_sealed_secret.pb.h"
#include "asylo/identity/sgx/machine_configuration.pb.h"
#include "asylo/identity/sgx/platform_provisioning.pb.h"
#include "asylo/identity/sgx/self_identity.h"
#include "asylo/identity/sgx/sgx_identity_util_internal.h"
#include "asylo/platform/common/singleton.h"
#include "asylo/util/cleansing_types.h"
#include "asylo/util/status_macros.h"

namespace asylo {
namespace sgx {
namespace {

// Translates |cipher_suite| to the AeadScheme equivalent.
AeadScheme CipherSuiteToAeadScheme(CipherSuite cipher_suite) {
  switch (cipher_suite) {
    case sgx::AES256_GCM_SIV:
      return AeadScheme::AES256_GCM_SIV;
    default:
      return AeadScheme::UNKNOWN_AEAD_SCHEME;
  }
}

// Use the presence of the |version| field in the reference identity contained
// in the header ACL to determine whether the secret was created before/after
// the migration from CodeIdentity to SgxIdentity.
bool IsLegacySealedSecret(const SealedSecretHeader &header) {
  return !header.client_acl().expectation().reference_identity().has_version();
}

}  // namespace

namespace internal {

using experimental::AeadCryptor;

const char *const kSgxLocalSecretSealerRootName = "SGX";

Status ParseKeyGenerationParamsFromSealedSecretHeader(
    const SealedSecretHeader &header, AeadScheme *aead_scheme,
    SgxIdentityExpectation *sgx_expectation) {
  const SealingRootInformation &root_info = header.root_info();
  if (root_info.sealing_root_type() != LOCAL) {
    return Status(error::GoogleError::INVALID_ARGUMENT,
                  "Incorrect sealing_root_type");
  }
  if (root_info.sealing_root_name() != kSgxLocalSecretSealerRootName) {
    return Status(error::GoogleError::INVALID_ARGUMENT,
                  "Incorrect sealing_root_name");
  }
  if (header.client_acl().item_case() != IdentityAclPredicate::kExpectation) {
    return Status(error::GoogleError::INVALID_ARGUMENT, "Malformed client_acl");
  }

  const EnclaveIdentityExpectation &generic_expectation =
      header.client_acl().expectation();

  bool is_legacy = IsLegacySealedSecret(header);

  if (is_legacy) {
    LOG(WARNING)
        << "Support for unsealing pre-Asylo 0.5 secrets is deprecated, and "
           "will be removed in Asylo 0.6. To retain the ability to unseal "
        << (header.has_secret_name() ? header.secret_name() : "this secret")
        << ", please reseal it (see the Asylo release notes for more info).";
  }

  // Call ParseSgxExpectation before attempting to populate legacy fields to
  // avoid causing the parsing step to "undo" any of the fields manually set
  // by the legacy flow.
  ASYLO_RETURN_IF_ERROR(
      ParseSgxExpectation(generic_expectation, sgx_expectation, is_legacy));

  // Set CPUSVN from SealedSecretAdditionalInfo if present (ie. legacy secret);
  // otherwise, it was populated when parsing the SgxIdentityExpectation above.
  if (root_info.has_additional_info()) {
    SealedSecretAdditionalInfo info;
    if (!info.ParseFromString(root_info.additional_info())) {
      return Status(error::GoogleError::INVALID_ARGUMENT,
                    "Could not parse additional_info");
    }
    if (info.cpusvn().size() != kCpusvnSize) {
      return Status(error::GoogleError::INVALID_ARGUMENT,
                    "Incorrect cpusvn size");
    }
    sgx_expectation->mutable_reference_identity()
        ->mutable_machine_configuration()
        ->mutable_cpu_svn()
        ->set_value(info.cpusvn());
  }

  ASYLO_ASSIGN_OR_RETURN(*aead_scheme,
                         ParseAeadSchemeFromSealedSecretHeader(header));

  bool result;
  std::string explanation;
  ASYLO_ASSIGN_OR_RETURN(
      result,
      MatchIdentityToExpectation(GetSelfIdentity()->sgx_identity,
                                 *sgx_expectation, &explanation, is_legacy));
  if (!result) {
    return Status(
        error::GoogleError::PERMISSION_DENIED,
        absl::StrCat("Identity of the current enclave does not match the ACL: ",
                     explanation));
  }
  return Status::OkStatus();
}

uint16_t ConvertMatchSpecToKeypolicy(const SgxIdentityMatchSpec &spec) {
  uint16_t policy = 0;
  if (spec.code_identity_match_spec().is_mrenclave_match_required()) {
    policy |= kKeypolicyMrenclaveBitMask;
  }
  if (spec.code_identity_match_spec().is_mrsigner_match_required()) {
    policy |= kKeypolicyMrsignerBitMask;
  }
  return policy;
}

Status GenerateCryptorKey(AeadScheme aead_scheme, const std::string &key_id,
                          const SgxIdentityExpectation &sgx_expectation,
                          size_t key_size, CleansingVector<uint8_t> *key) {
  // The function generates the |key_size| number of bytes by concatenating
  // bytes from one or more hardware-generated "subkeys." Each of the subkeys
  // is obtained by calling the GetHardwareKey() function. Except for the last
  // subkey, all bytes from all other subkeys are utilized. If more than one
  // subkey is used, each subkey is generated using a different value of
  // the KEYID field of the KEYREQUEST input to the GetHardwareKey() function.
  // All the other fields of the KEYREQUEST structure stay unchanged across
  // the GetHardwareKey() calls.

  // Create and populate an aligned KEYREQUEST structure.
  AlignedKeyrequestPtr req;

  // Zero-out the KEYREQUEST.
  *req = TrivialZeroObject<Keyrequest>();

  req->keyname = KeyrequestKeyname::SEAL_KEY;
  req->keypolicy = ConvertMatchSpecToKeypolicy(sgx_expectation.match_spec());
  req->isvsvn = sgx_expectation.reference_identity()
                    .code_identity()
                    .signer_assigned_identity()
                    .isvsvn();
  req->cpusvn = UnsafeBytes<kCpusvnSize>(sgx_expectation.reference_identity()
                                             .machine_configuration()
                                             .cpu_svn()
                                             .value());

  ConvertSecsAttributeRepresentation(sgx_expectation.match_spec()
                                         .code_identity_match_spec()
                                         .attributes_match_mask(),
                                     &req->attributemask);
  // req->keyid is populated uniquely on each call to GetHardwareKey().
  req->miscmask = sgx_expectation.match_spec()
                      .code_identity_match_spec()
                      .miscselect_match_mask();

  key->resize(0);
  key->reserve(key_size);

  size_t remaining_key_bytes = key_size;
  size_t key_subscript = 0;
  Sha256Hash hasher;
  while (remaining_key_bytes > 0) {
    std::vector<uint8_t> key_info;
    // Build a key_info string that uniquely and unambiguously encodes
    // sealing-root description, ciphersuite with which the key will be used,
    // key identifier, key size, and key subscript.
    ASYLO_RETURN_IF_ERROR(SerializeByteContainers(
        &key_info, SealingRootType_Name(LOCAL), kSgxLocalSecretSealerRootName,
        AeadScheme_Name(aead_scheme), key_id, absl::StrCat(key_size),
        absl::StrCat(key_subscript)));

    static_assert(decltype(req->keyid)::size() == SHA256_DIGEST_LENGTH,
                  "KEYREQUEST.KEYID field has unexpected size");

    hasher.Init();
    hasher.Update(key_info);
    std::vector<uint8_t> digest;
    hasher.CumulativeHash(&digest);
    req->keyid.assign(digest);

    AlignedHardwareKeyPtr hardware_key;
    ASYLO_RETURN_IF_ERROR(GetHardwareKey(*req, hardware_key.get()));
    size_t copy_size = std::min(hardware_key->size(), remaining_key_bytes);
    remaining_key_bytes -= copy_size;
    std::copy(hardware_key->cbegin(), hardware_key->cbegin() + copy_size,
              std::back_inserter(*key));
    ++key_subscript;
  }
  return Status::OkStatus();
}

StatusOr<std::unique_ptr<AeadCryptor>> MakeCryptor(AeadScheme aead_scheme,
                                                   ByteContainerView key) {
  switch (aead_scheme) {
    case AeadScheme::AES256_GCM_SIV:
      return AeadCryptor::CreateAesGcmSivCryptor(key);
    default:
      return Status(error::GoogleError::INVALID_ARGUMENT,
                    "Unsupported cipher suite");
  }
}

Status Seal(AeadCryptor *cryptor, ByteContainerView secret,
            ByteContainerView additional_data, SealedSecret *sealed_secret) {
  std::vector<uint8_t> ciphertext(secret.size() + cryptor->MaxSealOverhead());
  std::vector<uint8_t> iv(cryptor->NonceSize());

  size_t ciphertext_size = 0;
  ASYLO_RETURN_IF_ERROR(
      cryptor->Seal(secret, additional_data, absl::MakeSpan(iv),
                    absl::MakeSpan(ciphertext), &ciphertext_size));

  sealed_secret->set_secret_ciphertext(ciphertext.data(), ciphertext_size);
  sealed_secret->set_iv(iv.data(), iv.size());

  return Status::OkStatus();
}

Status Open(AeadCryptor *cryptor, const SealedSecret &sealed_secret,
            ByteContainerView additional_data,
            CleansingVector<uint8_t> *secret) {
  secret->resize(sealed_secret.secret_ciphertext().size());
  size_t plaintext_size = 0;
  ASYLO_RETURN_IF_ERROR(cryptor->Open(
      sealed_secret.secret_ciphertext(), additional_data, sealed_secret.iv(),
      absl::MakeSpan(*secret), &plaintext_size));
  secret->resize(plaintext_size);
  return Status::OkStatus();
}

StatusOr<AeadScheme> ParseAeadSchemeFromSealedSecretHeader(
    const SealedSecretHeader &header) {
  AeadScheme aead_scheme;
  if (IsLegacySealedSecret(header)) {
    // This is a legacy secret, so we need to parse the sgx::CipherSuite from
    // the |additional_info| field, and then convert it to asylo::AeadScheme.
    SealedSecretAdditionalInfo info;
    if (!info.ParseFromString(header.root_info().additional_info())) {
      return Status(error::GoogleError::INVALID_ARGUMENT,
                    "Could not parse additional_info");
    }
    aead_scheme = CipherSuiteToAeadScheme(info.cipher_suite());
  } else {
    aead_scheme = header.root_info().aead_scheme();
  }

  if (aead_scheme != AeadScheme::AES256_GCM_SIV) {
    return Status(
        error::GoogleError::INVALID_ARGUMENT,
        absl::StrCat("Unsupported AeadScheme ", AeadScheme_Name(aead_scheme)));
  }

  return aead_scheme;
}

}  // namespace internal
}  // namespace sgx
}  // namespace asylo

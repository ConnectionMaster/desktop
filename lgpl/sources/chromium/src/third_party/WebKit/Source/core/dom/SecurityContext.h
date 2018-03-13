/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef SecurityContext_h
#define SecurityContext_h

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "core/CoreExport.h"
#include "core/dom/SandboxFlags.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/Suborigin.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "public/platform/WebURLRequest.h"
#include "third_party/WebKit/common/feature_policy/feature_policy.h"

#include <memory>

namespace blink {

class SecurityOrigin;
class ContentSecurityPolicy;

class CORE_EXPORT SecurityContext : public GarbageCollectedMixin {
 public:
  virtual void Trace(blink::Visitor*);

  using InsecureNavigationsSet = HashSet<unsigned, WTF::AlreadyHashed>;

  // TODO(hiroshige): Make GetSecurityOrigin() return |const SecurityOrigin*|.
  // crbug.com/779730
  SecurityOrigin* GetSecurityOrigin() const { return security_origin_.get(); }
  SecurityOrigin* GetMutableSecurityOrigin() { return security_origin_.get(); }

  ContentSecurityPolicy* GetContentSecurityPolicy() const {
    return content_security_policy_.Get();
  }

  // Explicitly override the security origin for this security context.
  // Note: It is dangerous to change the security origin of a script context
  //       that already contains content.
  void SetSecurityOrigin(scoped_refptr<SecurityOrigin>);
  virtual void DidUpdateSecurityOrigin() = 0;

  SandboxFlags GetSandboxFlags() const { return sandbox_flags_; }
  bool IsSandboxed(SandboxFlags mask) const { return sandbox_flags_ & mask; }
  virtual void EnforceSandboxFlags(SandboxFlags mask);

  void SetAddressSpace(WebAddressSpace space) { address_space_ = space; }
  WebAddressSpace AddressSpace() const { return address_space_; }
  String addressSpaceForBindings() const;

  void SetRequireTrustedTypes() { require_safe_types_ = true; }
  bool RequireTrustedTypes() const { return require_safe_types_; }

  void AddInsecureNavigationUpgrade(unsigned hashed_host) {
    insecure_navigations_to_upgrade_.insert(hashed_host);
  }
  InsecureNavigationsSet* InsecureNavigationsToUpgrade() {
    return &insecure_navigations_to_upgrade_;
  }

  virtual void SetInsecureRequestPolicy(WebInsecureRequestPolicy policy) {
    insecure_request_policy_ = policy;
  }
  WebInsecureRequestPolicy GetInsecureRequestPolicy() const {
    return insecure_request_policy_;
  }

  void EnforceSuborigin(const Suborigin&);

  FeaturePolicy* GetFeaturePolicy() const { return feature_policy_.get(); }
  void InitializeFeaturePolicy(const ParsedFeaturePolicy& parsed_header,
                               const ParsedFeaturePolicy& container_policy,
                               const FeaturePolicy* parent_feature_policy);
  void UpdateFeaturePolicyOrigin();

  void ApplySandboxFlags(SandboxFlags mask);

 protected:
  SecurityContext();
  virtual ~SecurityContext();

  void SetContentSecurityPolicy(ContentSecurityPolicy*);

  SandboxFlags sandbox_flags_;

 private:
  scoped_refptr<SecurityOrigin> security_origin_;
  Member<ContentSecurityPolicy> content_security_policy_;
  std::unique_ptr<FeaturePolicy> feature_policy_;

  WebAddressSpace address_space_;
  WebInsecureRequestPolicy insecure_request_policy_;
  InsecureNavigationsSet insecure_navigations_to_upgrade_;
  bool require_safe_types_;
  DISALLOW_COPY_AND_ASSIGN(SecurityContext);
};

}  // namespace blink

#endif  // SecurityContext_h

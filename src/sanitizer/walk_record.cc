/*
//@HEADER
// *****************************************************************************
//
//                                walk_record.cc
//                           DARMA Toolkit v. 1.0.0
//                       DARMA/Serialization Sanitizer
//
// Copyright 2019 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact darma@sandia.gov
//
// *****************************************************************************
//@HEADER
*/

#include "common.h"
#include "walk_record.h"
#include "member_list.h"

#include <fmt/format.h>

namespace sanitizer {

void WalkRecord::walk(MatchResult const& result) {
  using clang::isa;
  using clang::cast;
  using clang::CXXRecordDecl;
  using clang::TemplateTypeParmDecl;

  auto const *rd = result.Nodes.getNodeAs<CXXRecordDecl>("recordDecl");
  if (rd) {
    #if SANITIZER_DEBUG
      fmt::print("Traversing class {}\n", rd->getQualifiedNameAsString());
    #endif

    bool temp_instantiation = rd->getTemplateInstantiationPattern() != nullptr;
    if (gen_inline_ && temp_instantiation) {
      // skip template instantiation when generating inline
      return;
    } else if (rd->getDescribedClassTemplate()) {
      // skip template classes when generating out-of-line
      return;
    }

    // If this is a member class of a class template, but not an instantiation
    // of a member class, we need to skip it
    for (auto* p = rd->getDeclContext(); p; p = p->getParent()) {
      if (clang::isa<CXXRecordDecl>(p)) {
        auto parent = clang::cast<CXXRecordDecl>(p);
        if (parent->getDescribedClassTemplate()) {
          return;
        }
      }
    }

    // Walk declarations for this struct
    for (auto&& m : rd->decls()) {
      // Skip non-templated functions
      if (not m->isTemplateDecl()) {
        continue;
      }

      // Skip functions not called serialize that have exactly one parameter
      // Matches intrusive pattern
      auto fn = m->getAsFunction();
      if (!fn || fn->getNameAsString() != "serialize" || fn->param_size() != 1) {
        continue;
      }

      // Examine the template parameters to the serialize function
      auto ft = fn->getDescribedFunctionTemplate();
      auto tp = ft->getTemplateParameters();

      // If we are more than one, we might have a enable_if (Footprinting?)
      if (tp->size() != 1) {
        #if SANITIZER_DEBUG
          for (unsigned int i = 0; i < tp->size(); i++) {
            auto elm = tp->getParam(i);
            if (isa<TemplateTypeParmDecl>(elm)) {
              auto ttpd = cast<TemplateTypeParmDecl>(elm);
              if (ttpd->hasDefaultArgument()) {
                fmt::print("Template parameter with default argument\n");
                ttpd->getDefaultArgument()->dump();
              } else {
                fmt::print("Template parameter without default argument\n");
                ttpd->dumpColor();
              }
            }
          }
        #endif

        continue;
      }

      // After all these checks, we have a valid serialize!
      found_serialize_ = true;

      // Look for any existing checks in the body
      findExistingChecks(fn);

      // Gather the member fields in the class
      gatherMembers(rd);

      // Invoke the code generator
      if (gen_ != nullptr) {
        gen_->run(rd, fn, members_);
      }

      break;
    }
  }
}

void WalkRecord::gatherMembers(clang::CXXRecordDecl const* rd) {
  #if SANITIZER_DEBUG
    fmt::print("Gather members of class {}\n", rd->getQualifiedNameAsString());
  #endif

  // Walk all the fields in the class
  for (auto&& f : rd->fields()) {
    auto qual = f->getQualifiedNameAsString();
    auto unqual = f->getNameAsString();

    // Skip members that already have checks
    auto iter = existing_checks_.find(unqual);
    if (iter == existing_checks_.end()) {
      members_.emplace_back(Member{unqual, qual});
    }
  }
}

void WalkRecord::findExistingChecks(clang::FunctionDecl* fn) {
  using clang::isa;
  using clang::cast;
  using clang::CallExpr;
  using clang::CXXDependentScopeMemberExpr;
  using clang::MemberExpr;

  // Skip if the function body is missing
  if (not fn->hasBody()) {
    return;
  }

  for (auto&& elm : fn->getBody()->children()) {
    // Skip any non-call expressions
    if (not isa<CallExpr>(elm)) {
      continue;
    }

    // Only look for a specific expression form
    auto ce = cast<CallExpr>(elm);
    if (!ce or not ce->getCallee()->isTypeDependent() or ce->getNumArgs() != 2) {
      continue;
    }

    // Inspect further to see if this is a "check" invocation
    auto iter = ce->child_begin();
    if (iter != ce->child_end()) {
      if (isa<CXXDependentScopeMemberExpr>(*iter)) {
        auto cxx = cast<CXXDependentScopeMemberExpr>(*iter);
        if (cxx->getMemberNameInfo().getName().getAsString() == "check") {
          iter++;

          // Save the existing check to a list so we know it exists
          if (isa<MemberExpr>(*iter)) {
            auto member = clang::cast<clang::MemberExpr>(*iter);
            existing_checks_.insert(member->getMemberDecl()->getNameAsString());
          }
        }
      }

    }
  }
}


} /* end namespace sanitizer */

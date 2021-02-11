/*
//@HEADER
// *****************************************************************************
//
//                                  plugin.cc
//                           DARMA Toolkit v. 1.0.0
//                       DARMA/Serialization Sanitizer
//
// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
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

#include "plugin.h"

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

#include <unordered_set>

using namespace clang;
using namespace ast_matchers;

namespace plugin {

void printWarning(FieldDecl const* field_decl) {
  llvm::errs() << "Warning: field ";
  llvm::errs().changeColor(llvm::raw_ostream::YELLOW, true);
  // consider field_decl->getNameForDiagnostic
  llvm::errs() << field_decl->getQualifiedNameAsString();
  llvm::errs().resetColor();
  llvm::errs() << " is not serialized.\n";
}

std::unordered_set<FieldDecl const*>getSerializedFields(
  CXXMethodDecl const* method
) {
  std::unordered_set<FieldDecl const*> serialized;
  for (auto child : method->getBody()->children()) {
    BinaryOperator const* binary_op = dyn_cast<BinaryOperator>(child);
    if (not binary_op) {
      continue;
    }

    Expr const* lhs;
    do {
      lhs = binary_op->getLHS();
      auto rhs_member_expr = dyn_cast<MemberExpr>(binary_op->getRHS());
      if (!rhs_member_expr) {
        continue; // possibly CXXDependentScopeMemberExpr
      }
      auto field = dyn_cast<FieldDecl>(rhs_member_expr->getMemberDecl());
      serialized.insert(field->getFirstDecl());
    } while ((binary_op = dyn_cast<BinaryOperator>(lhs)));
  }

  return serialized;
}

void SanitizerMatcher::run(MatchFinder::MatchResult const& result) {
  auto record = result.Nodes.getNodeAs<CXXRecordDecl>("record");
  // llvm::errs() << "Processing " << record->getQualifiedNameAsString() << "\n";
  auto method = result.Nodes.getNodeAs<CXXMethodDecl>("method");
  if (not method->hasBody()) {
    return;
  }

  auto serialized = getSerializedFields(method);
  for (auto field : record->fields()) {
    if (serialized.find(field->getFirstDecl()) == serialized.end()) {
      printWarning(field);
    }
  }
}

SanitizerASTConsumer::SanitizerASTConsumer() {
  auto binary_op =
    cxxRecordDecl(
      has(functionTemplateDecl(
        hasName("serialize"), has(cxxMethodDecl(
          parameterCountIs(1)
        ).bind("method"))
      ))
    ).bind("record");

  finder_.addMatcher(binary_op, &callback_);
}


// FrotendAction
struct SanitizerPluginAction : public PluginASTAction {
public:
  bool ParseArgs(CompilerInstance const&,
                 std::vector<std::string> const&) override {
    return true;
  }

  // returns our ASTConsumer per translation unit.
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance&,
                                                 StringRef) override {
    return std::make_unique<SanitizerASTConsumer>();
  }
};

// register the plugin
static FrontendPluginRegistry::Add<SanitizerPluginAction>
    X(/*Name=*/"sanitizer",
      /*Desc=*/"Serialization Sanitizer");

} /* end namespace plugin */

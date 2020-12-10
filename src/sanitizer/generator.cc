/*
//@HEADER
// *****************************************************************************
//
//                                 generator.cc
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
#include "generator.h"

#include "qualified_name.h"

#include <fmt/format.h>

#include <list>
#include <string>

namespace sanitizer {

void InlineGenerator::run(
  clang::CXXRecordDecl const* rd, clang::FunctionDecl* fn,
  MemberListType members
) {
  // No members to generate
  if (members.size() == 0) {
    return;
  }

  // If the class is lacking a serialize body, code can't be generated
  if (not fn->hasBody()) {
    fmt::print(
      stderr,
      "{}: {} members exist, but no serialize body found!\n",
      rd->getQualifiedNameAsString(), members.size()
    );
    return;
  }

  #if SANITIZER_DEBUG
    fmt::print("Inserting checks for {}\n", rd->getQualifiedNameAsString());
  #endif

  auto body = fn->getBody();
  auto start = body->getLocEnd();
  rw_.InsertText(start, "  /* begin generated sanitizer code */\n", true, true);
  for (auto&& m : members) {
    auto str = fmt::format("  s.check({}, \"{}\");\n", m.unqual(), m.qual());
    rw_.InsertText(start, str, true, true);
  }
  rw_.InsertText(start, "  /* end generated sanitizer code */\n", true, true);
}

static constexpr char const* sanitizer = "checkpoint::serializers::Sanitizer";
static constexpr char const* begin = "{";
static constexpr char const* end = "}";

void PartialSpecializationGenerator::run(
  clang::CXXRecordDecl const* rd, clang::FunctionDecl* fn,
  MemberListType members
) {
  #if SANITIZER_DEBUG
    fmt::print("Gen specialization for {}\n", rd->getQualifiedNameAsString());
  #endif

  using clang::TemplateSpecializationKind;

  TemplateSpecializationKind kind = rd->getTemplateSpecializationKind();

  if (kind == TemplateSpecializationKind::TSK_Undeclared) {
    auto qual_name = rd->getQualifiedNameAsString();

    fmt::print(out_, "template <>\n");
    fmt::print(
      out_, "void {}::serialize<{}>({}& s) {}\n",
      qual_name, sanitizer, sanitizer, begin
    );
    for (auto&& m : members) {
      fmt::print(out_, "  s.check({}, \"{}\");\n", m.unqual(), m.qual());
    }
    fmt::print(out_, "{}\n", end);
  } else if (kind == TemplateSpecializationKind::TSK_ImplicitInstantiation) {
    if (clang::isa<clang::ClassTemplateSpecializationDecl>(rd)) {
      auto ctsd = clang::cast<clang::ClassTemplateSpecializationDecl>(rd);

      clang::PrintingPolicy policy(ctsd->getASTContext().getPrintingPolicy());
      policy.SuppressScope = false;
      policy.AnonymousTagLocations = false;
      policy.PolishForDeclaration = true;
      policy.SuppressUnwrittenScope = true;

      auto qt = clang::TypeName2::getFullyQualifiedType(
        clang::QualType(ctsd->getTypeForDecl(),0), ctsd->getASTContext(), false
      );

      std::string qualified_type_outer = qt.getAsString(policy);

      fmt::print(out_, "template <>\n");
      fmt::print(out_, "template <>\n");
      fmt::print(
        out_, "void {}::serialize<{}>({}& s) {}\n", qualified_type_outer,
        sanitizer, sanitizer, begin
      );
      for (auto&& m : members) {
        fmt::print(out_,"  s.check({}, \"{}\");\n", m.unqual(), m.qual());
      }
      fmt::print(out_,"{}\n", end);
    }
  }
}

void SeperateGenerator::run(
  clang::CXXRecordDecl const* rd, clang::FunctionDecl* fn,
  MemberListType members
) {
  std::list<std::string> templates_decl;
  std::list<std::string> templates_def;

  auto cls_templ = rd->getDescribedClassTemplate();
  if (cls_templ) {
    auto tparams = cls_templ->getTemplateParameters();
    for (auto iter = tparams->begin(); iter != tparams->end(); ++iter) {
      auto tparam = *iter;
      if (clang::isa<clang::TemplateTypeParmDecl>(tparam)) {
        auto type_param = clang::cast<clang::TemplateTypeParmDecl>(tparam);
        auto pack_str = type_param->isParameterPack() ? "..." : "";
        fmt::print("TYPE TPARAM: {}\n", tparam->getNameAsString());
        templates_decl.push_back(std::string("typename ") + pack_str + type_param->getName().str());
        templates_def.push_back(type_param->getName().str());
        tparam->dump();
      } else if (clang::isa<clang::NonTypeTemplateParmDecl>(tparam)) {
        auto non_type_param = clang::cast<clang::NonTypeTemplateParmDecl>(tparam);

        auto qualified_type = clang::TypeName2::getFullyQualifiedName(
          non_type_param->getType(),
          non_type_param->getASTContext(),
          true
        );

        templates_decl.push_back(
          qualified_type + " " + non_type_param->getName().str()
        );
        templates_def.push_back(non_type_param->getName().str());
        tparam->dump();
      }

    }
  }

  //fmt::print("TEMPLATE: {}\n", template_str)

  std::string template_decl_context = "";
  for (auto&& elm : templates_decl) {
    template_decl_context += ", " + elm;
  }

  std::string template_def_context = "";
  if (templates_def.size() > 0) {
    template_def_context += "<";
    std::size_t cur = 0;
    for (auto&& elm : templates_def) {
      template_def_context += elm;
      if (cur != templates_def.size() - 1) {
        template_def_context += ",";
      }
      cur++;
    }
    template_def_context += ">";
  }

  auto qual_name = rd->getQualifiedNameAsString();

  fmt::print("template <typename SerializerT{}>\n", template_decl_context);
  fmt::print(
    "void serializeCheck(SerializerT& s, {}{}& obj) {\n",
    qual_name, template_def_context
  );
  for (auto&& m : members) {
    fmt::print("\ts.check(obj.{}, \"{}\");\n", m.unqual(), m.qual());
  }
  fmt::print("}\n\n");
}

} /* end namespace sanitizer */

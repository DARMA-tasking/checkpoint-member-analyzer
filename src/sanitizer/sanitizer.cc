/*
//@HEADER
// *****************************************************************************
//
//                                 sanitizer.cc
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

// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/AST/ExprCXX.h"

#include <memory>
#include <list>
#include <tuple>
#include <unordered_set>

#include <fmt/format.h>

#include "qualified_name.h"

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;
using namespace llvm;

static FILE* out = nullptr;

static cl::opt<std::string> Filename("o", cl::desc("Filename to output generated code"));

static cl::list<std::string> Includes("I", cl::desc("Include directories"), cl::ZeroOrMore);

static cl::opt<bool> GenerateInline("inline", cl::desc("Generate code inline and modify files"));

static cl::opt<bool> OutputMainFile("main", cl::desc("Output main file with generated code"));

static cl::opt<bool> IncludeVTHeader("Ivt", cl::desc("Include VT headers in generated code"));

DeclarationMatcher RecordMatcher = cxxRecordDecl().bind("recordDecl");

struct ClassFuncDeclRewriter : MatchFinder::MatchCallback {
  using MemberListType = std::list<std::tuple<std::string, std::string>>;
  using CXXDeclType = CXXRecordDecl const*;

  explicit ClassFuncDeclRewriter(Rewriter& in_rw)
    : rw(in_rw)
  { }

  virtual void run(const MatchFinder::MatchResult &Result) {
    if (CXXRecordDecl const *rd = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("recordDecl")) {
      // fmt::print("Traversing class {}\n", rd->getQualifiedNameAsString());

      // We are in a template instantiation--skip!
      if (GenerateInline) {
        if (rd->getTemplateInstantiationPattern() != nullptr) {
          return;
        }
      } else {
        // Skip template classes, walk instantiations instead
        if (rd->getDescribedClassTemplate()) {
          return;
        }
      }

      //rd->dump();

      FunctionDecl* fndecl = nullptr;
      bool has_serialize = false;
      std::unordered_set<std::string> existing_checks_;

      // Go through the declarations for this struct
      for (auto&& m : rd->decls()) {
        //fmt::print("DECL {}\n", m->getDeclKindName());

        // Look for template decls, named serialize, with exactly one parameter
        // (intrusive) serialize
        if (m->isTemplateDecl()) {
          auto cur_fndecl = m->getAsFunction();
          if (cur_fndecl) {
            if (cur_fndecl->getNameAsString() == "serialize" and cur_fndecl->param_size() == 1) {
              // Save this function decl so we can insert code later
              fndecl = cur_fndecl;
              has_serialize = true;

              // If we have a body the serialize member function, walk it
              if (fndecl->hasBody()) {
                auto body = fndecl->getBody();
                for (auto&& child : body->children()) {
                  // Look for CallExpr
                  if (isa<CallExpr>(child)) {
                    auto ce = cast<CallExpr>(child);
                    if (ce && ce->getCallee()->isTypeDependent() and ce->getNumArgs() == 2) {
                      auto expr_iter = ce->child_begin();
                      if (expr_iter != ce->child_end()) {
                        if (isa<CXXDependentScopeMemberExpr>(*expr_iter)) {
                          auto cxx = cast<CXXDependentScopeMemberExpr>(*expr_iter);
                          if (cxx->getMemberNameInfo().getName().getAsString() == "check") {
                            //ce->dump();
                            expr_iter++;

                            // Store a list of checks that already exist to
                            // compare to members to see if the existing checks
                            // are valid!
                            if (isa<MemberExpr>(*expr_iter)) {
                              auto member = cast<MemberExpr>(*expr_iter);
                              existing_checks_.insert(
                                member->getMemberDecl()->getNameAsString()
                              );
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }

      if (not has_serialize) {
        return;
      }

      MemberListType members_to_serialize;

      auto record = rd->getQualifiedNameAsString();
      //fmt::print("Finding fields for CXX record {}\n", record);
      for (auto&& f : rd->fields()) {
        auto member = f->getQualifiedNameAsString();
        auto unqualified_member = f->getNameAsString();

        auto existing_iter = existing_checks_.find(unqualified_member);
        if (existing_iter == existing_checks_.end()) {
          //fmt::print("{}: {}\n", record, member);
          //f->dumpColor();
          members_to_serialize.push_back(std::make_tuple(unqualified_member, member));
        }
      }

      if (GenerateInline) {
        if (not fndecl->hasBody() and members_to_serialize.size() > 0) {
          fmt::print(
            stderr,
            "{}: {} members exist, but no serialize body found!\n",
            record, members_to_serialize.size()
          );
          return;
        }
      }

      if (members_to_serialize.size() > 0) {
        if (GenerateInline) {
          generateChecksInline(rd, fndecl, members_to_serialize);
        } else {
          generateChecksSeperateInstance(rd, fndecl, members_to_serialize);
        }
      }
    }
  }

  void generateChecksInline(
    CXXDeclType rd, FunctionDecl* fn, MemberListType const& members
  ) {
    if (fn->hasBody()) {
      auto record = rd->getQualifiedNameAsString();
      fmt::print("Inserting new checks for: %s\n", record.c_str());
      auto body = fn->getBody();

      auto start = body->getLocEnd();
      rw.InsertText(start, "  /* begin generated serialize check code */\n", true, true);
      for (auto&& elm : members) {
        rw.InsertText(
          start,
          "  s.check(" + std::get<0>(elm) + "," "\"" + std::get<1>(elm) + "\"" ");\n",
          true, true
        );
      }
      rw.InsertText(start, "  /* end generated serialize check code */\n", true, true);
    }
  }

  void generateChecksSeperateInstance(
    CXXDeclType rd, FunctionDecl* fn, MemberListType const& members
  ) {
    //rd->dump();
    //fmt::print("{}: SPECIALIZATION: {}\n", rd->getQualifiedNameAsString(), rd->getTemplateSpecializationKind());

    TemplateSpecializationKind kind = rd->getTemplateSpecializationKind();

    if (kind == TemplateSpecializationKind::TSK_Undeclared) {
      auto qual_name = rd->getQualifiedNameAsString();

      fmt::print(out,"template <>\n");
      fmt::print(out,"void {}::serialize<checkpoint::dispatch::Counter>(checkpoint::dispatch::Counter& s) {\n", qual_name);
      for (auto&& m : members) {
        fmt::print(out,"\ts.check({}, \"{}\");\n", std::get<0>(m), std::get<1>(m));
      }
      fmt::print(out,"}\n");
    } else if (kind == TemplateSpecializationKind::TSK_ImplicitInstantiation) {
      if (isa<ClassTemplateSpecializationDecl>(rd)) {
        auto ctsd = cast<ClassTemplateSpecializationDecl>(rd);

        PrintingPolicy policy(ctsd->getASTContext().getPrintingPolicy());
        policy.SuppressScope = false;
        policy.AnonymousTagLocations = false;
        policy.PolishForDeclaration = true;
        policy.SuppressUnwrittenScope = true;

        auto qt = clang::TypeName2::getFullyQualifiedType(
          QualType(ctsd->getTypeForDecl(),0), ctsd->getASTContext(), false
        );

        std::string qualified_type_outer = qt.getAsString(policy);

        fmt::print(out,"template <>\n");
        fmt::print(out,"template <>\n");
        fmt::print(out,"void {}::serialize<checkpoint::dispatch::Counter>(checkpoint::dispatch::Counter& s) {\n",
                   qualified_type_outer);
        for (auto&& m : members) {
          fmt::print(out,"\ts.check({}, \"{}\");\n", std::get<0>(m), std::get<1>(m));
        }
        fmt::print(out,"}\n");
      }
    }
  }

  void generateChecksSeperate(
    CXXDeclType rd, FunctionDecl* fn, MemberListType const& members
  ) {
    std::list<std::string> templates_decl;
    std::list<std::string> templates_def;

    auto cls_templ = rd->getDescribedClassTemplate();
    if (cls_templ) {
      auto tparams = cls_templ->getTemplateParameters();
      for (auto iter = tparams->begin(); iter != tparams->end(); ++iter) {
        auto tparam = *iter;
        if (isa<TemplateTypeParmDecl>(tparam)) {
          auto type_param = cast<TemplateTypeParmDecl>(tparam);
          auto pack_str = type_param->isParameterPack() ? "..." : "";
          fmt::print("TYPE TPARAM: {}\n", tparam->getNameAsString());
          templates_decl.push_back(std::string("typename ") + pack_str + type_param->getName().str());
          templates_def.push_back(type_param->getName().str());
          tparam->dump();
        } else if (isa<NonTypeTemplateParmDecl>(tparam)) {
          auto non_type_param = cast<NonTypeTemplateParmDecl>(tparam);

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

    //rd->dumpColor();

    auto qual_name = rd->getQualifiedNameAsString();

    fmt::print("template <typename SerializerT{}>\n", template_decl_context);
    fmt::print(
      "void serializeCheck(SerializerT& s, {}{}& obj) {\n",
      qual_name, template_def_context
    );
    for (auto&& m : members) {
      fmt::print("\ts.check(obj.{}, \"{}\");\n", std::get<0>(m), std::get<1>(m));
    }
    fmt::print("}\n\n");
  }

  Rewriter& rw;
};

struct MyASTConsumer : ASTConsumer {
  MyASTConsumer(Rewriter& in_rw) : record_handler(in_rw) {
    matcher_.addMatcher(RecordMatcher, &record_handler);
  }

  void HandleTranslationUnit(ASTContext& Context) override {
    // Run the matchers when we have the whole TU parsed.
    matcher_.matchAST(Context);
  }

private:
  ClassFuncDeclRewriter record_handler;
  MatchFinder matcher_;
};

// For each source file provided to the tool, a new FrontendAction is created.
struct MyFrontendAction : ASTFrontendAction {
  void EndSourceFileAction() override {
    //rw_.getEditBuffer(rw_.getSourceMgr().getMainFileID()).write(llvm::outs());
    //rw_.getSourceMgr().getMainFileID()

    auto& sm = rw_.getSourceMgr();
    for (auto iter = rw_.buffer_begin(); iter != rw_.buffer_end(); ++iter) {
      fmt::print(
        stderr, "Modified file %s\n",
        sm.getFileEntryForID(iter->first)->getName().str().c_str()
      );
    }

    rw_.overwriteChangedFiles();
  }

  std::unique_ptr<ASTConsumer>
  CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
    rw_.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());

    if (OutputMainFile) {
      auto buf = rw_.getSourceMgr().getBufferData(rw_.getSourceMgr().getMainFileID());
      fmt::print(out, "{}", buf.str());
    }

    return llvm::make_unique<MyASTConsumer>(rw_);
  }

private:
  Rewriter rw_;
};

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static cl::OptionCategory SerializeCheckerCategory("Serialize checker");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nGenerates static checking code for serialize methods\n");

int main(int argc, const char **argv) {
  CommonOptionsParser OptionsParser(argc, argv, SerializeCheckerCategory);

  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  if (Filename == "") {
    out = stdout;
  } else {
    out = fopen(Filename.c_str(), "w");
  }

  if (IncludeVTHeader) {
    fmt::print(out, "#include <vt/transport.h>\n");
  }

  for (auto&& e : Includes) {
    auto str = std::string("-I") + e;
    ArgumentsAdjuster ad1 = getInsertArgumentAdjuster(str.c_str());
    Tool.appendArgumentsAdjuster(ad1);
    fmt::print(stderr, "Including %s\n", e.c_str());
  }

  Tool.run(newFrontendActionFactory<MyFrontendAction>().get());

  if (Filename == "" and out != nullptr) {
    fclose(out);
  }
  return 0;
}

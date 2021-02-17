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
#include "clang/Frontend/FrontendPluginRegistry.h"

#include <memory>

#include <fmt/format.h>

#include "generator.h"
#include "walk_record.h"

using namespace clang;
using namespace llvm;
using namespace clang::tooling;
using namespace clang::ast_matchers;

static FILE* out = stdout;

static cl::opt<std::string> Filename("o", cl::desc("Filename to output generated code"));
static cl::opt<bool> GenerateInline("inline", cl::desc("Generate code inline and modify files"));
static cl::opt<bool> OutputMainFile("include-input", cl::desc("Output input file with generated code"));
static cl::opt<bool> IncludeVTHeader("Ivt", cl::desc("Include VT headers in generated code"));

DeclarationMatcher RecordMatcher = cxxRecordDecl().bind("recordDecl");

struct ClassFuncDeclRewriter : MatchFinder::MatchCallback {
  explicit ClassFuncDeclRewriter(Rewriter& in_rw)
    : rw(in_rw)
  { }

  virtual void run(MatchFinder::MatchResult const& result) {
    std::unique_ptr<sanitizer::Generator> gen = nullptr;

    if (GenerateInline) {
      gen = std::make_unique<sanitizer::InlineGenerator>(rw);
    } else {
      gen = std::make_unique<sanitizer::PartialSpecializationGenerator>(out);
    }

    auto t = std::make_unique<sanitizer::WalkRecord>(GenerateInline, std::move(gen));
    t->walk(result);
  }

private:
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
        stderr, "Modified file {}\n",
        sm.getFileEntryForID(iter->first)->getName().str()
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

    return std::make_unique<MyASTConsumer>(rw_);
  }

private:
  Rewriter rw_;
};

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nGenerates sanitizer code for serializers\n");

// int main(int argc, const char **argv) {
//   if (Filename == "") {
//     out = stdout;
//   } else {
//     out = fopen(Filename.c_str(), "w");
//   }

//   if (IncludeVTHeader) {
//     fmt::print(out, "#include <vt/transport.h>\n");
//   }

//   for (auto&& e : Includes) {
//     auto str = std::string("-I") + e;
//     ArgumentsAdjuster ad1 = getInsertArgumentAdjuster(str.c_str());
//     Tool.appendArgumentsAdjuster(ad1);
//     fmt::print(stderr, "Including {}\n", e);
//   }

//   Tool.run(newFrontendActionFactory<MyFrontendAction>().get());

//   if (Filename == "" and out != nullptr) {
//     fclose(out);
//   }
//   return 0;
// }

struct SanitizerPluginAction : public PluginASTAction {
public:
  bool ParseArgs(CompilerInstance const&,
                 std::vector<std::string> const& args) override {
    for (auto&& arg : args) {
      llvm::errs() << arg;
    }
    return true;
  }

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance&,
                                                 StringRef) override {
    return std::make_unique<MyASTConsumer>(rw_);
  }

private:
  Rewriter rw_;
};

// register the plugin
static FrontendPluginRegistry::Add<SanitizerPluginAction>
    X(/*Name=*/"sanitizer",
      /*Desc=*/"Serialization Sanitizer");

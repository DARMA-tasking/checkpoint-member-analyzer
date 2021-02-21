/*
//@HEADER
// *****************************************************************************
//
//                                 sanitizer.cc
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

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

#include <fmt/format.h>

#include <algorithm>
#include <memory>

#include "generator.h"
#include "sanitizer.h"
#include "walk_record.h"

using namespace clang;
using namespace clang::ast_matchers;

namespace sanitizer {

static FILE* out = stdout;
static bool GenerateInline = false;
static bool OutputMainFile = false;

struct SerializeRewriter : MatchFinder::MatchCallback {
  explicit SerializeRewriter(Rewriter& in_rw)
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

struct SanitizerASTConsumer : ASTConsumer {
  SanitizerASTConsumer(Rewriter& in_rw) : record_handler(in_rw) {
    // matches intrusive pattern
    auto record_matcher =
      cxxRecordDecl(
        has(functionTemplateDecl(
          hasName("serialize"), has(cxxMethodDecl(
            parameterCountIs(1)
          ).bind("serializeDecl"))
        ))
      ).bind("recordDecl");

    matcher_.addMatcher(record_matcher, &record_handler);
  }

  void HandleTranslationUnit(ASTContext& Context) override {
    // Run the matchers when we have the whole TU parsed.
    matcher_.matchAST(Context);
  }

private:
  SerializeRewriter record_handler;
  MatchFinder matcher_;
};


bool SanitizerPluginAction::ParseArgs(
  CompilerInstance const&, std::vector<std::string> const& args
) {
  // output input file with generated code
  if (std::find(args.begin(), args.end(), "-include-input") != args.end()) {
    OutputMainFile = true;
  }

  // filename to output generated code
  auto filename_opt = std::find(args.begin(), args.end(), "-o");
  if (filename_opt != args.end()) {
    out = fopen((++filename_opt)->c_str(), "w");
  }

  // generate code inline and modify files
  if (std::find(args.begin(), args.end(), "-inline") != args.end()) {
    GenerateInline = true;
  }

  // include VT headers in generated code
  if (std::find(args.begin(), args.end(), "-Ivt") != args.end()) {
    fmt::print(out, "#include <vt/transport.h>\n");
  }

  return true;
}

std::unique_ptr<ASTConsumer> SanitizerPluginAction::CreateASTConsumer(
  CompilerInstance &ci, StringRef file
) {
  rw_.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());

  if (OutputMainFile) {
    auto buf = rw_.getSourceMgr().getBufferData(rw_.getSourceMgr().getMainFileID());
    fmt::print(out, "{}", buf.str());
  }

  return std::make_unique<SanitizerASTConsumer>(rw_);
}

void SanitizerPluginAction::EndSourceFileAction() {
  auto& sm = rw_.getSourceMgr();
  for (auto iter = rw_.buffer_begin(); iter != rw_.buffer_end(); ++iter) {
    fmt::print(
      stderr, "Modified file {}\n",
      sm.getFileEntryForID(iter->first)->getName().str()
    );
  }

  rw_.overwriteChangedFiles();
  if (out != stdout) {
    fclose(out);
  }
}

} /* end namespace sanitizer */

// register the plugin
static FrontendPluginRegistry::Add<sanitizer::SanitizerPluginAction>
    X(/*Name=*/"sanitizer",
      /*Desc=*/"Serialization Sanitizer");

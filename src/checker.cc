
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

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;
using namespace llvm;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static cl::OptionCategory MyToolCategory("my-tool options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...\n");

DeclarationMatcher RecordMatcher = cxxRecordDecl().bind("recordDecl");

struct ClassFuncDeclRewriter : MatchFinder::MatchCallback {
  explicit ClassFuncDeclRewriter(Rewriter& in_rw)
    : rw(in_rw)
  { }

  virtual void run(const MatchFinder::MatchResult &Result) {
    if (CXXRecordDecl const *rd = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("recordDecl")) {
      //printf("Traversing class %s\n", rd->getQualifiedNameAsString().c_str());

      // We are in a template instantiation--skip!
      if (rd->getTemplateInstantiationPattern() != nullptr) {
        return;
      }

      FunctionDecl* fndecl = nullptr;
      bool has_serialize = false;
      bool already_contains_checks = false;

      // Go through the declarations for this struct
      for (auto&& m : rd->decls()) {
        //printf("DECL %s\n", m->getDeclKindName());

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
                      if (ce->child_begin() != ce->child_end()) {
                        if (isa<CXXDependentScopeMemberExpr>(*ce->child_begin())) {
                          auto cxx = cast<CXXDependentScopeMemberExpr>(*ce->child_begin());
                          if (cxx->getMemberNameInfo().getName().getAsString() == "check") {
                            //cxx->dump();
                            already_contains_checks = true;
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

      if (not already_contains_checks) {
        std::list<std::tuple<std::string, std::string>> members_to_serialize;

        auto record = rd->getQualifiedNameAsString();
        //printf("Processing record %s\n", record.c_str());
        for (auto&& f : rd->fields()) {
          auto member = f->getQualifiedNameAsString();
          auto unqualified_member = f->getNameAsString();
          printf("%s: %s\n", record.c_str(), member.c_str());
          f->dumpColor();
          members_to_serialize.push_back(std::make_tuple(unqualified_member, member));
        }


        if (members_to_serialize.size() > 0) {
          if (fndecl->hasBody()) {
            printf("inserting for: %s\n", record.c_str());
            auto body = fndecl->getBody();

            auto start = body->getLocEnd();
            rw.InsertText(start, "/* begin generated serialize check code */\n");
            for (auto&& elm : members_to_serialize) {
              rw.InsertText(
                start,
                "s.check(" + std::get<0>(elm) + "," "\"" + std::get<1>(elm) + "\"" ");\n"
              );
            }
            rw.InsertText(start, "/* end generated serialize check code */\n");
          }
        }
      }
    }
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
    rw_.overwriteChangedFiles();
  }

  std::unique_ptr<ASTConsumer>
  CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
    rw_.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return llvm::make_unique<MyASTConsumer>(rw_);
  }

private:
  Rewriter rw_;
};


int main(int argc, const char **argv) {
  CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
  return 0;
}

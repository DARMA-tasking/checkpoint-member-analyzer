
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

#include <memory>
#include <unordered_map>

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

// This doesn't work due to the template matching..
// DeclarationMatcher RecordMatcher = cxxRecordDecl(hasMethod(hasName("serialize"))).bind("recordDecl");

DeclarationMatcher RecordMatcher = cxxRecordDecl().bind("recordDecl");
StatementMatcher OpMatcher = binaryOperator(hasOperatorName("|")).bind("binOp");

std::unordered_map<std::string, std::list<std::string>> members_to_serialize;

struct ClassFuncDeclPrinter : MatchFinder::MatchCallback {
  virtual void run(const MatchFinder::MatchResult &Result) {
    if (CXXRecordDecl const *rd = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("recordDecl")) {
      //printf("Traversing class %s\n", rd->getQualifiedNameAsString().c_str());

      bool has_serialize = false;

      // Go through the declarations for this struct
      for (auto&& m : rd->decls()) {
        //printf("DECL %s\n", m->getDeclKindName());

        // Only look at template decls that are functions
        if (m->isTemplateDecl()) {
          auto fndecl = m->getAsFunction();
          if (fndecl) {
            if (fndecl->getNameAsString() == "serialize" and fndecl->param_size() == 1) {
              has_serialize = true;
              //fndecl->dump();
              //fndecl->getBody()
              // for (auto st : fndecl->getBody()->children()) {
              //   st->dump();
              // }
            }
          }
        }
      }

      if (has_serialize) {
        auto record = rd->getQualifiedNameAsString();
        for (auto&& f : rd->fields()) {
          //f->dump();
          auto member = f->getQualifiedNameAsString();
          printf("%s: %s\n", record.c_str(), member.c_str());
          members_to_serialize[record].push_back(member);
        }
      }

    }
  }
};


/*******************************************/

struct ClassFuncDeclRewriter : MatchFinder::MatchCallback {
  explicit ClassFuncDeclRewriter(Rewriter& in_rw)
    : rw(in_rw)
  { }

  virtual void run(const MatchFinder::MatchResult &Result) {
    if (CXXRecordDecl const *rd = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("recordDecl")) {
      auto record = rd->getQualifiedNameAsString();

      // Go through the declarations for this struct
      for (auto&& m : rd->decls()) {

        // Only look at template decls that are functions
        if (m->isTemplateDecl()) {
          auto fndecl = m->getAsFunction();
          if (fndecl) {
            if (fndecl->getNameAsString() == "serialize" and fndecl->param_size() == 1) {
              if (fndecl->getBody() && fndecl->getBody()->child_begin() != fndecl->getBody()->child_end()) {
                auto start = fndecl->getBody()->child_begin()->getLocStart();
                rw.InsertText(start, "/* begin generated serialize code */\n");
                for (auto&& elm : members_to_serialize[record]) {
                  rw.InsertText(start, "s.check(" + elm + "," "\"" + elm + "\"" ");\n");
                }
                rw.InsertText(start, "/* end generated serialize code */\n");
              }

              //rw.InsertText(fndecl->getLocStart(), "test");
            }
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

  {
    ClassFuncDeclPrinter findSerializeRecords;
    MatchFinder Finder;
    Finder.addMatcher(RecordMatcher, &findSerializeRecords);

    Tool.run(newFrontendActionFactory(&Finder).get());
  }

  {
    Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
  }
  return 0;
}

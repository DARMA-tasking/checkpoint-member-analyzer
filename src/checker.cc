
// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

#include <memory>

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

struct ClassFuncDeclPrinter : MatchFinder::MatchCallback {
  virtual void run(const MatchFinder::MatchResult &Result) {
    if (CXXRecordDecl const *rd = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("recordDecl")) {
      //rd->dump();

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
        for (auto&& f : rd->fields()) {
          //f->dump();
          printf("%s: %s\n",rd->getQualifiedNameAsString().c_str(), f->getQualifiedNameAsString().c_str());
        }

      }

      // for (auto&& m : rd->redecls()) {
      //   m->dump();
      // }
      // for (auto&& m : rd->methods()) {
      //   m->dump();
      // }
      // for (auto&& f : rd->fields()) {
      //   f->dump();
      // }
    }
  }
};

int main(int argc, const char **argv) {
  CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  ClassFuncDeclPrinter findSerializeRecords;
  MatchFinder Finder;
  Finder.addMatcher(RecordMatcher, &findSerializeRecords);

  return Tool.run(newFrontendActionFactory(&Finder).get());
}

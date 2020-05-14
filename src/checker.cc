
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
#include <unordered_map>
#include <unordered_set>
#include <fstream>

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
StatementMatcher CheckMatcher = callExpr(anything()).bind("checkMatch");

using ListType = std::list<std::tuple<std::string, std::string>>;
std::unordered_map<std::string, ListType> members_to_serialize;
std::unordered_set<std::string> inserted;

struct ClassFuncDeclCheckFinder : MatchFinder::MatchCallback {
  virtual void run(const MatchFinder::MatchResult &Result) {
    printf("run\n");
    if (CallExpr const *ce = Result.Nodes.getNodeAs<clang::CallExpr>("checkMatch")) {
      if (ce->getCallee()->isTypeDependent() and ce->getNumArgs() == 2) {
        if (ce->child_begin() != ce->child_end()) {
          auto cxx =cast_or_null<CXXDependentScopeMemberExpr>(*ce->child_begin());
          printf("%s\n",cxx->getMemberNameInfo().getName().getAsString().c_str());
        }
      }
    }
  }
};

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
            }
          }
        }
      }

      if (has_serialize) {
        auto record = rd->getQualifiedNameAsString();
        for (auto&& f : rd->fields()) {
          //f->dump();
          auto member = f->getQualifiedNameAsString();
          auto unqualified_member = f->getNameAsString();
          printf("%s: %s\n", record.c_str(), member.c_str());
          members_to_serialize[record].push_back(
            std::make_tuple(unqualified_member, member)
          );
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

                bool already_inserted = false;
                auto body = fndecl->getBody();

#if 0
                for (auto iter = body->child_begin(); iter != body->child_end(); ++iter) {
                  if (iter->getStmtClass() == Stmt::StmtClass::CallExprClass) {
                    auto ce = cast_or_null<CallExpr>(*iter);

                    //printf("FOUND CE %s\n", record.c_str());

                    if (ce && ce->getCallee()->isTypeDependent() and ce->getNumArgs() == 2) {
                      if (ce->child_begin() != ce->child_end()) {
                        auto cxx = cast_or_null<CXXDependentScopeMemberExpr>(*ce->child_begin());
                        if (cxx->getMemberNameInfo().getName().getAsString() == "check") {
                          already_inserted = true;
                        }
                        // printf(
                        //   "FOUND in %s: %s\n",
                        //   record.c_str(),
                        //   cxx->getMemberNameInfo().getName().getAsString().c_str()
                        // );
                      }
                    }
                  }
                }
#endif

                if (not already_inserted) {
                  if (inserted.find(record) == inserted.end()) {
                    printf("inserting for: %s\n", record.c_str());
                    inserted.insert(record);

                    auto start = body->getLocEnd();
                    //auto start = place->getLocStart();
                    rw.InsertText(start, "/* begin generated serialize code */\n");
                    for (auto&& elm : members_to_serialize[record]) {
                      rw.InsertText(
                        start,
                        "s.check(" + std::get<0>(elm) + "," "\"" + std::get<1>(elm) + "\"" ");\n"
                      );
                    }
                    rw.InsertText(start, "/* end generated serialize code */\n");
                  }
                }
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

    printf("writing file\n");
    std::ofstream file("/Users/jliffla/output.txt");
    if (not file.good()) {
      printf("FAIL writing file\n");
    }
    for (auto&& elm : inserted) {
      file << elm << "\n";
    }
    file.close();
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

#if 0
  {
    ClassFuncDeclCheckFinder findChecker;
    MatchFinder Finder;
    Finder.addMatcher(CheckMatcher, &findChecker);

    Tool.run(newFrontendActionFactory(&Finder).get());
  }
#endif

  {
    std::ifstream input("/Users/jliffla/output.txt");
    if (input.good()) {
      while (not input.eof()) {
        std::string elm;
        input >> elm;
        inserted.insert(elm);
      }
    }
  }

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

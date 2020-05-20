
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

// This moved in later versions of clang
#include "clang/Tooling/Core/QualTypeNames.h"
//#include "clang/AST/QualTypeNames.h"

#include <memory>
#include <list>
#include <tuple>
#include <unordered_set>

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;
using namespace llvm;

namespace clang {

namespace TypeName2 {
/// \brief Generates a QualType that can be used to name the same type
/// if used at the end of the current translation unit. This ignores
/// issues such as type shadowing.
///
/// \param[in] QT - the type for which the fully qualified type will be
/// returned.
/// \param[in] Ctx - the ASTContext to be used.
/// \param[in] WithGlobalNsPrefix - Indicate whether the global namespace
/// specifier "::" should be prepended or not.
static QualType getFullyQualifiedType(QualType QT, const ASTContext &Ctx,
                                      bool WithGlobalNsPrefix);

/// \brief Create a NestedNameSpecifier for Namesp and its enclosing
/// scopes.
///
/// \param[in] Ctx - the AST Context to be used.
/// \param[in] Namesp - the NamespaceDecl for which a NestedNameSpecifier
/// is requested.
/// \param[in] WithGlobalNsPrefix - Indicate whether the global namespace
/// specifier "::" should be prepended or not.
static NestedNameSpecifier *createNestedNameSpecifier(
    const ASTContext &Ctx,
    const NamespaceDecl *Namesp,
    bool WithGlobalNsPrefix);

/// \brief Create a NestedNameSpecifier for TagDecl and its enclosing
/// scopes.
///
/// \param[in] Ctx - the AST Context to be used.
/// \param[in] TD - the TagDecl for which a NestedNameSpecifier is
/// requested.
/// \param[in] FullyQualify - Convert all template arguments into fully
/// qualified names.
/// \param[in] WithGlobalNsPrefix - Indicate whether the global namespace
/// specifier "::" should be prepended or not.
static NestedNameSpecifier *createNestedNameSpecifier(
    const ASTContext &Ctx, const TypeDecl *TD,
    bool FullyQualify, bool WithGlobalNsPrefix);

static NestedNameSpecifier *createNestedNameSpecifierForScopeOf(
    const ASTContext &Ctx, const Decl *decl,
    bool FullyQualified, bool WithGlobalNsPrefix);

static NestedNameSpecifier *getFullyQualifiedNestedNameSpecifier(
    const ASTContext &Ctx, NestedNameSpecifier *scope, bool WithGlobalNsPrefix);

static bool getFullyQualifiedTemplateName(const ASTContext &Ctx,
                                          TemplateName &TName,
                                          bool WithGlobalNsPrefix) {
  bool Changed = false;
  NestedNameSpecifier *NNS = nullptr;

  TemplateDecl *ArgTDecl = TName.getAsTemplateDecl();
  // ArgTDecl won't be NULL because we asserted that this isn't a
  // dependent context very early in the call chain.
  assert(ArgTDecl != nullptr);
  QualifiedTemplateName *QTName = TName.getAsQualifiedTemplateName();

  if (QTName && !QTName->hasTemplateKeyword()) {
    NNS = QTName->getQualifier();
    NestedNameSpecifier *QNNS = getFullyQualifiedNestedNameSpecifier(
        Ctx, NNS, WithGlobalNsPrefix);
    if (QNNS != NNS) {
      Changed = true;
      NNS = QNNS;
    } else {
      NNS = nullptr;
    }
  } else {
    NNS = createNestedNameSpecifierForScopeOf(
        Ctx, ArgTDecl, true, WithGlobalNsPrefix);
  }
  if (NNS) {
    TName = Ctx.getQualifiedTemplateName(NNS,
                                         /*TemplateKeyword=*/false, ArgTDecl);
    Changed = true;
  }
  return Changed;
}

static bool getFullyQualifiedTemplateArgument(const ASTContext &Ctx,
                                              TemplateArgument &Arg,
                                              bool WithGlobalNsPrefix) {
  bool Changed = false;

  // Note: we do not handle TemplateArgument::Expression, to replace it
  // we need the information for the template instance decl.

  if (Arg.getKind() == TemplateArgument::Template) {
    TemplateName TName = Arg.getAsTemplate();
    Changed = getFullyQualifiedTemplateName(Ctx, TName, WithGlobalNsPrefix);
    if (Changed) {
      Arg = TemplateArgument(TName);
    }
  } else if (Arg.getKind() == TemplateArgument::Type) {
    QualType SubTy = Arg.getAsType();
    // Check if the type needs more desugaring and recurse.
    QualType QTFQ = getFullyQualifiedType(SubTy, Ctx, WithGlobalNsPrefix);

    if (QTFQ != SubTy) {
      Arg = TemplateArgument(QTFQ);
      Changed = true;
    }
  } else if (Arg.getKind() == TemplateArgument::Pack) {
    assert(0 && "Should not be reachable");
  }

  return Changed;
}

static const Type *getFullyQualifiedTemplateType(const ASTContext &Ctx,
                                                 const Type *TypePtr,
                                                 bool WithGlobalNsPrefix) {
  //printf("getFQTT(): %s\n", QualType(TypePtr, 0).getAsString().c_str());

  // DependentTemplateTypes exist within template declarations and
  // definitions. Therefore we shouldn't encounter them at the end of
  // a translation unit. If we do, the caller has made an error.
  assert(!isa<DependentTemplateSpecializationType>(TypePtr));
  // In case of template specializations, iterate over the arguments
  // and fully qualify them as well.
  if (const auto *TST = dyn_cast<const TemplateSpecializationType>(TypePtr)) {
    //printf("getFQTT() is TemplateSpecializationType\n");

    bool MightHaveChanged = false;
    SmallVector<TemplateArgument, 4> FQArgs;
    for (TemplateSpecializationType::iterator I = TST->begin(), E = TST->end();
         I != E; ++I) {
      // Cheap to copy and potentially modified by
      // getFullyQualifedTemplateArgument.
      TemplateArgument Arg(*I);
      MightHaveChanged |= getFullyQualifiedTemplateArgument(
          Ctx, Arg, WithGlobalNsPrefix);
      FQArgs.push_back(Arg);
    }

    // If a fully qualified arg is different from the unqualified arg,
    // allocate new type in the AST.
    if (MightHaveChanged) {
      QualType QT = Ctx.getTemplateSpecializationType(
          TST->getTemplateName(), FQArgs,
          TST->getCanonicalTypeInternal());
      // getTemplateSpecializationType returns a fully qualified
      // version of the specialization itself, so no need to qualify
      // it.
      return QT.getTypePtr();
    }
  } else if (const auto *TSTRecord = dyn_cast<const RecordType>(TypePtr)) {
    //printf("getFQTT() is RecordType\n");

    // We are asked to fully qualify and we have a Record Type,
    // which can point to a template instantiation with no sugar in any of
    // its template argument, however we still need to fully qualify them.

    if (const auto *TSTDecl =
        dyn_cast<ClassTemplateSpecializationDecl>(TSTRecord->getDecl())) {
      const TemplateArgumentList &TemplateArgs = TSTDecl->getTemplateArgs();

      bool MightHaveChanged = false;
      SmallVector<TemplateArgument, 4> FQArgs;
      for (unsigned int I = 0, E = TemplateArgs.size(); I != E; ++I) {
        // cheap to copy and potentially modified by
        // getFullyQualifedTemplateArgument
        TemplateArgument Arg(TemplateArgs[I]);
        if (Arg.getKind() == TemplateArgument::Pack) {
          for (auto piter = Arg.pack_begin(); piter != Arg.pack_end(); ++piter) {
            TemplateArgument Arg2(*piter);
            MightHaveChanged |= getFullyQualifiedTemplateArgument(
              Ctx, Arg2, WithGlobalNsPrefix);
            FQArgs.push_back(Arg2);
          }
        } else {
          MightHaveChanged |= getFullyQualifiedTemplateArgument(
            Ctx, Arg, WithGlobalNsPrefix);
          FQArgs.push_back(Arg);
        }
        // if (Arg.getKind() == TemplateArgument::ArgKind::Type) {
        //   printf("dump22: %d %s\n", I, Arg.getAsType().getAsString().c_str());
        // }
      }

      // If a fully qualified arg is different from the unqualified arg,
      // allocate new type in the AST.
      if (MightHaveChanged or true) {
        TemplateName TN(TSTDecl->getSpecializedTemplate());
        QualType QT = Ctx.getTemplateSpecializationType(
            TN, FQArgs,
            TSTRecord->getCanonicalTypeInternal());
        //printf("QT: %s\n", QT.getAsString().c_str());
        // getTemplateSpecializationType returns a fully qualified
        // version of the specialization itself, so no need to qualify
        // it.
        return QT.getTypePtr();
      }
    }
  }

  //printf("getFQTT() is nothing checked\n");

  return TypePtr;
}

static NestedNameSpecifier *createOuterNNS(const ASTContext &Ctx, const Decl *D,
                                           bool FullyQualify,
                                           bool WithGlobalNsPrefix) {
  const DeclContext *DC = D->getDeclContext();
  if (const auto *NS = dyn_cast<NamespaceDecl>(DC)) {
    while (NS && NS->isInline()) {
      // Ignore inline namespace;
      NS = dyn_cast<NamespaceDecl>(NS->getDeclContext());
    }
    if (NS->getDeclName()) {
      return createNestedNameSpecifier(Ctx, NS, WithGlobalNsPrefix);
    }
    return nullptr;  // no starting '::', no anonymous
  } else if (const auto *TD = dyn_cast<TagDecl>(DC)) {
    return createNestedNameSpecifier(Ctx, TD, FullyQualify, WithGlobalNsPrefix);
  } else if (const auto *TDD = dyn_cast<TypedefNameDecl>(DC)) {
    return createNestedNameSpecifier(
        Ctx, TDD, FullyQualify, WithGlobalNsPrefix);
  } else if (WithGlobalNsPrefix && DC->isTranslationUnit()) {
    return NestedNameSpecifier::GlobalSpecifier(Ctx);
  }
  return nullptr;  // no starting '::' if |WithGlobalNsPrefix| is false
}

/// \brief Return a fully qualified version of this name specifier.
static NestedNameSpecifier *getFullyQualifiedNestedNameSpecifier(
    const ASTContext &Ctx, NestedNameSpecifier *Scope,
    bool WithGlobalNsPrefix) {
  switch (Scope->getKind()) {
    case NestedNameSpecifier::Global:
      // Already fully qualified
      return Scope;
    case NestedNameSpecifier::Namespace:
      return TypeName2::createNestedNameSpecifier(
          Ctx, Scope->getAsNamespace(), WithGlobalNsPrefix);
    case NestedNameSpecifier::NamespaceAlias:
      // Namespace aliases are only valid for the duration of the
      // scope where they were introduced, and therefore are often
      // invalid at the end of the TU.  So use the namespace name more
      // likely to be valid at the end of the TU.
      return TypeName2::createNestedNameSpecifier(
          Ctx,
          Scope->getAsNamespaceAlias()->getNamespace()->getCanonicalDecl(),
          WithGlobalNsPrefix);
    case NestedNameSpecifier::Identifier:
      // A function or some other construct that makes it un-namable
      // at the end of the TU. Skip the current component of the name,
      // but use the name of it's prefix.
      return getFullyQualifiedNestedNameSpecifier(
          Ctx, Scope->getPrefix(), WithGlobalNsPrefix);
    case NestedNameSpecifier::Super:
    case NestedNameSpecifier::TypeSpec:
    case NestedNameSpecifier::TypeSpecWithTemplate: {
      const Type *Type = Scope->getAsType();
      // Find decl context.
      const TagDecl *TD = nullptr;
      if (const TagType *TagDeclType = Type->getAs<TagType>()) {
        TD = TagDeclType->getDecl();
      } else {
        TD = Type->getAsCXXRecordDecl();
      }
      if (TD) {
        return TypeName2::createNestedNameSpecifier(Ctx, TD,
                                                   true /*FullyQualified*/,
                                                   WithGlobalNsPrefix);
      } else if (const auto *TDD = dyn_cast<TypedefType>(Type)) {
        return TypeName2::createNestedNameSpecifier(Ctx, TDD->getDecl(),
                                                   true /*FullyQualified*/,
                                                   WithGlobalNsPrefix);
      }
      return Scope;
    }
  }
  llvm_unreachable("bad NNS kind");
}

/// \brief Create a nested name specifier for the declaring context of
/// the type.
static NestedNameSpecifier *createNestedNameSpecifierForScopeOf(
    const ASTContext &Ctx, const Decl *Decl,
    bool FullyQualified, bool WithGlobalNsPrefix) {
  assert(Decl);

  const DeclContext *DC = Decl->getDeclContext()->getRedeclContext();
  const auto *Outer = dyn_cast_or_null<NamedDecl>(DC);
  const auto *OuterNS = dyn_cast_or_null<NamespaceDecl>(DC);
  if (Outer && !(OuterNS && OuterNS->isAnonymousNamespace())) {
    if (const auto *CxxDecl = dyn_cast<CXXRecordDecl>(DC)) {
      if (ClassTemplateDecl *ClassTempl =
              CxxDecl->getDescribedClassTemplate()) {
        // We are in the case of a type(def) that was declared in a
        // class template but is *not* type dependent.  In clang, it
        // gets attached to the class template declaration rather than
        // any specific class template instantiation.  This result in
        // 'odd' fully qualified typename:
        //
        //    vector<_Tp,_Alloc>::size_type
        //
        // Make the situation is 'useable' but looking a bit odd by
        // picking a random instance as the declaring context.
        if (ClassTempl->spec_begin() != ClassTempl->spec_end()) {
          Decl = *(ClassTempl->spec_begin());
          Outer = dyn_cast<NamedDecl>(Decl);
          OuterNS = dyn_cast<NamespaceDecl>(Decl);
        }
      }
    }

    if (OuterNS) {
      return createNestedNameSpecifier(Ctx, OuterNS, WithGlobalNsPrefix);
    } else if (const auto *TD = dyn_cast<TagDecl>(Outer)) {
      return createNestedNameSpecifier(
          Ctx, TD, FullyQualified, WithGlobalNsPrefix);
    } else if (dyn_cast<TranslationUnitDecl>(Outer)) {
      // Context is the TU. Nothing needs to be done.
      return nullptr;
    } else {
      // Decl's context was neither the TU, a namespace, nor a
      // TagDecl, which means it is a type local to a scope, and not
      // accessible at the end of the TU.
      return nullptr;
    }
  } else if (WithGlobalNsPrefix && DC->isTranslationUnit()) {
    return NestedNameSpecifier::GlobalSpecifier(Ctx);
  }
  return nullptr;
}

/// \brief Create a nested name specifier for the declaring context of
/// the type.
static NestedNameSpecifier *createNestedNameSpecifierForScopeOf(
    const ASTContext &Ctx, const Type *TypePtr,
    bool FullyQualified, bool WithGlobalNsPrefix) {
  if (!TypePtr) return nullptr;

  Decl *Decl = nullptr;
  // There are probably other cases ...
  if (const auto *TDT = dyn_cast<TypedefType>(TypePtr)) {
    Decl = TDT->getDecl();
  } else if (const auto *TagDeclType = dyn_cast<TagType>(TypePtr)) {
    Decl = TagDeclType->getDecl();
  } else if (const auto *TST = dyn_cast<TemplateSpecializationType>(TypePtr)) {
    Decl = TST->getTemplateName().getAsTemplateDecl();
  } else {
    Decl = TypePtr->getAsCXXRecordDecl();
  }

  if (!Decl) return nullptr;

  return createNestedNameSpecifierForScopeOf(
      Ctx, Decl, FullyQualified, WithGlobalNsPrefix);
}

NestedNameSpecifier *createNestedNameSpecifier(const ASTContext &Ctx,
                                               const NamespaceDecl *Namespace,
                                               bool WithGlobalNsPrefix) {
  while (Namespace && Namespace->isInline()) {
    // Ignore inline namespace;
    Namespace = dyn_cast<NamespaceDecl>(Namespace->getDeclContext());
  }
  if (!Namespace) return nullptr;

  bool FullyQualified = true;  // doesn't matter, DeclContexts are namespaces
  return NestedNameSpecifier::Create(
      Ctx,
      createOuterNNS(Ctx, Namespace, FullyQualified, WithGlobalNsPrefix),
      Namespace);
}

NestedNameSpecifier *createNestedNameSpecifier(const ASTContext &Ctx,
                                               const TypeDecl *TD,
                                               bool FullyQualify,
                                               bool WithGlobalNsPrefix) {
  return NestedNameSpecifier::Create(
      Ctx,
      createOuterNNS(Ctx, TD, FullyQualify, WithGlobalNsPrefix),
      false /*No TemplateKeyword*/,
      TD->getTypeForDecl());
}

/// \brief Return the fully qualified type, including fully-qualified
/// versions of any template parameters.
QualType getFullyQualifiedType(QualType QT, const ASTContext &Ctx,
                               bool WithGlobalNsPrefix) {
  // In case of myType* we need to strip the pointer first, fully
  // qualify and attach the pointer once again.
  if (isa<PointerType>(QT.getTypePtr())) {
    // Get the qualifiers.
    Qualifiers Quals = QT.getQualifiers();
    QT = getFullyQualifiedType(QT->getPointeeType(), Ctx, WithGlobalNsPrefix);
    QT = Ctx.getPointerType(QT);
    // Add back the qualifiers.
    QT = Ctx.getQualifiedType(QT, Quals);
    return QT;
  }

  // In case of myType& we need to strip the reference first, fully
  // qualify and attach the reference once again.
  if (isa<ReferenceType>(QT.getTypePtr())) {
    // Get the qualifiers.
    bool IsLValueRefTy = isa<LValueReferenceType>(QT.getTypePtr());
    Qualifiers Quals = QT.getQualifiers();
    QT = getFullyQualifiedType(QT->getPointeeType(), Ctx, WithGlobalNsPrefix);
    // Add the r- or l-value reference type back to the fully
    // qualified one.
    if (IsLValueRefTy)
      QT = Ctx.getLValueReferenceType(QT);
    else
      QT = Ctx.getRValueReferenceType(QT);
    // Add back the qualifiers.
    QT = Ctx.getQualifiedType(QT, Quals);
    return QT;
  }

  // Remove the part of the type related to the type being a template
  // parameter (we won't report it as part of the 'type name' and it
  // is actually make the code below to be more complex (to handle
  // those)
  while (isa<SubstTemplateTypeParmType>(QT.getTypePtr())) {
    // Get the qualifiers.
    Qualifiers Quals = QT.getQualifiers();

    QT = dyn_cast<SubstTemplateTypeParmType>(QT.getTypePtr())->desugar();

    // Add back the qualifiers.
    QT = Ctx.getQualifiedType(QT, Quals);
  }

  NestedNameSpecifier *Prefix = nullptr;
  // Local qualifiers are attached to the QualType outside of the
  // elaborated type.  Retrieve them before descending into the
  // elaborated type.
  Qualifiers PrefixQualifiers = QT.getLocalQualifiers();
  QT = QualType(QT.getTypePtr(), 0);
  ElaboratedTypeKeyword Keyword = ETK_None;
  if (const auto *ETypeInput = dyn_cast<ElaboratedType>(QT.getTypePtr())) {
    QT = ETypeInput->getNamedType();
    assert(!QT.hasLocalQualifiers());
    Keyword = ETypeInput->getKeyword();
  }
  // Create a nested name specifier if needed.
  Prefix = createNestedNameSpecifierForScopeOf(Ctx, QT.getTypePtr(),
                                               true /*FullyQualified*/,
                                               WithGlobalNsPrefix);

  // In case of template specializations iterate over the arguments and
  // fully qualify them as well.
  if (isa<const TemplateSpecializationType>(QT.getTypePtr()) ||
      isa<const RecordType>(QT.getTypePtr())) {
    // We are asked to fully qualify and we have a Record Type (which
    // may point to a template specialization) or Template
    // Specialization Type. We need to fully qualify their arguments.

    const Type *TypePtr = getFullyQualifiedTemplateType(
        Ctx, QT.getTypePtr(), WithGlobalNsPrefix);
    QT = QualType(TypePtr, 0);
  }
  if (Prefix || Keyword != ETK_None) {
    QT = Ctx.getElaboratedType(Keyword, Prefix, QT);
  }
  QT = Ctx.getQualifiedType(QT, PrefixQualifiers);
  return QT;
}

std::string getFullyQualifiedName(QualType QT,
                                  const ASTContext &Ctx,
                                  bool WithGlobalNsPrefix) {
  PrintingPolicy Policy(Ctx.getPrintingPolicy());
  Policy.SuppressScope = false;
  Policy.AnonymousTagLocations = false;
  Policy.PolishForDeclaration = true;
  Policy.SuppressUnwrittenScope = true;
  QualType FQQT = getFullyQualifiedType(QT, Ctx, WithGlobalNsPrefix);
  return FQQT.getAsString(Policy);
}

}  // end namespace TypeName2
}  // end namespace clang


// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static cl::OptionCategory MyToolCategory("my-tool options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...\n");

static bool generate_checks_inline = false;

DeclarationMatcher RecordMatcher = cxxRecordDecl().bind("recordDecl");

struct ClassFuncDeclRewriter : MatchFinder::MatchCallback {
  using MemberListType = std::list<std::tuple<std::string, std::string>>;
  using CXXDeclType = CXXRecordDecl const*;

  explicit ClassFuncDeclRewriter(Rewriter& in_rw)
    : rw(in_rw)
  { }

  virtual void run(const MatchFinder::MatchResult &Result) {
    if (CXXRecordDecl const *rd = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("recordDecl")) {
      //printf("Traversing class %s\n", rd->getQualifiedNameAsString().c_str());

      // We are in a template instantiation--skip!
      if (generate_checks_inline) {
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
      //printf("Finding fields for CXX record %s\n", record.c_str());
      for (auto&& f : rd->fields()) {
        auto member = f->getQualifiedNameAsString();
        auto unqualified_member = f->getNameAsString();

        auto existing_iter = existing_checks_.find(unqualified_member);
        if (existing_iter == existing_checks_.end()) {
          //printf("%s: %s\n", record.c_str(), member.c_str());
          //f->dumpColor();
          members_to_serialize.push_back(std::make_tuple(unqualified_member, member));
        }
      }

      if (generate_checks_inline) {
        if (not fndecl->hasBody() and members_to_serialize.size() > 0) {
          printf(
            "%s: %zu members exist, but no serialize body found!\n",
            record.c_str(), members_to_serialize.size()
          );
          return;
        }
      }

      if (members_to_serialize.size() > 0) {
        if (generate_checks_inline) {
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
      printf("Inserting new checks for: %s\n", record.c_str());
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
    //printf("%s: SPECIALIZATION: %d\n", rd->getQualifiedNameAsString().c_str(), rd->getTemplateSpecializationKind());

    TemplateSpecializationKind kind = rd->getTemplateSpecializationKind();

    if (kind == TemplateSpecializationKind::TSK_Undeclared) {
      auto qual_name = rd->getQualifiedNameAsString();

      printf("template <>\n");
      printf("void %s::serialize<checkpoint::dispatch::Counter>(checkpoint::dispatch::Counter& s) {\n", qual_name.c_str());
      for (auto&& m : members) {
        printf("\ts.check(%s, \"%s\");\n", std::get<0>(m).c_str(), std::get<1>(m).c_str());
      }
      printf("}\n");
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

        printf("template <>\n");
        printf("template <>\n");
        printf("void %s::serialize<checkpoint::dispatch::Counter>(checkpoint::dispatch::Counter& s) {\n",
               qualified_type_outer.c_str());
        for (auto&& m : members) {
          printf("\ts.check(%s, \"%s\");\n", std::get<0>(m).c_str(), std::get<1>(m).c_str());
        }
        printf("}\n");
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
          printf("TYPE TPARAM: %s\n", tparam->getNameAsString().c_str());
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

    //printf("TEMPLATE: %s\n", template_str.c_str())

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

    printf("template <typename SerializerT%s>\n", template_decl_context.c_str());
    printf(
      "void serializeCheck(SerializerT& s, %s%s& obj) {\n",
      qual_name.c_str(), template_def_context.c_str()
    );
    for (auto&& m : members) {
      printf("\ts.check(obj.%s, \"%s\");\n", std::get<0>(m).c_str(), std::get<1>(m).c_str());
    }
    printf("}\n\n");
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

    auto& sm = rw_.getSourceMgr();
    for (auto iter = rw_.buffer_begin(); iter != rw_.buffer_end(); ++iter) {
      printf("Modified file %s\n", sm.getFileEntryForID(iter->first)->getName().str().c_str());
    }

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

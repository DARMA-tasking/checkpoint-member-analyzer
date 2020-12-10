/*
//@HEADER
// *****************************************************************************
//
//                                 generator.h
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

#if !defined INCLUDED_SANITIZER_GENERATOR_H
#define INCLUDED_SANITIZER_GENERATOR_H

#include "common.h"
#include "member_list.h"

#include "clang/AST/ExprCXX.h"
#include "clang/Rewrite/Core/Rewriter.h"

namespace sanitizer {

/**
 * \struct Generator
 *
 * \brief Abstract code generator for sanitizer
 */
struct Generator {

  virtual ~Generator() = default;

  /**
   * \brief Run the generator on a specific class with the fields
   *
   * \param[in] rd the class
   * \param[in] fn the serialize method
   * \param[in] members the list of fields in the class
   */
  virtual void run(
    clang::CXXRecordDecl const* rd, clang::FunctionDecl* fn,
    MemberListType members
  ) = 0;

};

/**
 * \struct InlineGenerator
 *
 * \brief Generates checks in the serialize method directly. Requires
 * modification of source files.
 */
struct InlineGenerator : Generator {

  explicit InlineGenerator(clang::Rewriter& in_rw)
    : rw_(in_rw)
  { }

  void run(
    clang::CXXRecordDecl const* rd, clang::FunctionDecl* fn,
    MemberListType members
  ) override;

private:
  clang::Rewriter& rw_;
};

/**
 * \struct PartialSpecializationGenerator
 *
 * \brief Generates checks in a partial specialization of the serialize method.
 */
struct PartialSpecializationGenerator : Generator {

  explicit PartialSpecializationGenerator(FILE* in_out)
    : out_(in_out)
  { }

  void run(
    clang::CXXRecordDecl const* rd, clang::FunctionDecl* fn,
    MemberListType members
  ) override;

private:
  FILE* out_ = nullptr;
};

/**
 * \struct SeperateGenerator
 *
 * \brief Generates checks as a completely separate method (experimental).
 */
struct SeperateGenerator : Generator {

  void run(
    clang::CXXRecordDecl const* rd, clang::FunctionDecl* fn,
    MemberListType members
  ) override;

};

} /* end namespace sanitizer */

#endif /*INCLUDED_SANITIZER_GENERATOR_H*/

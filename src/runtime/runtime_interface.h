/*
//@HEADER
// *****************************************************************************
//
//                             runtime_interface.h
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

#if !defined INCLUDED_SANITIZER_RUNTIME_RUNTIME_INTERFACE_H
#define INCLUDED_SANITIZER_RUNTIME_RUNTIME_INTERFACE_H

#include <string>
#include <memory>

namespace checkpoint { namespace sanitizer {

/**
 * \struct Runtime
 *
 * \brief Base class for sanitizer runtime implemented by the sanitizer
 */
struct Runtime {

  virtual ~Runtime() = default;

  /**
   * \brief Check that a member is serialized
   *
   * \param[in] addr the memory address of the element
   * \param[in] name the name of the element
   * \param[in] tinfo the typeinfo of the element
   */
  virtual void checkMember(void* addr, std::string name, std::string tinfo) {}

  /**
   * \brief Inform sanitizer that a member's serialization is skipped
   *
   * \param[in] addr the memory address of the element
   * \param[in] name the name of the element
   * \param[in] tinfo the typeinfo of the element
   */
  virtual void skipMember(void* addr, std::string name, std::string tinfo) {}

  /**
   * \brief Inform sanitizer that a member is serialized
   *
   * \param[in] addr the memory address of the element
   * \param[in] num the number of elements
   * \param[in] tinfo the typeinfo of the element
   */
  virtual void isSerialized(void* addr, std::size_t num, std::string tinfo) {}

  /**
   * \brief Push a stack frame of the current serializer context we are entering
   *
   * \param[in] tinfo the name of the type recursed into
   */
  virtual void push(std::string tinfo) {}

  /**
   * \brief Pop a stack frame of the current serializer context we are leaving
   *
   * \param[in] tinfo the name of the type recursed out of
   */
  virtual void pop(std::string tinfo) {}

};

/// pimpl to runtime that contains runtime sanitizer logic
extern std::unique_ptr<Runtime> rt_;

}} /* end namespace checkpoint::sanitizer*/

#endif /*INCLUDED_SANITIZER_RUNTIME_RUNTIME_INTERFACE_H*/

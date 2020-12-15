/*
//@HEADER
// *****************************************************************************
//
//                                stack_record.h
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

#if !defined INCLUDED_SANITIZER_RUNTIME_STACK_RECORD_H
#define INCLUDED_SANITIZER_RUNTIME_STACK_RECORD_H

#include <unordered_set>
#include <string>
#include <tuple>

namespace checkpoint { namespace sanitizer {

struct PtrName {

  PtrName(void* in_addr, std::string const& in_name)
    : addr(in_addr),
      name(in_name)
  { }

  void* addr = nullptr;
  std::string name = "";

  friend bool operator==(PtrName const& a, PtrName const& b) {
    return a.addr == b.addr;
  }
};

struct PtrNameType : PtrName {

  PtrNameType(
    void* in_addr, std::string const& in_name, std::string const& in_tinfo
  ) : PtrName(in_addr, in_name),
      tinfo(in_tinfo)
  { }

  std::string tinfo = "";

  friend bool operator==(PtrNameType const& a, PtrNameType const& b) {
    return static_cast<PtrName>(a) == static_cast<PtrName>(b);
  }
};

}} /* end namespace checkpoint::sanitizer */

namespace std {

template <>
struct hash<checkpoint::sanitizer::PtrName> {
  std::size_t operator()(checkpoint::sanitizer::PtrName const& p) const {
    return
      std::hash<intptr_t>{}(reinterpret_cast<intptr_t>(p.addr)) ^
      std::hash<std::string>{}(p.name);
  }
};

template <>
struct hash<checkpoint::sanitizer::PtrNameType> {
  std::size_t operator()(checkpoint::sanitizer::PtrNameType const& p) const {
    return
      std::hash<std::string>{}(p.tinfo) ^
      std::hash<checkpoint::sanitizer::PtrName>{}(p);
  }
};

} /* end namespace std */

namespace checkpoint { namespace sanitizer {

struct StackRecord {
  explicit StackRecord(std::string const& in_name)
    : name_(in_name)
  { }

  void isSerialized(void* addr, std::string name) {
    is_serialized_.emplace(PtrName{addr, name});
  }

  void checkElm(void* addr, std::string const& name, std::string const& tinfo) {
    check_.emplace(PtrNameType{addr, name, tinfo});
  }

  void ignoreElm(void* addr, std::string const& name, std::string const& tinfo) {
    ignored_.emplace(PtrNameType{addr, name, tinfo});
  }

  std::unordered_set<PtrNameType> const& getCheck() const { return check_; }
  std::unordered_set<PtrNameType> const& getIgnored() const { return ignored_; }
  std::unordered_set<PtrName> const& getIsSerial() const { return is_serialized_; }
  std::string const& getName() const { return name_; }

private:
  std::string name_ = "";
  std::unordered_set<PtrNameType> check_;
  std::unordered_set<PtrNameType> ignored_;
  std::unordered_set<PtrName> is_serialized_;
};

}} /* end namespace checkpoint::sanitizer */

#endif /*INCLUDED_SANITIZER_RUNTIME_STACK_RECORD_H*/

/*
//@HEADER
// *****************************************************************************
//
//                                missing_info.h
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

#if !defined INCLUDED_SANITIZER_RUNTIME_MISSING_INFO_H
#define INCLUDED_SANITIZER_RUNTIME_MISSING_INFO_H

#include <vector>
#include <string>

namespace checkpoint { namespace sanitizer {

struct CallStack {
  using StackType = std::vector<std::string>;

  explicit CallStack(StackType const& in_stack)
    : stack_(in_stack),
      instances_(1)
  { }

  bool isSame(StackType const& in) const {
    if (stack_.size() != in.size()) {
      return false;
    }
    for (std::size_t i = 0; i < stack_.size(); i++) {
      if (in.at(i) != stack_.at(i)) {
        return false;
      }
    }
    return true;
  }

  void addInstance() { instances_++; }

  int getInstances() const { return instances_; }

  StackType const& getStack() const { return stack_; }

private:
  StackType stack_;
  int instances_ = 0;
};

struct MissingInfo {
  using StackType = std::vector<std::string>;

  MissingInfo(
    std::string const& in_name, std::string const& in_tinfo,
    StackType const& in_stack
  ) : name_(in_name),
      tinfo_(in_tinfo)
  {
    addStack(in_stack);
  }

  void addStack(StackType const& in_stack) {
    instances_++;

    for (auto&& s : stacks_) {
      if (s.isSame(in_stack)) {
        s.addInstance();
        return;
      }
    }
    stacks_.emplace_back(CallStack{in_stack});
  }

  int getInstances() const { return instances_; }
  std::string const& getName() const { return name_; }
  std::string const& getTinfo() const { return tinfo_; }
  std::vector<CallStack> const& getStacks() const { return stacks_; }

private:
  std::string name_ = "";
  std::string tinfo_ = "";
  std::vector<CallStack> stacks_;
  int instances_ = 0;
};

}} /* end namespace checkpoint::sanitizer */

#endif /*INCLUDED_SANITIZER_RUNTIME_MISSING_INFO_H*/

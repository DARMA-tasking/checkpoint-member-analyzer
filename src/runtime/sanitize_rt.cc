/*
//@HEADER
// *****************************************************************************
//
//                                sanitize_rt.cc
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

#include "sanitize_rt.h"

#include <cassert>

namespace checkpoint { namespace sanitizer {

Runtime* rt() {
  static std::unique_ptr<Sanitizer> san_rt;
  if (san_rt == nullptr) {
    san_rt = std::make_unique<Sanitizer>();
  }
  return san_rt.get();
}

bool enabled() {
  fmt::print("returning enabled true!\n");
  return true;
}

void Sanitizer::checkMember(void* addr, std::string name, std::string tinfo) {
  assert(stack_.size() > 0 && "Must have valid live stack");
  fmt::print(
    "check: {}, name={}, tinfo={}: size={}\n",
    static_cast<void const*>(addr), name, tinfo, stack_.size()
  );
  stack_.back().checkElm(addr, name, tinfo);
}

void Sanitizer::skipMember(void* addr, std::string name, std::string tinfo) {
  assert(stack_.size() > 0 && "Must have valid live stack");
  fmt::print(
    "skip: {}, name={}, tinfo={}: size={}\n",
    static_cast<void const*>(addr), name, tinfo, stack_.size()
  );
  stack_.back().ignoreElm(addr, name, tinfo);
}

void Sanitizer::isSerialized(void* addr, std::size_t num, std::string tinfo) {
  // At a top-level, nothing to do!
  if (stack_.size() == 0) {
    return;
  }
  fmt::print(
    "isSerialized: {}, num={}. tinfo={}: size={}\n",
    static_cast<void const*>(addr), num, tinfo, stack_.size()
  );
  stack_.back().isSerialized(addr, tinfo);
}

void Sanitizer::push(std::string tinfo) {
  fmt::print("push: tinfo={}\n", tinfo);
  stack_.push_back(StackRecord{tinfo});
}

void Sanitizer::pop(std::string tinfo) {
  fmt::print("pop: tinfo={}\n", tinfo);

  // before we pop check the validity of this stack frame.
  checkValidityFrame();

  stack_.pop_back();
}

void Sanitizer::checkValidityFrame() {
  fmt::print("checkValidityFrame: size={}\n", stack_.size());

  assert(stack_.size() > 0 && "Must have a valid live stack");
  auto& e = stack_.back();

  auto const& checked = e.getCheck();
  auto const& ignored = e.getIgnored();
  auto const& is_serialized = e.getIsSerial();

  for (auto&& elm : checked) {
    // Skip check for elements that are explicitly skipped/ignored by the user
    auto ignore_iter = ignored.find(elm);
    if (ignore_iter != ignored.end()) {
      continue;
    }

    auto ser_iter = is_serialized.find(elm);
    if (ser_iter == is_serialized.end()) {
      // we are missing a element in the serializer
      std::vector<std::string> stack;
      for (auto i = stack_.rbegin(); i != stack_.rend(); i++) {
        stack.push_back(i->getName());
      }

      auto missing_iter = missing_.find(elm.name);
      if (missing_iter == missing_.end()) {
        missing_.emplace(
          std::piecewise_construct,
          std::forward_as_tuple(elm.name),
          std::forward_as_tuple(
            std::make_unique<MissingInfo>(elm.name, elm.tinfo, stack)
          )
        );
      } else {
        missing_iter->second->addStack(stack);
      }
    }
  }
}

void Sanitizer::printSummary() {
  std::vector<MissingInfo*> m;
  for (auto&& e : missing_) {
    m.push_back(e.second.get());
  }
  std::sort(m.begin(), m.end(), [](MissingInfo* m1, MissingInfo* m2) -> bool {
    return m1->getInstances() > m2->getInstances();
  });
  for (auto&& e : m) {
    auto const& name = e->getName();
    auto const& tinfo = e->getTinfo();
    auto const& stacks = e->getStacks();
    auto const& insts = e->getInstances();

    fmt::print("---- Missing Serialized Member ----\n");
    fmt::print("-----------------------------------\n");
    fmt::print("---- {} -- {} instances ----\n", name, insts);
    fmt::print("---- type: {} ---- \n", tinfo);
    for (std::size_t i = 0; i < stacks.size(); i++) {
      auto const& cs = stacks.at(i);
      auto const& stack = cs.getStack();
      auto const& sinsts = cs.getInstances();
      fmt::print("---- stack {}, {} instances \n", i, sinsts);
      for (std::size_t j = 0; j < stack.size(); j++) {
        fmt::print("\t {}\n", stack.at(j));
      }
    };
    fmt::print("-----------------------------------\n");
  }
}

}} /* end namespace checkpoint::sanitizer */

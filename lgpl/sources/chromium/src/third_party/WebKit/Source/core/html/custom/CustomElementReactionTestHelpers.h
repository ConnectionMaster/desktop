// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementReactionTestHelpers_h
#define CustomElementReactionTestHelpers_h

#include "core/html/custom/CustomElementReaction.h"

#include <initializer_list>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "core/html/custom/CustomElementReactionQueue.h"
#include "core/html/custom/CustomElementReactionStack.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Functional.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class Element;

class Command : public GarbageCollectedFinalized<Command> {
 public:
  Command() = default;
  virtual ~Command() = default;
  virtual void Trace(blink::Visitor* visitor) {}
  virtual void Run(Element*) = 0;

  DISALLOW_COPY_AND_ASSIGN(Command);
};

class Call : public Command {
 public:
  using Callback = WTF::Function<void(Element*)>;
  Call(Callback callback) : callback_(std::move(callback)) {}
  ~Call() override = default;
  void Run(Element* element) override { std::move(callback_).Run(element); }

 private:
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(Call);
};

class Unreached : public Command {
 public:
  Unreached(const char* message) : message_(message) {}
  ~Unreached() override = default;
  void Run(Element*) override { EXPECT_TRUE(false) << message_; }

 private:
  const char* message_;

  DISALLOW_COPY_AND_ASSIGN(Unreached);
};

class Log : public Command {
 public:
  Log(char what, std::vector<char>& where) : what_(what), where_(where) {}
  ~Log() override = default;
  void Run(Element*) override { where_.push_back(what_); }

 private:
  char what_;
  std::vector<char>& where_;

  DISALLOW_COPY_AND_ASSIGN(Log);
};

class Recurse : public Command {
 public:
  Recurse(CustomElementReactionQueue* queue) : queue_(queue) {}
  ~Recurse() override = default;
  virtual void Trace(blink::Visitor* visitor) {
    Command::Trace(visitor);
    visitor->Trace(queue_);
  }
  void Run(Element* element) override { queue_->InvokeReactions(element); }

 private:
  Member<CustomElementReactionQueue> queue_;

  DISALLOW_COPY_AND_ASSIGN(Recurse);
};

class Enqueue : public Command {
 public:
  Enqueue(CustomElementReactionQueue* queue, CustomElementReaction* reaction)
      : queue_(queue), reaction_(reaction) {}
  ~Enqueue() override = default;
  virtual void Trace(blink::Visitor* visitor) {
    Command::Trace(visitor);
    visitor->Trace(queue_);
    visitor->Trace(reaction_);
  }
  void Run(Element*) override { queue_->Add(reaction_); }

 private:
  Member<CustomElementReactionQueue> queue_;
  Member<CustomElementReaction> reaction_;

  DISALLOW_COPY_AND_ASSIGN(Enqueue);
};

class TestReaction : public CustomElementReaction {
 public:
  TestReaction(std::initializer_list<Command*> commands)
      : CustomElementReaction(nullptr) {
    // TODO(dominicc): Simply pass the initializer list when
    // HeapVector supports initializer lists like Vector.
    for (auto& command : commands)
      commands_.push_back(command);
  }
  ~TestReaction() override = default;
  virtual void Trace(blink::Visitor* visitor) {
    CustomElementReaction::Trace(visitor);
    visitor->Trace(commands_);
  }
  void Invoke(Element* element) override {
    for (auto& command : commands_)
      command->Run(element);
  }

 private:
  HeapVector<Member<Command>> commands_;

  DISALLOW_COPY_AND_ASSIGN(TestReaction);
};

class ResetCustomElementReactionStackForTest final {
  STACK_ALLOCATED();
 public:
  ResetCustomElementReactionStackForTest()
      : stack_(new CustomElementReactionStack),
        old_stack_(
            CustomElementReactionStackTestSupport::SetCurrentForTest(stack_)) {}

  ~ResetCustomElementReactionStackForTest() {
    CustomElementReactionStackTestSupport::SetCurrentForTest(old_stack_);
  }

  CustomElementReactionStack& Stack() { return *stack_; }

 private:
  Member<CustomElementReactionStack> stack_;
  Member<CustomElementReactionStack> old_stack_;

  DISALLOW_COPY_AND_ASSIGN(ResetCustomElementReactionStackForTest);
};

}  // namespace blink

#endif  // CustomElementReactionTestHelpers_h

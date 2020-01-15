/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/selection_modifier.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/inline_box_position.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_utils.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"

namespace blink {

namespace {

// Abstracts similarities between RootInlineBox and NGPhysicalLineBoxFragment
class AbstractLineBox {
  STACK_ALLOCATED();

 public:
  AbstractLineBox() = default;

  static AbstractLineBox CreateFor(const VisiblePosition&);

  bool IsNull() const { return type_ == Type::kNull; }

  bool CanBeCaretContainer() const {
    DCHECK(!IsNull());
    // We want to skip zero height boxes.
    // This could happen in case it is a TrailingFloatsRootInlineBox.
    if (IsOldLayout()) {
      return GetRootInlineBox().LogicalHeight() &&
             GetRootInlineBox().FirstLeafChild();
    }
    if (GetLineBoxFragment().IsEmptyLineBox())
      return false;
    const PhysicalSize physical_size = GetLineBoxFragment().Size();
    const LogicalSize logical_size = physical_size.ConvertToLogical(
        GetLineBoxFragment().Style().GetWritingMode());
    if (!logical_size.block_size)
      return false;
    // Use |ClosestLeafChildForPoint| to check if there's any leaf child.
    return GetLineBoxFragment().ClosestLeafChildForPoint(PhysicalOffset(),
                                                         false);
  }

  AbstractLineBox PreviousLine() const {
    DCHECK(!IsNull());
    if (IsOldLayout()) {
      const RootInlineBox* previous_root = GetRootInlineBox().PrevRootBox();
      return previous_root ? AbstractLineBox(*previous_root)
                           : AbstractLineBox();
    }
    const auto children = ng_box_fragment_->Children();
    for (wtf_size_t i = ng_child_index_; i;) {
      --i;
      if (!children[i]->IsLineBox())
        continue;
      return AbstractLineBox(*ng_box_fragment_, i);
    }
    return AbstractLineBox();
  }

  AbstractLineBox NextLine() const {
    DCHECK(!IsNull());
    if (IsOldLayout()) {
      const RootInlineBox* next_root = GetRootInlineBox().NextRootBox();
      return next_root ? AbstractLineBox(*next_root) : AbstractLineBox();
    }
    const auto children = ng_box_fragment_->Children();
    for (wtf_size_t i = ng_child_index_ + 1; i < children.size(); ++i) {
      if (!children[i]->IsLineBox())
        continue;
      return AbstractLineBox(*ng_box_fragment_, i);
    }
    return AbstractLineBox();
  }

  PhysicalOffset AbsoluteLineDirectionPointToLocalPointInBlock(
      LayoutUnit line_direction_point) {
    DCHECK(!IsNull());
    const LayoutBlockFlow& containing_block = GetBlock();
    // TODO(yosin): Is kIgnoreTransforms correct here?
    PhysicalOffset absolute_block_point = containing_block.LocalToAbsolutePoint(
        PhysicalOffset(), kIgnoreTransforms);
    if (containing_block.HasOverflowClip()) {
      absolute_block_point -=
          PhysicalOffset(containing_block.ScrolledContentOffset());
    }

    if (containing_block.IsHorizontalWritingMode()) {
      return PhysicalOffset(line_direction_point - absolute_block_point.left,
                            PhysicalBlockOffset());
    }
    return PhysicalOffset(PhysicalBlockOffset(),
                          line_direction_point - absolute_block_point.top);
  }

  const LayoutObject* ClosestLeafChildForPoint(
      const PhysicalOffset& point,
      bool only_editable_leaves) const {
    DCHECK(!IsNull());
    if (IsOldLayout()) {
      return GetRootInlineBox().ClosestLeafChildForPoint(
          GetBlock().FlipForWritingMode(point), only_editable_leaves);
    }
    const PhysicalOffset local_physical_point =
        point - ng_box_fragment_->Children()[ng_child_index_].offset;
    return GetLineBoxFragment().ClosestLeafChildForPoint(local_physical_point,
                                                         only_editable_leaves);
  }

 private:
  explicit AbstractLineBox(const RootInlineBox& root_inline_box)
      : root_inline_box_(&root_inline_box), type_(Type::kOldLayout) {}

  AbstractLineBox(const NGPhysicalBoxFragment& box_fragment,
                  wtf_size_t child_index)
      : ng_box_fragment_(&box_fragment),
        ng_child_index_(child_index),
        type_(Type::kLayoutNG) {
    DCHECK_LT(child_index, box_fragment.Children().size());
    DCHECK(box_fragment.Children()[child_index]->IsLineBox());
  }

  const LayoutBlockFlow& GetBlock() const {
    DCHECK(!IsNull());
    if (IsOldLayout()) {
      return *To<LayoutBlockFlow>(
          LineLayoutAPIShim::LayoutObjectFrom(GetRootInlineBox().Block()));
    }
    DCHECK(ng_box_fragment_->GetLayoutObject());
    return *To<LayoutBlockFlow>(ng_box_fragment_->GetLayoutObject());
  }

  LayoutUnit PhysicalBlockOffset() const {
    DCHECK(!IsNull());
    if (IsOldLayout()) {
      return GetBlock().FlipForWritingMode(
          GetRootInlineBox().BlockDirectionPointInLine());
    }
    const PhysicalOffset physical_offset =
        ng_box_fragment_->Children()[ng_child_index_].offset;
    return ng_box_fragment_->Style().IsHorizontalWritingMode()
               ? physical_offset.top
               : physical_offset.left;
  }

  bool IsOldLayout() const { return type_ == Type::kOldLayout; }

  bool IsLayoutNG() const { return type_ == Type::kLayoutNG; }

  const RootInlineBox& GetRootInlineBox() const {
    DCHECK(IsOldLayout());
    return *root_inline_box_;
  }

  const NGPhysicalLineBoxFragment& GetLineBoxFragment() const {
    DCHECK(IsLayoutNG());
    return To<NGPhysicalLineBoxFragment>(
        *ng_box_fragment_->Children()[ng_child_index_]);
  }

  enum class Type { kNull, kOldLayout, kLayoutNG };

  const RootInlineBox* root_inline_box_ = nullptr;
  const NGPhysicalBoxFragment* ng_box_fragment_ = nullptr;
  wtf_size_t ng_child_index_ = 0u;
  Type type_ = Type::kNull;
};

// static
AbstractLineBox AbstractLineBox::CreateFor(const VisiblePosition& position) {
  if (position.IsNull() ||
      !position.DeepEquivalent().AnchorNode()->GetLayoutObject()) {
    return AbstractLineBox();
  }

  const PositionWithAffinity adjusted = ComputeInlineAdjustedPosition(position);
  if (adjusted.IsNull())
    return AbstractLineBox();

  if (const NGPaintFragment* line_paint_fragment =
          NGContainingLineBoxOf(adjusted)) {
    const NGPhysicalBoxFragment& box_fragment = To<NGPhysicalBoxFragment>(
        line_paint_fragment->Parent()->PhysicalFragment());
    const NGPhysicalFragment& line_box_fragment =
        line_paint_fragment->PhysicalFragment();
    for (wtf_size_t i = 0; i < box_fragment.Children().size(); ++i) {
      if (box_fragment.Children()[i].get() == &line_box_fragment)
        return AbstractLineBox(box_fragment, i);
    }
    NOTREACHED();
    return AbstractLineBox();
  }

  const InlineBox* box =
      ComputeInlineBoxPositionForInlineAdjustedPosition(adjusted).inline_box;
  if (!box)
    return AbstractLineBox();
  return AbstractLineBox(box->Root());
}

ContainerNode* HighestEditableRootOfNode(const Node& node) {
  return HighestEditableRoot(FirstPositionInOrBeforeNode(node));
}

Node* PreviousNodeConsideringAtomicNodes(const Node& start) {
  if (start.previousSibling()) {
    Node* node = start.previousSibling();
    while (!IsAtomicNode(node) && node->lastChild())
      node = node->lastChild();
    return node;
  }
  return start.parentNode();
}

Node* NextNodeConsideringAtomicNodes(const Node& start) {
  if (!IsAtomicNode(&start) && start.hasChildren())
    return start.firstChild();
  if (start.nextSibling())
    return start.nextSibling();
  const Node* node = &start;
  while (node && !node->nextSibling())
    node = node->parentNode();
  if (node)
    return node->nextSibling();
  return nullptr;
}

// Returns the previous leaf node or nullptr if there are no more. Delivers leaf
// nodes as if the whole DOM tree were a linear chain of its leaf nodes.
Node* PreviousAtomicLeafNode(const Node& start) {
  Node* node = PreviousNodeConsideringAtomicNodes(start);
  while (node) {
    if (IsAtomicNode(node))
      return node;
    node = PreviousNodeConsideringAtomicNodes(*node);
  }
  return nullptr;
}

// Returns the next leaf node or nullptr if there are no more. Delivers leaf
// nodes as if the whole DOM tree were a linear chain of its leaf nodes.
Node* NextAtomicLeafNode(const Node& start) {
  Node* node = NextNodeConsideringAtomicNodes(start);
  while (node) {
    if (IsAtomicNode(node))
      return node;
    node = NextNodeConsideringAtomicNodes(*node);
  }
  return nullptr;
}

Node* PreviousLeafWithSameEditability(const Node& node) {
  const bool editable = HasEditableStyle(node);
  for (Node* runner = PreviousAtomicLeafNode(node); runner;
       runner = PreviousAtomicLeafNode(*runner)) {
    if (editable == HasEditableStyle(*runner))
      return runner;
  }
  return nullptr;
}

Node* NextLeafWithGivenEditability(Node* node, bool editable) {
  if (!node)
    return nullptr;

  for (Node* runner = NextAtomicLeafNode(*node); runner;
       runner = NextAtomicLeafNode(*runner)) {
    if (editable == HasEditableStyle(*runner))
      return runner;
  }
  return nullptr;
}

bool InSameLine(const Node& node, const VisiblePosition& visible_position) {
  if (!node.GetLayoutObject())
    return true;
  return InSameLine(CreateVisiblePosition(FirstPositionInOrBeforeNode(node)),
                    visible_position);
}

Node* FindNodeInPreviousLine(const Node& start_node,
                             const VisiblePosition& visible_position) {
  for (Node* runner = PreviousLeafWithSameEditability(start_node); runner;
       runner = PreviousLeafWithSameEditability(*runner)) {
    if (!InSameLine(*runner, visible_position))
      return runner;
  }
  return nullptr;
}

// FIXME: consolidate with code in previousLinePosition.
Position PreviousRootInlineBoxCandidatePosition(
    Node* node,
    const VisiblePosition& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  ContainerNode* highest_root =
      HighestEditableRoot(visible_position.DeepEquivalent());
  Node* const previous_node = FindNodeInPreviousLine(*node, visible_position);
  for (Node* runner = previous_node; runner && !runner->IsShadowRoot();
       runner = PreviousLeafWithSameEditability(*runner)) {
    if (HighestEditableRootOfNode(*runner) != highest_root)
      break;

    const Position& candidate =
        IsA<HTMLBRElement>(*runner)
            ? Position::BeforeNode(*runner)
            : Position::EditingPositionOf(runner, CaretMaxOffset(runner));
    if (IsVisuallyEquivalentCandidate(candidate))
      return candidate;
  }
  return Position();
}

Position NextRootInlineBoxCandidatePosition(
    Node* node,
    const VisiblePosition& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  ContainerNode* highest_root =
      HighestEditableRoot(visible_position.DeepEquivalent());
  // TODO(xiaochengh): We probably also need to pass in the starting editability
  // to |PreviousLeafWithSameEditability|.
  const bool is_editable = HasEditableStyle(
      *visible_position.DeepEquivalent().ComputeContainerNode());
  Node* next_node = NextLeafWithGivenEditability(node, is_editable);
  while (next_node && InSameLine(*next_node, visible_position)) {
    next_node = NextLeafWithGivenEditability(next_node, is_editable);
  }

  for (Node* runner = next_node; runner && !runner->IsShadowRoot();
       runner = NextLeafWithGivenEditability(runner, is_editable)) {
    if (HighestEditableRootOfNode(*runner) != highest_root)
      break;

    const Position& candidate =
        Position::EditingPositionOf(runner, CaretMinOffset(runner));
    if (IsVisuallyEquivalentCandidate(candidate))
      return candidate;
  }
  return Position();
}

}  // namespace

// static
VisiblePosition SelectionModifier::PreviousLinePosition(
    const VisiblePosition& visible_position,
    LayoutUnit line_direction_point) {
  DCHECK(visible_position.IsValid()) << visible_position;

  // TODO(xiaochengh): Make all variables |const|.

  Position p = visible_position.DeepEquivalent();
  Node* node = p.AnchorNode();

  if (!node)
    return VisiblePosition();

  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return VisiblePosition();

  AbstractLineBox line = AbstractLineBox::CreateFor(visible_position);
  if (!line.IsNull()) {
    line = line.PreviousLine();
    if (line.IsNull() || !line.CanBeCaretContainer())
      line = AbstractLineBox();
  }

  if (line.IsNull()) {
    Position position =
        PreviousRootInlineBoxCandidatePosition(node, visible_position);
    if (position.IsNotNull()) {
      const VisiblePosition candidate = CreateVisiblePosition(position);
      line = AbstractLineBox::CreateFor(candidate);
      if (line.IsNull()) {
        // TODO(editing-dev): Investigate if this is correct for null
        // |candidate|.
        return candidate;
      }
    }
  }

  if (!line.IsNull()) {
    // FIXME: Can be wrong for multi-column layout and with transforms.
    PhysicalOffset point_in_line =
        line.AbsoluteLineDirectionPointToLocalPointInBlock(
            line_direction_point);
    const LayoutObject* closest_leaf_child =
        line.ClosestLeafChildForPoint(point_in_line, IsEditablePosition(p));
    if (closest_leaf_child) {
      const Node* node = closest_leaf_child->GetNode();
      if (node && EditingIgnoresContent(*node))
        return VisiblePosition::InParentBeforeNode(*node);
      return CreateVisiblePosition(
          closest_leaf_child->PositionForPoint(point_in_line));
    }
  }

  // Could not find a previous line. This means we must already be on the first
  // line. Move to the start of the content in this block, which effectively
  // moves us to the start of the line we're on.
  Element* root_element = HasEditableStyle(*node)
                              ? RootEditableElement(*node)
                              : node->GetDocument().documentElement();
  if (!root_element)
    return VisiblePosition();
  return VisiblePosition::FirstPositionInNode(*root_element);
}

// static
VisiblePosition SelectionModifier::NextLinePosition(
    const VisiblePosition& visible_position,
    LayoutUnit line_direction_point) {
  DCHECK(visible_position.IsValid()) << visible_position;

  // TODO(xiaochengh): Make all variables |const|.

  Position p = visible_position.DeepEquivalent();
  Node* node = p.AnchorNode();

  if (!node)
    return VisiblePosition();

  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return VisiblePosition();

  AbstractLineBox line = AbstractLineBox::CreateFor(visible_position);
  if (!line.IsNull()) {
    line = line.NextLine();
    if (line.IsNull() || !line.CanBeCaretContainer())
      line = AbstractLineBox();
  }

  if (line.IsNull()) {
    // FIXME: We need do the same in previousLinePosition.
    Node* child = NodeTraversal::ChildAt(*node, p.ComputeEditingOffset());
    Node* search_start_node =
        child ? child : &NodeTraversal::LastWithinOrSelf(*node);
    Position position =
        NextRootInlineBoxCandidatePosition(search_start_node, visible_position);
    if (position.IsNotNull()) {
      const VisiblePosition candidate = CreateVisiblePosition(position);
      line = AbstractLineBox::CreateFor(candidate);
      if (line.IsNull()) {
        // TODO(editing-dev): Investigate if this is correct for null
        // |candidate|.
        return candidate;
      }
    }
  }

  if (!line.IsNull()) {
    // FIXME: Can be wrong for multi-column layout and with transforms.
    PhysicalOffset point_in_line =
        line.AbsoluteLineDirectionPointToLocalPointInBlock(
            line_direction_point);
    const LayoutObject* closest_leaf_child =
        line.ClosestLeafChildForPoint(point_in_line, IsEditablePosition(p));
    if (closest_leaf_child) {
      const Node* node = closest_leaf_child->GetNode();
      if (node && EditingIgnoresContent(*node))
        return VisiblePosition::InParentBeforeNode(*node);
      return CreateVisiblePosition(
          closest_leaf_child->PositionForPoint(point_in_line));
    }
  }

  // Could not find a next line. This means we must already be on the last line.
  // Move to the end of the content in this block, which effectively moves us
  // to the end of the line we're on.
  Element* root_element = HasEditableStyle(*node)
                              ? RootEditableElement(*node)
                              : node->GetDocument().documentElement();
  if (!root_element)
    return VisiblePosition();
  return VisiblePosition::LastPositionInNode(*root_element);
}

}  // namespace blink

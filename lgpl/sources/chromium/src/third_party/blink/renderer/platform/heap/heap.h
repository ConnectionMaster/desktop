/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_H_

#include <limits>
#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/heap/finalizer_traits.h"
#include "third_party/blink/renderer/platform/heap/gc_info.h"
#include "third_party/blink/renderer/platform/heap/heap_page.h"
#include "third_party/blink/renderer/platform/heap/process_heap.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/thread_state_statistics.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/heap/worklist.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"

namespace blink {

namespace incremental_marking_test {
class IncrementalMarkingScopeBase;
}  // namespace incremental_marking_test

class AddressCache;
class ConcurrentMarkingVisitor;
class ThreadHeapStatsCollector;
class PagePool;
class ProcessHeapReporter;
class RegionTree;

struct MarkingItem {
  void* object;
  TraceCallback callback;
};

using CustomCallbackItem = MarkingItem;
using NotFullyConstructedItem = void*;
using WeakTableItem = MarkingItem;

struct BackingStoreCallbackItem {
  void* backing;
  MovingObjectCallback callback;
};

using V8Reference = const TraceWrapperV8Reference<v8::Value>*;

// Segment size of 512 entries necessary to avoid throughput regressions. Since
// the work list is currently a temporary object this is not a problem.
using MarkingWorklist = Worklist<MarkingItem, 512 /* local entries */>;
using NotFullyConstructedWorklist =
    Worklist<NotFullyConstructedItem, 16 /* local entries */>;
using WeakCallbackWorklist =
    Worklist<CustomCallbackItem, 256 /* local entries */>;
// Using large local segments here (sized 512 entries) to avoid throughput
// regressions.
using MovableReferenceWorklist =
    Worklist<MovableReference*, 512 /* local entries */>;
using WeakTableWorklist = Worklist<WeakTableItem, 16 /* local entries */>;
using BackingStoreCallbackWorklist =
    Worklist<BackingStoreCallbackItem, 16 /* local entries */>;
using V8ReferencesWorklist = Worklist<V8Reference, 16 /* local entries */>;

class PLATFORM_EXPORT HeapAllocHooks {
  STATIC_ONLY(HeapAllocHooks);

 public:
  // TODO(hajimehoshi): Pass a type name of the allocated object.
  typedef void AllocationHook(Address, size_t, const char*);
  typedef void FreeHook(Address);

  // Sets allocation hook. Only one hook is supported.
  static void SetAllocationHook(AllocationHook* hook) {
    CHECK(!allocation_hook_ || !hook);
    allocation_hook_ = hook;
  }

  // Sets free hook. Only one hook is supported.
  static void SetFreeHook(FreeHook* hook) {
    CHECK(!free_hook_ || !hook);
    free_hook_ = hook;
  }

  static void AllocationHookIfEnabled(Address address,
                                      size_t size,
                                      const char* type_name) {
    AllocationHook* allocation_hook = allocation_hook_;
    if (UNLIKELY(!!allocation_hook))
      allocation_hook(address, size, type_name);
  }

  static void FreeHookIfEnabled(Address address) {
    FreeHook* free_hook = free_hook_;
    if (UNLIKELY(!!free_hook))
      free_hook(address);
  }

 private:
  static AllocationHook* allocation_hook_;
  static FreeHook* free_hook_;
};

class HeapCompact;
template <typename T>
class Member;
template <typename T>
class WeakMember;
template <typename T>
class UntracedMember;

namespace internal {

template <typename T, bool = NeedsAdjustPointer<T>::value>
class ObjectAliveTrait;

template <typename T>
class ObjectAliveTrait<T, false> {
  STATIC_ONLY(ObjectAliveTrait);

 public:
  static bool IsHeapObjectAlive(const T* object) {
    static_assert(sizeof(T), "T must be fully defined");
    return HeapObjectHeader::FromPayload(object)->IsMarked();
  }
};

template <typename T>
class ObjectAliveTrait<T, true> {
  STATIC_ONLY(ObjectAliveTrait);

 public:
  NO_SANITIZE_ADDRESS
  static bool IsHeapObjectAlive(const T* object) {
    static_assert(sizeof(T), "T must be fully defined");
    const HeapObjectHeader* header = object->GetHeapObjectHeader();
    if (header == BlinkGC::kNotFullyConstructedObject) {
      // Objects under construction are always alive.
      return true;
    }
    return header->IsMarked();
  }
};

template <typename T, typename = int>
struct IsGarbageCollectedContainer : std::false_type {};

template <typename T>
struct IsGarbageCollectedContainer<
    T,
    typename T::IsGarbageCollectedCollectionTypeMarker> : std::true_type {};

}  // namespace internal

class PLATFORM_EXPORT ThreadHeap {
  USING_FAST_MALLOC(ThreadHeap);

 public:
  explicit ThreadHeap(ThreadState*);
  ~ThreadHeap();

  template <typename T>
  static inline bool IsHeapObjectAlive(const T* object) {
    static_assert(sizeof(T), "T must be fully defined");
    // The strongification of collections relies on the fact that once a
    // collection has been strongified, there is no way that it can contain
    // non-live entries, so no entries will be removed. Since you can't set
    // the mark bit on a null pointer, that means that null pointers are
    // always 'alive'.
    if (!object)
      return true;
    // TODO(keishi): some tests create CrossThreadPersistent on non attached
    // threads.
    if (!ThreadState::Current())
      return true;
    DCHECK(&ThreadState::Current()->Heap() ==
           &PageFromObject(object)->Arena()->GetThreadState()->Heap());
    return internal::ObjectAliveTrait<T>::IsHeapObjectAlive(object);
  }
  template <typename T>
  static inline bool IsHeapObjectAlive(const Member<T>& member) {
    return IsHeapObjectAlive(member.Get());
  }
  template <typename T>
  static inline bool IsHeapObjectAlive(const WeakMember<T>& member) {
    return IsHeapObjectAlive(member.Get());
  }
  template <typename T>
  static inline bool IsHeapObjectAlive(const UntracedMember<T>& member) {
    return IsHeapObjectAlive(member.Get());
  }

  MarkingWorklist* GetMarkingWorklist() const {
    return marking_worklist_.get();
  }

  NotFullyConstructedWorklist* GetNotFullyConstructedWorklist() const {
    return not_fully_constructed_worklist_.get();
  }

  WeakCallbackWorklist* GetWeakCallbackWorklist() const {
    return weak_callback_worklist_.get();
  }

  MovableReferenceWorklist* GetMovableReferenceWorklist() const {
    return movable_reference_worklist_.get();
  }

  WeakTableWorklist* GetWeakTableWorklist() const {
    return weak_table_worklist_.get();
  }

  BackingStoreCallbackWorklist* GetBackingStoreCallbackWorklist() const {
    return backing_store_callback_worklist_.get();
  }

  V8ReferencesWorklist* GetV8ReferencesWorklist() const {
    return v8_references_worklist_.get();
  }

  // Register an ephemeron table for fixed-point iteration.
  void RegisterWeakTable(void* container_object,
                         EphemeronCallback);

  // Heap compaction registration methods:

  // Checks whether we need to register |addr| as a backing store or a slot
  // containing reference to it.
  bool ShouldRegisterMovingAddress(Address addr);

  RegionTree* GetRegionTree() { return region_tree_.get(); }

  static inline size_t AllocationSizeFromSize(size_t size) {
    // Add space for header.
    size_t allocation_size = size + sizeof(HeapObjectHeader);
    // The allocation size calculation can overflow for large sizes.
    CHECK_GT(allocation_size, size);
    // Align size with allocation granularity.
    allocation_size = (allocation_size + kAllocationMask) & ~kAllocationMask;
    return allocation_size;
  }
  Address AllocateOnArenaIndex(ThreadState*,
                               size_t,
                               int arena_index,
                               uint32_t gc_info_index,
                               const char* type_name);
  template <typename T>
  static Address Allocate(size_t);

  void WeakProcessing(MarkingVisitor*);

  // Moves not fully constructed objects to previously not fully constructed
  // objects. Such objects can be iterated using the Trace() method and do
  // not need to rely on conservative handling.
  void FlushNotFullyConstructedObjects();

  // Marks not fully constructed objects.
  void MarkNotFullyConstructedObjects(MarkingVisitor*);
  // Marks the transitive closure including ephemerons.
  bool AdvanceMarking(MarkingVisitor*, base::TimeTicks deadline);
  void VerifyMarking();

  // Returns true if marker is done
  bool AdvanceConcurrentMarking(ConcurrentMarkingVisitor*, base::TimeTicks);

  // Conservatively checks whether an address is a pointer in any of the
  // thread heaps.  If so marks the object pointed to as live.
  Address CheckAndMarkPointer(MarkingVisitor*, Address);

  size_t ObjectPayloadSizeForTesting();
  void ResetAllocationPointForTesting();

  AddressCache* address_cache() const { return address_cache_.get(); }

  PagePool* GetFreePagePool() { return free_page_pool_.get(); }

  // This look-up uses the region search tree and a negative contains cache to
  // provide an efficient mapping from arbitrary addresses to the containing
  // heap-page if one exists.
  BasePage* LookupPageForAddress(Address);

  HeapCompact* Compaction();

  // Get one of the heap structures for this thread.
  // The thread heap is split into multiple heap parts based on object types
  // and object sizes.
  BaseArena* Arena(int arena_index) const {
    DCHECK_LE(0, arena_index);
    DCHECK_LT(arena_index, BlinkGC::kNumberOfArenas);
    return arenas_[arena_index];
  }

  // VectorBackingArena() returns an arena that the vector allocation should
  // use.  We have four vector arenas and want to choose the best arena here.
  //
  // The goal is to improve the succession rate where expand and
  // promptlyFree happen at an allocation point. This is a key for reusing
  // the same memory as much as possible and thus improves performance.
  // To achieve the goal, we use the following heuristics:
  //
  // - A vector that has been expanded recently is likely to be expanded
  //   again soon.
  // - A vector is likely to be promptly freed if the same type of vector
  //   has been frequently promptly freed in the past.
  // - Given the above, when allocating a new vector, look at the four vectors
  //   that are placed immediately prior to the allocation point of each arena.
  //   Choose the arena where the vector is least likely to be expanded
  //   nor promptly freed.
  //
  // To implement the heuristics, we add an arenaAge to each arena. The arenaAge
  // is updated if:
  //
  // - a vector on the arena is expanded; or
  // - a vector that meets the condition (*) is allocated on the arena
  //
  //   (*) More than 33% of the same type of vectors have been promptly
  //       freed since the last GC.
  //
  BaseArena* VectorBackingArena(uint32_t gc_info_index) {
    DCHECK(thread_state_->CheckThread());
    uint32_t entry_index = gc_info_index & kLikelyToBePromptlyFreedArrayMask;
    --likely_to_be_promptly_freed_[entry_index];
    int arena_index = vector_backing_arena_index_;
    // If likely_to_be_promptly_freed_[entryIndex] > 0, that means that
    // more than 33% of vectors of the type have been promptly freed
    // since the last GC.
    if (likely_to_be_promptly_freed_[entry_index] > 0) {
      arena_ages_[arena_index] = ++current_arena_ages_;
      vector_backing_arena_index_ =
          ArenaIndexOfVectorArenaLeastRecentlyExpanded(
              BlinkGC::kVector1ArenaIndex, BlinkGC::kVector4ArenaIndex);
    }
    DCHECK(IsVectorArenaIndex(arena_index));
    return arenas_[arena_index];
  }
  BaseArena* ExpandedVectorBackingArena(uint32_t gc_info_index);
  static bool IsVectorArenaIndex(int arena_index) {
    return BlinkGC::kVector1ArenaIndex <= arena_index &&
           arena_index <= BlinkGC::kVector4ArenaIndex;
  }
  static bool IsNormalArenaIndex(int);
  void AllocationPointAdjusted(int arena_index);
  void PromptlyFreed(uint32_t gc_info_index);
  void ClearArenaAges();
  int ArenaIndexOfVectorArenaLeastRecentlyExpanded(int begin_arena_index,
                                                   int end_arena_index);

  void MakeConsistentForGC();
  // MakeConsistentForMutator() drops marks from marked objects and rebuild
  // free lists. This is called after taking a snapshot and before resuming
  // the executions of mutators.
  void MakeConsistentForMutator();

  void Compact();

  enum class SweepingType : uint8_t { kMutator, kConcurrent };
  bool AdvanceSweep(SweepingType sweeping_type, base::TimeTicks deadline);

  void PrepareForSweep();
  void RemoveAllPages();
  void InvokeFinalizersOnSweptPages();
  void CompleteSweep();

  void CollectStatistics(ThreadState::Statistics* statistics);

  ThreadHeapStatsCollector* stats_collector() const {
    return heap_stats_collector_.get();
  }

#if defined(ADDRESS_SANITIZER)
  void PoisonUnmarkedObjects();
#endif

#if DCHECK_IS_ON()
  // Infrastructure to determine if an address is within one of the
  // address ranges for the Blink heap. If the address is in the Blink
  // heap the containing heap page is returned.
  BasePage* FindPageFromAddress(Address);
  BasePage* FindPageFromAddress(const void* pointer) {
    return FindPageFromAddress(
        reinterpret_cast<Address>(const_cast<void*>(pointer)));
  }
#endif

 private:
  static int ArenaIndexForObjectSize(size_t);

  void SetupWorklists();
  void DestroyMarkingWorklists(BlinkGC::StackState);
  void DestroyCompactionWorklists();

  void InvokeEphemeronCallbacks(MarkingVisitor*);

  void FlushV8References();

  ThreadState* thread_state_;
  std::unique_ptr<ThreadHeapStatsCollector> heap_stats_collector_;
  std::unique_ptr<RegionTree> region_tree_;
  std::unique_ptr<AddressCache> address_cache_;
  std::unique_ptr<PagePool> free_page_pool_;
  std::unique_ptr<ProcessHeapReporter> process_heap_reporter_;

  // All objects on this worklist have been fully initialized and assigned a
  // trace callback for iterating the body of the object. This worklist should
  // contain almost all objects.
  std::unique_ptr<MarkingWorklist> marking_worklist_;

  // Objects on this worklist were observed to be in construction (in their
  // constructor) and thus have been delayed for processing. They have not yet
  // been assigned a valid header and trace callback.
  std::unique_ptr<NotFullyConstructedWorklist> not_fully_constructed_worklist_;

  // Objects on this worklist were previously in construction but have been
  // moved here upon observing a safepoint, i.e., processing without stack. They
  // have not yet been assigned a valid header and trace callback but are fully
  // specified and can thus be iterated using the trace callback (which can be
  // looked up dynamically).
  std::unique_ptr<NotFullyConstructedWorklist>
      previously_not_fully_constructed_worklist_;

  // Worklist of weak callbacks accumulated for objects. Such callbacks are
  // processed after finishing marking objects.
  std::unique_ptr<WeakCallbackWorklist> weak_callback_worklist_;

  // The worklist is to remember slots that are traced during
  // marking phases. The mapping between the slots and the backing stores are
  // created at the atomic pause phase.
  std::unique_ptr<MovableReferenceWorklist> movable_reference_worklist_;

  // Worklist of ephemeron callbacks. Used to pass new callbacks from
  // MarkingVisitor to ThreadHeap.
  std::unique_ptr<WeakTableWorklist> weak_table_worklist_;

  // This worklist is used to passing backing store callback to HeapCompact.
  std::unique_ptr<BackingStoreCallbackWorklist>
      backing_store_callback_worklist_;

  // Worklist for storing the V8 references until ThreadHeap can flush them
  // to V8.
  std::unique_ptr<V8ReferencesWorklist> v8_references_worklist_;

  // No duplicates allowed for ephemeron callbacks. Hence, we use a hashmap
  // with the key being the HashTable.
  WTF::HashMap<void*, EphemeronCallback> ephemeron_callbacks_;

  std::unique_ptr<HeapCompact> compaction_;

  BaseArena* arenas_[BlinkGC::kNumberOfArenas];
  int vector_backing_arena_index_;
  size_t arena_ages_[BlinkGC::kNumberOfArenas];
  size_t current_arena_ages_;

  // Ideally we want to allocate an array of size |gcInfoTableMax| but it will
  // waste memory. Thus we limit the array size to 2^8 and share one entry
  // with multiple types of vectors. This won't be an issue in practice,
  // since there will be less than 2^8 types of objects in common cases.
  static const int kLikelyToBePromptlyFreedArraySize = (1 << 8);
  static const int kLikelyToBePromptlyFreedArrayMask =
      kLikelyToBePromptlyFreedArraySize - 1;
  std::unique_ptr<int[]> likely_to_be_promptly_freed_;

  static ThreadHeap* main_thread_heap_;

  friend class incremental_marking_test::IncrementalMarkingScopeBase;
  template <typename T>
  friend class Member;
  friend class ThreadState;
};

template <typename T>
class GarbageCollected {
  IS_GARBAGE_COLLECTED_TYPE();

  // For now direct allocation of arrays on the heap is not allowed.
  void* operator new[](size_t size);

#if defined(OS_WIN) && defined(COMPILER_MSVC)
  // Due to some quirkiness in the MSVC compiler we have to provide
  // the delete[] operator in the GarbageCollected subclasses as it
  // is called when a class is exported in a DLL.
 protected:
  void operator delete[](void* p) { NOTREACHED(); }
#else
  void operator delete[](void* p);
#endif

 public:
  using GarbageCollectedType = T;

  void* operator new(size_t size) = delete;  // Must use MakeGarbageCollected.

  static void* AllocateObject(size_t size) {
    if (IsGarbageCollectedMixin<T>::value) {
      // Ban large mixin so we can use PageFromObject() on them.
      CHECK_GE(kLargeObjectSizeThreshold, size)
          << "GarbageCollectedMixin may not be a large object";
    }
    return ThreadHeap::Allocate<T>(size);
  }

  void operator delete(void* p) { NOTREACHED(); }

 protected:
  GarbageCollected() = default;

  DISALLOW_COPY_AND_ASSIGN(GarbageCollected);
};

// Default MakeGarbageCollected: Constructs an instance of T, which is a garbage
// collected type.
template <typename T, typename... Args>
T* MakeGarbageCollected(Args&&... args) {
  static_assert(WTF::IsGarbageCollectedType<T>::value,
                "T needs to be a garbage collected object");
  static_assert(std::is_trivially_destructible<T>::value ||
                    std::has_virtual_destructor<T>::value ||
                    std::is_final<T>::value ||
                    internal::IsGarbageCollectedContainer<T>::value ||
                    internal::HasFinalizeGarbageCollectedObject<T>::value,
                "Finalized GarbageCollected class should either have a virtual "
                "destructor or be marked as final.");
  void* memory = T::AllocateObject(sizeof(T));
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(memory);
  // Placement new as regular operator new() is deleted.
  T* object = ::new (memory) T(std::forward<Args>(args)...);
  header->MarkFullyConstructed<HeapObjectHeader::AccessMode::kAtomic>();
  return object;
}

// Used for passing custom sizes to MakeGarbageCollected.
struct AdditionalBytes {
  explicit AdditionalBytes(size_t bytes) : value(bytes) {}
  const size_t value;
};

// Constructs an instance of T, which is a garbage collected type. This special
// version takes size which enables constructing inline objects.
template <typename T, typename... Args>
T* MakeGarbageCollected(AdditionalBytes additional_bytes, Args&&... args) {
  static_assert(WTF::IsGarbageCollectedType<T>::value,
                "T needs to be a garbage collected object");
  static_assert(std::is_trivially_destructible<T>::value ||
                    std::has_virtual_destructor<T>::value ||
                    std::is_final<T>::value ||
                    internal::IsGarbageCollectedContainer<T>::value ||
                    internal::HasFinalizeGarbageCollectedObject<T>::value,
                "Finalized GarbageCollected class should either have a virtual "
                "destructor or be marked as final.");
  void* memory = T::AllocateObject(sizeof(T) + additional_bytes.value);
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(memory);
  // Placement new as regular operator new() is deleted.
  T* object = ::new (memory) T(std::forward<Args>(args)...);
  header->MarkFullyConstructed<HeapObjectHeader::AccessMode::kAtomic>();
  return object;
}

// Assigning class types to their arenas.
//
// We use sized arenas for most 'normal' objects to improve memory locality.
// It seems that the same type of objects are likely to be accessed together,
// which means that we want to group objects by type. That's one reason
// why we provide dedicated arenas for popular types (e.g., Node, CSSValue),
// but it's not practical to prepare dedicated arenas for all types.
// Thus we group objects by their sizes, hoping that this will approximately
// group objects by their types.
//

inline int ThreadHeap::ArenaIndexForObjectSize(size_t size) {
  if (size < 64) {
    if (size < 32)
      return BlinkGC::kNormalPage1ArenaIndex;
    return BlinkGC::kNormalPage2ArenaIndex;
  }
  if (size < 128)
    return BlinkGC::kNormalPage3ArenaIndex;
  return BlinkGC::kNormalPage4ArenaIndex;
}

inline bool ThreadHeap::IsNormalArenaIndex(int index) {
  return index >= BlinkGC::kNormalPage1ArenaIndex &&
         index <= BlinkGC::kNormalPage4ArenaIndex;
}

inline Address ThreadHeap::AllocateOnArenaIndex(ThreadState* state,
                                                size_t size,
                                                int arena_index,
                                                uint32_t gc_info_index,
                                                const char* type_name) {
  DCHECK(state->IsAllocationAllowed());
  DCHECK_NE(arena_index, BlinkGC::kLargeObjectArenaIndex);
  NormalPageArena* arena = static_cast<NormalPageArena*>(Arena(arena_index));
  Address address =
      arena->AllocateObject(AllocationSizeFromSize(size), gc_info_index);
  HeapAllocHooks::AllocationHookIfEnabled(address, size, type_name);
  return address;
}

template <typename T>
Address ThreadHeap::Allocate(size_t size) {
  ThreadState* state = ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState();
  const char* type_name = WTF_HEAP_PROFILER_TYPE_NAME(T);
  return state->Heap().AllocateOnArenaIndex(
      state, size, ThreadHeap::ArenaIndexForObjectSize(size),
      GCInfoTrait<T>::Index(), type_name);
}

template <typename T>
void Visitor::HandleWeakCell(Visitor* self, void* object) {
  WeakMember<T>* weak_member = reinterpret_cast<WeakMember<T>*>(object);
  if (weak_member->Get()) {
    if (weak_member->IsHashTableDeletedValue()) {
      // This can happen when weak fields are deleted while incremental marking
      // is running. Deleted values need to be preserved to avoid reviving
      // objects in containers.
      return;
    }
    if (!ThreadHeap::IsHeapObjectAlive(weak_member->Get()))
      weak_member->Clear();
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_H_

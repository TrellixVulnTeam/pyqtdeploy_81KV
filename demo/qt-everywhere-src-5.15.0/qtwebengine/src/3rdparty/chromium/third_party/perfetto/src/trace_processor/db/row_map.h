/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_DB_ROW_MAP_H_
#define SRC_TRACE_PROCESSOR_DB_ROW_MAP_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/optional.h"
#include "src/trace_processor/db/bit_vector.h"
#include "src/trace_processor/db/bit_vector_iterators.h"

namespace perfetto {
namespace trace_processor {

// Stores a list of row indicies in a space efficient manner. One or more
// columns can refer to the same RowMap. The RowMap defines the access pattern
// to iterate on rows.
//
// Implementation details:
//
// Behind the scenes, this class is impelemented using one of three backing
// data-structures:
// 1. A start and end index (internally named 'range')
// 1. BitVector
// 2. std::vector<uint32_t> (internally named IndexVector).
//
// Generally the preference for data structures is range > BitVector >
// std::vector<uint32>; this ordering is based mainly on memory efficiency as we
// expect RowMaps to be large.
//
// However, BitVector and std::vector<uint32_t> allow things which are not
// possible with the data-structures preferred to them:
//  * a range (as the name suggests) can only store a compact set of indices
//  with no holes. A BitVector works around this limitation by storing a 1 at an
//  index where that row is part of the RowMap and 0 otherwise.
//  * as soon as ordering or duplicate rows come into play, we cannot use a
//   BitVector anymore as ordering/duplicate row information cannot be captured
//   by a BitVector.
//
// For small, sparse RowMaps, it is possible that a std::vector<uint32_t> is
// more efficient than a BitVector; in this case, we will make a best effort
// switch to it but the cases where this happens is not precisely defined.
class RowMap {
 private:
  // We need to declare these iterator classes before RowMap::Iterator as it
  // depends on them. However, we don't want to make these public so keep them
  // under a special private section.

  // Iterator for ranged mode of RowMap.
  // This class should act as a drop-in replacement for
  // BitVector::SetBitsIterator.
  class RangeIterator {
   public:
    RangeIterator(const RowMap* rm) : rm_(rm), index_(rm->start_idx_) {}

    void Next() { ++index_; }

    operator bool() const { return index_ < rm_->end_idx_; }

    uint32_t index() const { return index_; }

    uint32_t ordinal() const { return index_ - rm_->start_idx_; }

   private:
    const RowMap* rm_ = nullptr;
    uint32_t index_ = 0;
  };

  // Iterator for index vector mode of RowMap.
  // This class should act as a drop-in replacement for
  // BitVector::SetBitsIterator.
  class IndexVectorIterator {
   public:
    IndexVectorIterator(const RowMap* rm) : rm_(rm) {}

    void Next() { ++ordinal_; }

    operator bool() const { return ordinal_ < rm_->index_vector_.size(); }

    uint32_t index() const { return rm_->index_vector_[ordinal_]; }

    uint32_t ordinal() const { return ordinal_; }

   private:
    const RowMap* rm_ = nullptr;
    uint32_t ordinal_ = 0;
  };

 public:
  // Allows efficient iteration over the rows of a RowMap.
  //
  // Note: you should usually prefer to use the methods on RowMap directly (if
  // they exist for the task being attempted) to avoid the lookup for the mode
  // of the RowMap on every method call.
  class Iterator {
   public:
    Iterator(const RowMap* rm) : rm_(rm) {
      switch (rm->mode_) {
        case Mode::kRange:
          range_it_.reset(new RangeIterator(rm));
          break;
        case Mode::kBitVector:
          set_bits_it_.reset(
              new BitVector::SetBitsIterator(rm->bit_vector_.IterateSetBits()));
          break;
        case Mode::kIndexVector:
          iv_it_.reset(new IndexVectorIterator(rm));
          break;
      }
    }

    Iterator(Iterator&&) noexcept = default;
    Iterator& operator=(Iterator&&) = default;

    // Forwards the iterator to the next row of the RowMap.
    void Next() {
      switch (rm_->mode_) {
        case Mode::kRange:
          range_it_->Next();
          break;
        case Mode::kBitVector:
          set_bits_it_->Next();
          break;
        case Mode::kIndexVector:
          iv_it_->Next();
          break;
      }
    }

    // Returns if the iterator is still valid.
    operator bool() const {
      switch (rm_->mode_) {
        case Mode::kRange:
          return *range_it_;
        case Mode::kBitVector:
          return *set_bits_it_;
        case Mode::kIndexVector:
          return *iv_it_;
      }
      PERFETTO_FATAL("For GCC");
    }

    // Returns the row pointed to by this iterator.
    uint32_t row() const {
      // RowMap uses the row/index nomenclature for referring to the mapping
      // from index to a row (as the name suggests). However, the data
      // structures used by RowMap use the index/ordinal naming (because they
      // don't have the concept of a "row"). Switch the naming here.
      switch (rm_->mode_) {
        case Mode::kRange:
          return range_it_->index();
        case Mode::kBitVector:
          return set_bits_it_->index();
        case Mode::kIndexVector:
          return iv_it_->index();
      }
      PERFETTO_FATAL("For GCC");
    }

    // Returns the index of the row the iterator points to.
    uint32_t index() const {
      // RowMap uses the row/index nomenclature for referring to the mapping
      // from index to a row (as the name suggests). However, the data
      // structures used by RowMap use the index/ordinal naming (because they
      // don't have the concept of a "row"). Switch the naming here.
      switch (rm_->mode_) {
        case Mode::kRange:
          return range_it_->ordinal();
        case Mode::kBitVector:
          return set_bits_it_->ordinal();
        case Mode::kIndexVector:
          return iv_it_->ordinal();
      }
      PERFETTO_FATAL("For GCC");
    }

   private:
    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;

    // Only one of the below will be non-null depending on the mode of the
    // RowMap.
    std::unique_ptr<RangeIterator> range_it_;
    std::unique_ptr<BitVector::SetBitsIterator> set_bits_it_;
    std::unique_ptr<IndexVectorIterator> iv_it_;

    const RowMap* rm_ = nullptr;
  };

  // Creates an empty RowMap.
  // By default this will be implemented using a range.
  RowMap();

  // Creates a RowMap containing the range of rows between |start| and |end|
  // i.e. all rows between |start| (inclusive) and |end| (exclusive).
  explicit RowMap(uint32_t start, uint32_t end);

  // Creates a RowMap backed by a BitVector.
  explicit RowMap(BitVector bit_vector);

  // Creates a RowMap backed by an std::vector<uint32_t>.
  explicit RowMap(std::vector<uint32_t> vec);

  // Creates a RowMap containing just |row|.
  // By default this will be implemented using a range.
  static RowMap SingleRow(uint32_t row) { return RowMap(row, row + 1); }

  // Creates a copy of the RowMap.
  // We have an explicit copy function because RowMap can hold onto large chunks
  // of memory and we want to be very explicit when making a copy to avoid
  // accidental leaks and copies.
  RowMap Copy() const;

  // Returns the size of the RowMap; that is the number of rows in the RowMap.
  uint32_t size() const {
    switch (mode_) {
      case Mode::kRange:
        return end_idx_ - start_idx_;
      case Mode::kBitVector:
        return bit_vector_.GetNumBitsSet();
      case Mode::kIndexVector:
        return static_cast<uint32_t>(index_vector_.size());
    }
    PERFETTO_FATAL("For GCC");
  }

  // Returns the row at index |row|.
  uint32_t Get(uint32_t idx) const {
    PERFETTO_DCHECK(idx < size());
    switch (mode_) {
      case Mode::kRange:
        return start_idx_ + idx;
      case Mode::kBitVector:
        return bit_vector_.IndexOfNthSet(idx);
      case Mode::kIndexVector:
        return index_vector_[idx];
    }
    PERFETTO_FATAL("For GCC");
  }

  // Returns whether the RowMap contains the given row.
  bool Contains(uint32_t row) const {
    switch (mode_) {
      case Mode::kRange: {
        return row >= start_idx_ && row < end_idx_;
      }
      case Mode::kBitVector: {
        return row < bit_vector_.size() && bit_vector_.IsSet(row);
      }
      case Mode::kIndexVector: {
        auto it = std::find(index_vector_.begin(), index_vector_.end(), row);
        return it != index_vector_.end();
      }
    }
    PERFETTO_FATAL("For GCC");
  }

  // Returns the first index of the given |row| in the RowMap.
  base::Optional<uint32_t> IndexOf(uint32_t row) const {
    switch (mode_) {
      case Mode::kRange: {
        if (row < start_idx_ || row >= end_idx_)
          return base::nullopt;
        return row - start_idx_;
      }
      case Mode::kBitVector: {
        return row < bit_vector_.size() && bit_vector_.IsSet(row)
                   ? base::make_optional(bit_vector_.GetNumBitsSet(row))
                   : base::nullopt;
      }
      case Mode::kIndexVector: {
        auto it = std::find(index_vector_.begin(), index_vector_.end(), row);
        return it != index_vector_.end()
                   ? base::make_optional(static_cast<uint32_t>(
                         std::distance(index_vector_.begin(), it)))
                   : base::nullopt;
      }
    }
    PERFETTO_FATAL("For GCC");
  }

  // Performs an ordered insert the row into the current RowMap (precondition:
  // this RowMap is ordered based on the rows it contains).
  //
  // Example:
  // this = [1, 5, 10, 11, 20]
  // Insert(10)  // this = [1, 5, 10, 11, 20]
  // Insert(12)  // this = [1, 5, 10, 11, 12, 20]
  // Insert(21)  // this = [1, 5, 10, 11, 12, 20, 21]
  // Insert(2)   // this = [1, 2, 5, 10, 11, 12, 20, 21]
  //
  // Speecifically, this means that it is only valid to call Insert on a RowMap
  // which is sorted by the rows it contains; this is automatically true when
  // the RowMap is in range or BitVector mode but is a required condition for
  // IndexVector mode.
  void Insert(uint32_t row) {
    switch (mode_) {
      case Mode::kRange:
        if (row == end_idx_) {
          // Fast path: if we're just appending to the end of the range, we can
          // stay in range mode and just bump the end index.
          end_idx_++;
        } else {
          // Slow path: the insert is somewhere else other than the end. This
          // means we need to switch to using a BitVector instead.
          bit_vector_.Resize(start_idx_, false);
          bit_vector_.Resize(end_idx_, true);
          *this = RowMap(std::move(bit_vector_));

          InsertIntoBitVector(row);
        }
        break;
      case Mode::kBitVector:
        InsertIntoBitVector(row);
        break;
      case Mode::kIndexVector: {
        PERFETTO_DCHECK(
            std::is_sorted(index_vector_.begin(), index_vector_.end()));
        auto it =
            std::upper_bound(index_vector_.begin(), index_vector_.end(), row);
        index_vector_.insert(it, row);
        break;
      }
    }
  }

  // Updates this RowMap by 'picking' the rows at indicies given by |picker|.
  // This is easiest to explain with an example; suppose we have the following
  // RowMaps:
  // this  : [0, 1, 4, 10, 11]
  // picker: [0, 3, 4, 4, 2]
  //
  // After calling Apply(picker), we now have the following:
  // this  : [0, 10, 11, 11, 4]
  //
  // Conceptually, we are performing the following algorithm:
  // RowMap rm = Copy()
  // for (idx : picker)
  //   rm[i++] = this[idx]
  // return rm;
  RowMap SelectRows(const RowMap& selector) const {
    uint32_t size = selector.size();

    // If the selector is empty, just return an empty RowMap.
    if (size == 0u)
      return RowMap();

    // If the selector is just picking a single row, just return that row
    // without any additional overhead.
    if (size == 1u)
      return RowMap::SingleRow(Get(selector.Get(0)));

    // For all other cases, go into the slow-path.
    return SelectRowsSlow(selector);
  }

  // Intersects |other| with |this| writing the result into |this|.
  // By "intersect", we mean to keep only the rows present in both RowMaps. The
  // order of the preserved rows will be the same as |this|.
  //
  // Conceptually, we are performing the following algorithm:
  // for (idx : this)
  //   if (!other.Contains(idx))
  //     Remove(idx)
  void Intersect(const RowMap& other) {
    uint32_t size = other.size();

    if (size == 0u) {
      // If other is empty, then we will also end up being empty.
      *this = RowMap();
      return;
    }

    if (size == 1u) {
      // If other just has a single row, see if we also have that row. If we
      // do, then just return that row. Otherwise, make ourselves empty.
      uint32_t row = other.Get(0);
      *this = Contains(row) ? RowMap::SingleRow(row) : RowMap();
      return;
    }

    if (mode_ == Mode::kRange && other.mode_ == Mode::kRange) {
      // If both RowMaps have ranges, we can just take the smallest intersection
      // of them as the new RowMap.
      // This case is important to optimize as it comes up with sorted columns.
      start_idx_ = std::max(start_idx_, other.start_idx_);
      end_idx_ = std::min(end_idx_, other.end_idx_);

      if (end_idx_ <= start_idx_)
        *this = RowMap();
      return;
    }

    // TODO(lalitm): improve efficiency of this if we end up needing it.
    RemoveIf([&other](uint32_t row) { return !other.Contains(row); });
  }

  // Filters the current RowMap into the RowMap given by |out| based on the
  // return value of |p(idx)|.
  //
  // Precondition: |out| should be sorted by the rows inside it (this is
  // required to keep this method efficient). This is automatically true if the
  // mode is out is Range or BitVector but needs to be enforced if the mode is
  // IndexVector.
  //
  // Specifically, the setup for each of the variables is as follows:
  //  this: contains the RowMap indices which will be looked up and passed to
  //        p to filter.
  //  out : contains indicies into |this| and will be filtered down to only
  //        contain indicies where p returns true.
  //  p   : takes an index given by |this| and returns whether the index should
  //        be retained in |out|.
  //
  // Concretely, the algorithm being invoked looks like (but more efficient
  // based on the mode of |this| and |out|):
  // for (idx : out)
  //   this_idx = (*this)[idx]
  //   if (!p(this_idx))
  //     out->Remove(idx)
  template <typename Predicate>
  void FilterInto(RowMap* out, Predicate p) const {
    PERFETTO_DCHECK(size() >= out->size());

    switch (mode_) {
      case Mode::kRange:
        FilterInto(out, RangeIterator(this), p);
        break;
      case Mode::kBitVector:
        FilterInto(out, bit_vector_.IterateSetBits(), p);
        break;
      case Mode::kIndexVector:
        FilterInto(out, IndexVectorIterator(this), p);
        break;
    }
  }

  template <typename Comparator>
  void StableSort(std::vector<uint32_t>* out, Comparator c) const {
    switch (mode_) {
      case Mode::kRange: {
        StableSort(out, c, [this](uint32_t off) { return start_idx_ + off; });
        break;
      }
      case Mode::kBitVector: {
        StableSort(out, c, [this](uint32_t off) {
          return bit_vector_.IndexOfNthSet(off);
        });
        break;
      }
      case Mode::kIndexVector: {
        StableSort(out, c, [this](uint32_t off) { return index_vector_[off]; });
        break;
      }
    }
  }

  // Returns the iterator over the rows in this RowMap.
  Iterator IterateRows() const { return Iterator(this); }

 private:
  enum class Mode {
    kRange,
    kBitVector,
    kIndexVector,
  };

  template <typename Iterator, typename Predicate>
  void FilterInto(RowMap* out, Iterator it, Predicate p) const {
    switch (out->mode_) {
      case Mode::kRange: {
        // TODO(lalitm): investigate whether we can reuse the data inside
        // out->bit_vector_ at some point.
        BitVector bv(out->end_idx_, false);
        for (auto out_it = bv.IterateAllBits(); it; it.Next(), out_it.Next()) {
          uint32_t ordinal = it.ordinal();
          if (ordinal < out->start_idx_)
            continue;
          if (ordinal >= out->end_idx_)
            break;

          if (p(it.index())) {
            out_it.Set();
          }
        }
        *out = RowMap(std::move(bv));
        break;
      }
      case Mode::kBitVector: {
        auto out_it = out->bit_vector_.IterateAllBits();
        for (; out_it; it.Next(), out_it.Next()) {
          PERFETTO_DCHECK(it);
          if (out_it.IsSet() && !p(it.index()))
            out_it.Clear();
        }
        break;
      }
      case Mode::kIndexVector: {
        PERFETTO_DCHECK(std::is_sorted(out->index_vector_.begin(),
                                       out->index_vector_.end()));
        auto fn = [&p, &it](uint32_t i) {
          while (it.ordinal() < i) {
            it.Next();
            PERFETTO_DCHECK(it);
          }
          PERFETTO_DCHECK(it.ordinal() == i);
          return !p(it.index());
        };
        auto iv_it = std::remove_if(out->index_vector_.begin(),
                                    out->index_vector_.end(), fn);
        out->index_vector_.erase(iv_it, out->index_vector_.end());
        break;
      }
    }
  }

  void InsertIntoBitVector(uint32_t row) {
    PERFETTO_DCHECK(mode_ == Mode::kBitVector);

    bit_vector_.Resize(row + 1, false);
    bit_vector_.Set(row);
  }

  // Removes any row where |p(row)| returns false from this RowMap.
  template <typename Predicate>
  void RemoveIf(Predicate p) {
    switch (mode_) {
      case Mode::kRange: {
        bit_vector_.Resize(start_idx_, false);
        for (uint32_t i = start_idx_; i < end_idx_; ++i) {
          if (p(i))
            bit_vector_.AppendFalse();
          else
            bit_vector_.AppendTrue();
        }
        *this = RowMap(std::move(bit_vector_));
        break;
      }
      case Mode::kBitVector: {
        for (auto it = bit_vector_.IterateSetBits(); it; it.Next()) {
          if (p(it.index()))
            it.Clear();
        }
        break;
      }
      case Mode::kIndexVector: {
        auto it = std::remove_if(index_vector_.begin(), index_vector_.end(), p);
        index_vector_.erase(it, index_vector_.end());
        break;
      }
    }
  }

  template <typename Comparator, typename Indexer>
  void StableSort(std::vector<uint32_t>* out, Comparator c, Indexer i) const {
    std::stable_sort(
        out->begin(), out->end(),
        [&c, &i](uint32_t a, uint32_t b) { return c(i(a), i(b)); });
  }

  RowMap SelectRowsSlow(const RowMap& selector) const;

  Mode mode_ = Mode::kRange;

  // Only valid when |mode_| == Mode::kRange.
  uint32_t start_idx_ = 0;  // This is an inclusive index.
  uint32_t end_idx_ = 0;    // This is an exclusive index.

  // Only valid when |mode_| == Mode::kBitVector.
  BitVector bit_vector_;

  // Only valid when |mode_| == Mode::kIndexVector.
  std::vector<uint32_t> index_vector_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_DB_ROW_MAP_H_

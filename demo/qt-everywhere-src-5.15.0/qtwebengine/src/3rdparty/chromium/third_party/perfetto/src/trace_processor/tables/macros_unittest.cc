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

#include "src/trace_processor/tables/macros.h"

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

#define PERFETTO_TP_TEST_EVENT_TABLE_DEF(NAME, PARENT, C) \
  NAME(TestEventTable, "event")                           \
  PARENT(PERFETTO_TP_ROOT_TABLE_PARENT_DEF, C)            \
  C(int64_t, ts, Column::Flag::kSorted)                   \
  C(int64_t, arg_set_id)
PERFETTO_TP_TABLE(PERFETTO_TP_TEST_EVENT_TABLE_DEF);

#define PERFETTO_TP_TEST_SLICE_TABLE_DEF(NAME, PARENT, C) \
  NAME(TestSliceTable, "slice")                           \
  PARENT(PERFETTO_TP_TEST_EVENT_TABLE_DEF, C)             \
  C(base::Optional<int64_t>, dur)                         \
  C(int64_t, depth)
PERFETTO_TP_TABLE(PERFETTO_TP_TEST_SLICE_TABLE_DEF);

#define PERFETTO_TP_TEST_CPU_SLICE_TABLE_DEF(NAME, PARENT, C) \
  NAME(TestCpuSliceTable, "cpu_slice")                        \
  PARENT(PERFETTO_TP_TEST_SLICE_TABLE_DEF, C)                 \
  C(int64_t, cpu)                                             \
  C(int64_t, priority)                                        \
  C(StringPool::Id, end_state)
PERFETTO_TP_TABLE(PERFETTO_TP_TEST_CPU_SLICE_TABLE_DEF);

class TableMacrosUnittest : public ::testing::Test {
 protected:
  StringPool pool_;

  TestEventTable event_{&pool_, nullptr};
  TestSliceTable slice_{&pool_, &event_};
  TestCpuSliceTable cpu_slice_{&pool_, &slice_};
};

TEST_F(TableMacrosUnittest, Name) {
  ASSERT_EQ(event_.table_name(), "event");
  ASSERT_EQ(slice_.table_name(), "slice");
  ASSERT_EQ(cpu_slice_.table_name(), "cpu_slice");
}

TEST_F(TableMacrosUnittest, InsertParent) {
  uint32_t id = event_.Insert(TestEventTable::Row(100, 0));
  ASSERT_EQ(id, 0u);
  ASSERT_EQ(event_.type().GetString(0), "event");
  ASSERT_EQ(event_.ts()[0], 100);
  ASSERT_EQ(event_.arg_set_id()[0], 0);

  id = slice_.Insert(TestSliceTable::Row(200, 123, 10, 0));
  ASSERT_EQ(id, 1u);

  ASSERT_EQ(event_.type().GetString(1), "slice");
  ASSERT_EQ(event_.ts()[1], 200);
  ASSERT_EQ(event_.arg_set_id()[1], 123);
  ASSERT_EQ(slice_.type().GetString(0), "slice");
  ASSERT_EQ(slice_.ts()[0], 200);
  ASSERT_EQ(slice_.arg_set_id()[0], 123);
  ASSERT_EQ(slice_.dur()[0], 10);
  ASSERT_EQ(slice_.depth()[0], 0);

  id = slice_.Insert(TestSliceTable::Row(210, 456, base::nullopt, 0));
  ASSERT_EQ(id, 2u);

  ASSERT_EQ(event_.type().GetString(2), "slice");
  ASSERT_EQ(event_.ts()[2], 210);
  ASSERT_EQ(event_.arg_set_id()[2], 456);
  ASSERT_EQ(slice_.type().GetString(1), "slice");
  ASSERT_EQ(slice_.ts()[1], 210);
  ASSERT_EQ(slice_.arg_set_id()[1], 456);
  ASSERT_EQ(slice_.dur()[1], base::nullopt);
  ASSERT_EQ(slice_.depth()[1], 0);
}

TEST_F(TableMacrosUnittest, InsertChild) {
  event_.Insert(TestEventTable::Row(100, 0));
  slice_.Insert(TestSliceTable::Row(200, 123, 10, 0));

  auto reason = pool_.InternString("R");
  uint32_t id = cpu_slice_.Insert(
      TestCpuSliceTable::Row(205, 456, 5, 1, 4, 1024, reason));
  ASSERT_EQ(id, 2u);
  ASSERT_EQ(event_.type().GetString(2), "cpu_slice");
  ASSERT_EQ(event_.ts()[2], 205);
  ASSERT_EQ(event_.arg_set_id()[2], 456);

  ASSERT_EQ(slice_.type().GetString(1), "cpu_slice");
  ASSERT_EQ(slice_.ts()[1], 205);
  ASSERT_EQ(slice_.arg_set_id()[1], 456);
  ASSERT_EQ(slice_.dur()[1], 5);
  ASSERT_EQ(slice_.depth()[1], 1);

  ASSERT_EQ(cpu_slice_.type().GetString(0), "cpu_slice");
  ASSERT_EQ(cpu_slice_.ts()[0], 205);
  ASSERT_EQ(cpu_slice_.arg_set_id()[0], 456);
  ASSERT_EQ(cpu_slice_.dur()[0], 5);
  ASSERT_EQ(cpu_slice_.depth()[0], 1);
  ASSERT_EQ(cpu_slice_.cpu()[0], 4);
  ASSERT_EQ(cpu_slice_.priority()[0], 1024);
  ASSERT_EQ(cpu_slice_.end_state()[0], reason);
  ASSERT_EQ(cpu_slice_.end_state().GetString(0), "R");
}

TEST_F(TableMacrosUnittest, NullableLongComparision) {
  slice_.Insert({});

  TestSliceTable::Row row;
  row.dur = 100;
  slice_.Insert(row);

  row.dur = 101;
  slice_.Insert(row);

  row.dur = 200;
  slice_.Insert(row);

  slice_.Insert({});

  Table out = slice_.Filter({slice_.dur().is_null()});
  const auto* dur = out.GetColumnByName("dur");
  ASSERT_EQ(out.size(), 2u);
  ASSERT_EQ(dur->Get(0).type, SqlValue::kNull);
  ASSERT_EQ(dur->Get(1).type, SqlValue::kNull);

  out = slice_.Filter({slice_.dur().is_not_null()});
  dur = out.GetColumnByName("dur");
  ASSERT_EQ(out.size(), 3u);
  ASSERT_EQ(dur->Get(0).long_value, 100);
  ASSERT_EQ(dur->Get(1).long_value, 101);
  ASSERT_EQ(dur->Get(2).long_value, 200);

  out = slice_.Filter({slice_.dur().lt(101)});
  dur = out.GetColumnByName("dur");
  ASSERT_EQ(out.size(), 1u);
  ASSERT_EQ(dur->Get(0).long_value, 100);

  out = slice_.Filter({slice_.dur().eq(101)});
  dur = out.GetColumnByName("dur");
  ASSERT_EQ(out.size(), 1u);
  ASSERT_EQ(dur->Get(0).long_value, 101);

  out = slice_.Filter({slice_.dur().gt(101)});
  dur = out.GetColumnByName("dur");
  ASSERT_EQ(out.size(), 1u);
  ASSERT_EQ(dur->Get(0).long_value, 200);

  out = slice_.Filter({slice_.dur().ne(100)});
  dur = out.GetColumnByName("dur");
  ASSERT_EQ(out.size(), 2u);
  ASSERT_EQ(dur->Get(0).long_value, 101);
  ASSERT_EQ(dur->Get(1).long_value, 200);

  out = slice_.Filter({slice_.dur().le(101)});
  dur = out.GetColumnByName("dur");
  ASSERT_EQ(out.size(), 2u);
  ASSERT_EQ(dur->Get(0).long_value, 100);
  ASSERT_EQ(dur->Get(1).long_value, 101);

  out = slice_.Filter({slice_.dur().ge(101)});
  dur = out.GetColumnByName("dur");
  ASSERT_EQ(out.size(), 2u);
  ASSERT_EQ(dur->Get(0).long_value, 101);
  ASSERT_EQ(dur->Get(1).long_value, 200);
}

TEST_F(TableMacrosUnittest, NullableLongCompareWrongType) {
  slice_.Insert({});

  TestSliceTable::Row row;
  row.dur = 100;
  slice_.Insert(row);

  row.dur = 101;
  slice_.Insert(row);

  row.dur = 200;
  slice_.Insert(row);

  slice_.Insert({});

  Table out = slice_.Filter({slice_.dur().ne_value(SqlValue())});
  ASSERT_EQ(out.size(), 0u);

  out = slice_.Filter({slice_.dur().eq_value(SqlValue::String("100"))});
  ASSERT_EQ(out.size(), 0u);

  out = slice_.Filter({slice_.dur().eq_value(SqlValue::Double(100.0))});
  ASSERT_EQ(out.size(), 0u);
}

TEST_F(TableMacrosUnittest, StringComparision) {
  cpu_slice_.Insert({});

  TestCpuSliceTable::Row row;
  row.end_state = pool_.InternString("R");
  cpu_slice_.Insert(row);

  row.end_state = pool_.InternString("D");
  cpu_slice_.Insert(row);

  cpu_slice_.Insert({});

  Table out = cpu_slice_.Filter({cpu_slice_.end_state().is_null()});
  const auto* end_state = out.GetColumnByName("end_state");
  ASSERT_EQ(out.size(), 2u);
  ASSERT_EQ(end_state->Get(0).type, SqlValue::kNull);
  ASSERT_EQ(end_state->Get(1).type, SqlValue::kNull);

  out = cpu_slice_.Filter({cpu_slice_.end_state().is_not_null()});
  end_state = out.GetColumnByName("end_state");
  ASSERT_EQ(out.size(), 2u);
  ASSERT_STREQ(end_state->Get(0).string_value, "R");
  ASSERT_STREQ(end_state->Get(1).string_value, "D");

  out = cpu_slice_.Filter({cpu_slice_.end_state().lt("R")});
  end_state = out.GetColumnByName("end_state");
  ASSERT_EQ(out.size(), 1u);
  ASSERT_STREQ(end_state->Get(0).string_value, "D");

  out = cpu_slice_.Filter({cpu_slice_.end_state().eq("D")});
  end_state = out.GetColumnByName("end_state");
  ASSERT_EQ(out.size(), 1u);
  ASSERT_STREQ(end_state->Get(0).string_value, "D");

  out = cpu_slice_.Filter({cpu_slice_.end_state().gt("D")});
  end_state = out.GetColumnByName("end_state");
  ASSERT_EQ(out.size(), 1u);
  ASSERT_STREQ(end_state->Get(0).string_value, "R");

  out = cpu_slice_.Filter({cpu_slice_.end_state().ne("D")});
  end_state = out.GetColumnByName("end_state");
  ASSERT_EQ(out.size(), 1u);
  ASSERT_STREQ(end_state->Get(0).string_value, "R");

  out = cpu_slice_.Filter({cpu_slice_.end_state().le("R")});
  end_state = out.GetColumnByName("end_state");
  ASSERT_EQ(out.size(), 2u);
  ASSERT_STREQ(end_state->Get(0).string_value, "R");
  ASSERT_STREQ(end_state->Get(1).string_value, "D");

  out = cpu_slice_.Filter({cpu_slice_.end_state().ge("D")});
  end_state = out.GetColumnByName("end_state");
  ASSERT_EQ(out.size(), 2u);
  ASSERT_STREQ(end_state->Get(0).string_value, "R");
  ASSERT_STREQ(end_state->Get(1).string_value, "D");
}

TEST_F(TableMacrosUnittest, FilterIdThenOther) {
  TestCpuSliceTable::Row row;
  row.cpu = 1;
  row.end_state = pool_.InternString("D");

  cpu_slice_.Insert(row);
  cpu_slice_.Insert(row);
  cpu_slice_.Insert(row);

  auto out =
      cpu_slice_.Filter({cpu_slice_.id().eq(0), cpu_slice_.end_state().eq("D"),
                         cpu_slice_.cpu().eq(1)});
  const auto* end_state = out.GetColumnByName("end_state");
  const auto* cpu = out.GetColumnByName("cpu");

  ASSERT_EQ(out.size(), 1u);
  ASSERT_EQ(cpu->Get(0).long_value, 1u);
  ASSERT_STREQ(end_state->Get(0).string_value, "D");
}

TEST_F(TableMacrosUnittest, Sort) {
  ASSERT_TRUE(event_.ts().IsSorted());

  event_.Insert(TestEventTable::Row(0 /* ts */, 100 /* arg_set_id */));
  event_.Insert(TestEventTable::Row(1 /* ts */, 1 /* arg_set_id */));
  event_.Insert(TestEventTable::Row(2 /* ts */, 3 /* arg_set_id */));

  Table out = event_.Sort({event_.arg_set_id().ascending()});
  const auto* ts = out.GetColumnByName("ts");
  const auto* arg_set_id = out.GetColumnByName("arg_set_id");

  ASSERT_FALSE(ts->IsSorted());
  ASSERT_TRUE(arg_set_id->IsSorted());

  ASSERT_EQ(arg_set_id->Get(0).long_value, 1);
  ASSERT_EQ(arg_set_id->Get(1).long_value, 3);
  ASSERT_EQ(arg_set_id->Get(2).long_value, 100);
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto

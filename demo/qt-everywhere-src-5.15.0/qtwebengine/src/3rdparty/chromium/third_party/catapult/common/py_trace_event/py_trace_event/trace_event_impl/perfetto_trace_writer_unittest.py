#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import StringIO

from py_trace_event.trace_event_impl import perfetto_trace_writer


class PerfettoTraceWriterTest(unittest.TestCase):
  """ Tests functions that write perfetto protobufs.

  TODO(crbug.com/944078): Switch to using python-protobuf library
  and implement proper protobuf parsing then.
  """
  def setUp(self):
    perfetto_trace_writer.reset_global_state()

  def testWriteThreadDescriptorEvent(self):
    result = StringIO.StringIO()
    perfetto_trace_writer.write_thread_descriptor_event(
        output=result,
        pid=1,
        tid=2,
        ts=1556716807306000,
    )
    expected_output = (
        '\n\x17P\x80\x80@\xc8\x02\x01\xe2\x02\r\x08\x01\x10'
        '\x020\x90\xf6\xc2\x82\xb6\xfa\xe1\x02'
    )
    self.assertEqual(expected_output, result.getvalue())

  def testWriteTwoEvents(self):
    result = StringIO.StringIO()
    perfetto_trace_writer.write_thread_descriptor_event(
        output=result,
        pid=1,
        tid=2,
        ts=1556716807306000,
    )
    perfetto_trace_writer.write_event(
        output=result,
        ph="M",
        category="category",
        name="event_name",
        ts=1556716807406000,
        args={},
        tid=2,
    )
    expected_output = (
       '\n\x17P\x80\x80@\xc8\x02\x01\xe2\x02\r\x08\x01\x10'
       '\x020\x90\xf6\xc2\x82\xb6\xfa\xe1\x02\n2P\x80\x80@Z\x0c\x08'
       '\xa0\x8d\x06\x18\x012\x04\x08\x01\x10Mb\x1e\n\x0c\x08\x01'
       '\x12\x08category\x12\x0e\x08\x01\x12\nevent_name'
    )
    self.assertEqual(expected_output, result.getvalue())

  def testWriteMetadata(self):
    result = StringIO.StringIO()
    perfetto_trace_writer.write_metadata(
        output=result,
        benchmark_start_time_us=1556716807306000,
        story_run_time_us=1556716807406000,
        benchmark_name="benchmark",
        benchmark_description="description",
        story_name="story",
        story_tags=["foo", "bar"],
        story_run_index=0,
        label="label",
    )
    expected_output = (
        '\nG\x82\x03D\x08\x90\xf6\xc2\x82\xb6\xfa\xe1'
        '\x02\x10\xb0\x83\xc9\x82\xb6\xfa\xe1\x02\x1a\tbenchmark"'
        '\x0bdescription*\x05label2\x05story:\x03foo:\x03bar@\x00'
    )
    self.assertEqual(expected_output, result.getvalue())

  def testWriteArgs(self):
    result = StringIO.StringIO()
    perfetto_trace_writer.write_thread_descriptor_event(
        output=result,
        pid=1,
        tid=2,
        ts=0,
    )
    perfetto_trace_writer.write_event(
        output=result,
        ph="M",
        category="",
        name="",
        ts=0,
        args={'int': 123, 'double': 1.23, 'string': 'onetwothree'},
        tid=2,
    )
    expected_output = (
       '\n\x10P\x80\x80@\xc8\x02\x01\xe2\x02\x06\x08\x01\x10'
       '\x020\x00\nQP\x80\x80@Z=\x08\x00\x18\x01"\x07R\x03int {"\x11'
       'R\x06double)\xaeG\xe1z\x14\xae\xf3?"\x15R\x06string2\x0bonetwothree2'
       '\x04\x08\x01\x10Mb\x0c\n\x04\x08\x01\x12\x00\x12\x04\x08\x01\x12\x00'
    )
    self.assertEqual(expected_output, result.getvalue())

  def testWriteChromeMetadata(self):
    result = StringIO.StringIO()
    perfetto_trace_writer.write_chrome_metadata(
        output=result,
        clock_domain='FOO',
    )
    expected_output = (
       '\n\x17*\x15\x12\x13\n\x0cclock-domain\x12\x03FOO'
    )
    self.assertEqual(expected_output, result.getvalue())

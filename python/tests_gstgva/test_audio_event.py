# ==============================================================================
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

import unittest
import gi
gi.require_version('GLib', '2.0')
from gi.repository import GLib

from gstgva.audio.audio_event import AudioEvent
from gstgva.audio.audio_event_meta import AudioEventMeta


class TestAudioEvent(unittest.TestCase):
    def setUp(self):
        self.meta = AudioEventMeta()
        self.meta._meta_flags = 7
        #self.meta._info
        self.meta.event_type = 9
        self.meta.id = 8
        self.meta.start_timestamp = 1
        self.meta.end_timestamp = 2
        #meta._params
        self.audio_event = AudioEvent(self.meta)

    def test_audio_event_segment(self):
        seg = self.audio_event.segment()
        self.assertEqual(seg.start_time, self.meta.start_timestamp)
        self.assertEqual(seg.end_time, self.meta.end_timestamp)

    def test_audio_event_label(self):
        label = self.audio_event.label()
        self.assertEqual(label, GLib.quark_to_string(self.meta.event_type))

    #def test_audio_event_confidence(self):

    def test_audio_event_confidence_None(self):
        self.assertEqual(self.audio_event.detection(), None)

    def tearDown(self):
        pass


if __name__ == '__main__':
    unittest.main()

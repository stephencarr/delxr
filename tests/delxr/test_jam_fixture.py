"""Validate the playable card fixtures independently of the ARM/emulator builds."""
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
CONTROL_PARAMS = ('oscAPulseWidth', 'oscAWavetablePosition', 'carrier1Feedback', 'modulator1Feedback')
ENGINES = ['delxrLxrDrum'] * 3 + ['delxrLxrSnare'] * 2 + ['delxrLxrCymbal', 'delxrLxrClosedHat', 'delxrLxrOpenHat']


class JamFixtureTest(unittest.TestCase):
    def setUp(self):
        self.kit = ET.parse(ROOT / 'sdcard/KITS/001 DELXR JAM.XML').getroot()
        self.song = ET.parse(ROOT / 'sdcard/SONGS/DELXR JAM.XML').getroot()

    def test_eight_native_rows_match_song(self):
        sounds = self.kit.findall('soundSources/sound')
        embedded = self.song.findall('instruments/kit/soundSources/sound')
        self.assertEqual([s.findtext('osc1/type') for s in sounds], ENGINES)
        self.assertEqual([s.find('osc1').get('type') for s in embedded], ENGINES)
        self.assertEqual(len({s.findtext('name') for s in sounds}), 8)
        for source, saved in zip(sounds, embedded):
            self.assertEqual(source.findtext('name'), saved.get('name'))
            self.assertEqual(source.findtext('polyphonic'), saved.get('polyphonic'))
        self.assertEqual([s.findtext('osc1/transpose') for s in sounds[:3]], ['0', '12', '19'])
        self.assertEqual([s.findtext('polyphonic') for s in sounds[6:]], ['choke', 'choke'])
        self.assertTrue(all(s.findtext('polyphonic') != 'choke' for s in sounds[:6]))

    def test_native_sequence_and_independent_parameters(self):
        clips = self.song.findall('sessionClips/instrumentClip')
        self.assertEqual(len(clips), 1)
        clip = clips[0]
        self.assertEqual(clip.get('length'), '192')
        self.assertEqual(clip.get('isPlaying'), '1')
        self.assertEqual(clip.get('instrumentPresetName'), self.song.find('instruments/kit').get('presetName'))
        rows = clip.findall('noteRows/noteRow')
        self.assertEqual([r.get('drumIndex') for r in rows], list(map(str, range(8))))
        for index, row in enumerate(rows):
            packed = row.get('noteDataWithSplitProb')[2:]
            self.assertTrue(packed)
            self.assertEqual(len(packed) % 28, 0)
            self.assertEqual(len(packed) // 28, [8, 3, 4, 4, 2, 2, 12, 4][index])
            positions = []
            for offset in range(0, len(packed), 28):
                note = packed[offset:offset + 28]
                position, duration, velocity = int(note[:8], 16), int(note[8:16], 16), int(note[16:18], 16)
                self.assertLess(position, 192)
                self.assertGreater(duration, 0)
                self.assertGreater(velocity, 0)
                self.assertLessEqual(velocity, 127)
                self.assertEqual(note[18:], '4014000000')
                positions.append(position)
            self.assertEqual(positions, sorted(set(positions)))
            kit_params = self.kit.findall('soundSources/sound')[index].find('defaultParams')
            self.assertGreater(int(kit_params.findtext('oscAPulseWidth'), 16), 0)
            self.assertLess(int(kit_params.findtext('oscAPulseWidth'), 16), 0x80000000)
            self.assertAlmostEqual(int(kit_params.findtext('oscAPulseWidth'), 16) / 2147483647,
                                   [.46, .33, .24, .40, .26, .38, .30, .38][index], places=7)
            for param in CONTROL_PARAMS:
                self.assertEqual(row.find('soundParams').get(param), kit_params.findtext(param))
        for param in CONTROL_PARAMS:
            self.assertEqual(len({r.find('soundParams').get(param) for r in rows[:3]}), 3)
        self.assertEqual(rows[3].find('soundParams').get('modulator1Feedback'), '0x80000000')
        self.assertNotEqual(rows[4].find('soundParams').get('modulator1Feedback'), '0x80000000')

    def test_gold_controls_and_native_tuning_filter(self):
        for sound in self.kit.findall('soundSources/sound'):
            knobs = [k.findtext('controlsParam') for k in sound.findall('modKnobs/modKnob')]
            self.assertEqual(len(knobs), 16)
            self.assertEqual(knobs[2:6], ['modulator1Feedback', 'oscAWavetablePosition', 'oscAPhaseWidth', 'carrier1Feedback'])
            self.assertEqual(knobs[6:8], ['lpfResonance', 'lpfFrequency'])
            self.assertIn('pitch', knobs)
            self.assertIn('volumePostFX', knobs)
            self.assertEqual(sound.findtext('modFXType'), 'none')


if __name__ == '__main__':
    unittest.main()

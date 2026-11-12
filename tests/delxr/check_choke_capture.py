#!/usr/bin/env python3
"""Compare two --choke-probe captures (native and --hat-choke off).

The private fixture triggers a long open hat at tick 0 and a silent but active
closed hat at tick 6. At 118 BPM the closed hit is 127ms later. Compare the
200..320ms tail of a complete subsequent cycle to avoid the transport-start
transient. No audition pad is held during the measured sequencer window.
"""
import argparse
import array
import json
import math
from pathlib import Path
import sys
import wave
import xml.etree.ElementTree as ET


def rms(samples):
    return math.sqrt(sum(x * x for x in samples) / max(1, len(samples)))


def measure(folder, expected_mode):
    result = json.loads((folder / 'result.json').read_text())
    assert result['choke_probe'] and result['hat_choke'] == expected_mode
    tree = ET.parse(folder / 'card-content/SONGS/DELXR JAM.XML')
    rows = tree.findall('./sessionClips/instrumentClip/noteRows/noteRow')
    assert rows[6].get('noteDataWithSplitProb') == '0x0000000600000003644014000000'
    assert rows[7].get('noteDataWithSplitProb') == '0x0000000000000003644014000000'
    assert rows[6].find('soundParams').get('oscAVolume') == '0x7FFFFFFF'
    assert rows[6].find('soundParams').get('volume') == '0x80000000'
    assert rows[7].find('soundParams').get('oscAPulseWidth') == '0x79999999'
    window = result['audio_windows'][0]
    with wave.open(str(folder / 'audio.wav'), 'rb') as wav:
        assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (2, 2, 44100)
        wav.setpos(window['start_frame'])
        samples = array.array('h', wav.readframes(window['frames']))
    if sys.byteorder != 'little':
        samples.byteswap()
    # 10ms blocks of interleaved stereo. Skip the first 2s so a complete loop's
    # onset is measured, rather than the first transport-edge partial note.
    block = 441 * 2
    levels = [rms(samples[i:i + block]) for i in range(0, len(samples), block)]
    onset = next(i for i in range(200, len(levels) - 33) if levels[i] > 40 and levels[i + 1] > 40)
    def region(a, b):
        return rms(samples[(onset * 441 + round(a * 44100)) * 2:(onset * 441 + round(b * 44100)) * 2])
    return {'onset_seconds_in_window': onset / 100, 'pre_trigger_rms_s16': region(.02, .08),
            'post_trigger_rms_s16': region(.20, .32), 'firmware_sha256': result['firmware_sha256']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native', type=Path)
    parser.add_argument('unchoked', type=Path)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    native, unchoked = measure(args.native, 'native'), measure(args.unchoked, 'off')
    ratio = unchoked['post_trigger_rms_s16'] / max(1, native['post_trigger_rms_s16'])
    result = {'native': native, 'unchoked': unchoked, 'tail_reduction_ratio': ratio,
              'tail_window_seconds_after_oh': [.20, .32], 'closed_hit_seconds_after_oh': 6 / 24 * 60 / 118}
    assert native['firmware_sha256'] == unchoked['firmware_sha256']
    assert min(native['pre_trigger_rms_s16'], unchoked['pre_trigger_rms_s16']) > 20, result
    assert unchoked['post_trigger_rms_s16'] > 20 and ratio > 10, result
    result['passed'] = 'Sequenced closed hat reduces the open-hat tail by more than 20dB using native choke'
    encoded = json.dumps(result, indent=2) + '\n'
    if args.out:
        args.out.write_text(encoded)
    print(encoded, end='')


if __name__ == '__main__':
    main()

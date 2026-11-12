#!/usr/bin/env python3
"""Run the DELXR JAM kit through source-built Delugemu, without a UI socket.

Loads the song, plays the native sequencer, turns both LEFT gold encoders on
both engine-control pages, saves, reloads, saves again, and checks the actual
firmware-written XML and stereo PCM. Captures full panel/OLED evidence.

Requires mtools, dosfstools and QEMU's shared libraries in PATH/LD_LIBRARY_PATH.
Use --isolate for independent sequenced family audibility checks. This tests
emulated firmware behavior; the captured OLEDs still require visual review.
"""
import argparse
import array
import json
import hashlib
import math
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import tempfile
import time
import wave
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
PARAMS = ('oscAPulseWidth', 'oscAWavetablePosition', 'carrier1Feedback', 'modulator1Feedback')
ENGINES = ['delxrLxrDrum'] * 3 + ['delxrLxrSnare'] * 2 + ['delxrLxrCymbal', 'delxrLxrClosedHat', 'delxrLxrOpenHat']
FAMILIES = {'all': list(range(8)), 'drum': [0, 1, 2], 'snare': [3], 'clap': [4], 'cymbal': [5], 'closed-hat': [6], 'open-hat': [7]}


def value(element, name):
    return element.get(name, element.findtext(name))


def read_song(path, hat_choke=True):
    document = ET.parse(path)
    sounds = document.findall('./instruments/kit/soundSources/sound')
    clips = document.findall('./sessionClips/instrumentClip')
    assert len(sounds) == 8 and len(clips) == 1, 'Expected one native eight-row kit and clip'
    assert [value(s.find('osc1'), 'type') for s in sounds] == ENGINES, 'Engine type did not survive firmware load/save'
    assert [value(s.find('osc1'), 'transpose') for s in sounds[:3]] == ['0', '12', '19']
    assert [value(s, 'polyphonic') for s in sounds[6:]] == (['choke', 'choke'] if hat_choke else ['mono', 'mono']), 'Hat choke mode was lost'
    assert clips[0].get('length') == '192', 'Two-bar sequence was lost'
    rows = clips[0].findall('noteRows/noteRow')
    assert [r.get('drumIndex') for r in rows] == list(map(str, range(8)))
    states = []
    for row in rows:
        packed = row.get('noteDataWithSplitProb', '')
        assert packed.startswith('0x') and len(packed) > 2 and (len(packed) - 2) % 28 == 0, 'Missing/current-schema sequence data'
        notes = [tuple(int(packed[offset + a:offset + b], 16) for a, b in
                       ((0, 8), (8, 16), (16, 18), (18, 20), (20, 22), (22, 26), (26, 28)))
                 for offset in range(2, len(packed), 28)]
        state = {p: value(row.find('soundParams'), p) for p in (*PARAMS, 'oscAVolume')}
        state['_notes'] = notes
        states.append(state)
    return states


class Panel:
    def __init__(self, sock, process, out):
        self.stream = sock.makefile('rw')
        self.process, self.out = process, out
        self.stream.readline()
        self.cmd('qmp_capabilities')

    def cmd(self, name, arguments=None):
        self.stream.write(json.dumps({'execute': name, 'arguments': arguments or {}}) + '\n')
        self.stream.flush()
        while True:
            line = self.stream.readline()
            if not line:
                raise RuntimeError('QMP disconnected; inspect emulator.log')
            reply = json.loads(line)
            if 'error' in reply:
                raise RuntimeError(reply['error'])
            if 'return' in reply:
                return reply['return']

    def pause(self, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise RuntimeError('QEMU exited unexpectedly; inspect emulator.log')
            time.sleep(min(.1, max(0, deadline - time.monotonic())))

    def key_state(self, code, down):
        self.cmd('input-send-event', {'events': [{'type': 'key', 'data': {'down': down, 'key': {'type': 'qcode', 'data': code}}}]})
        self.pause(.15 if down else .35)

    def key(self, code):
        self.key_state(code, True)
        self.key_state(code, False)

    def click(self, x, y):
        # QEMU input coordinates are normalized to its full 2256x1584 panel.
        self.cmd('input-send-event', {'events': [
            {'type': 'abs', 'data': {'axis': 'x', 'value': round(x * 32767 / 2255)}},
            {'type': 'abs', 'data': {'axis': 'y', 'value': round(y * 32767 / 1583)}}]})
        for down in (True, False):
            self.cmd('input-send-event', {'events': [{'type': 'btn', 'data': {'down': down, 'button': 'left'}}]})
            self.pause(.12 if down else .25)

    def shot(self, name):
        self.cmd('screendump', {'filename': str(self.out / (name + '.ppm'))})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', type=Path, required=True)
    parser.add_argument('--firmware', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--emulator-root', type=Path, default=ROOT / 'upstream/delugemu',
                        help='Delugemu source checkout containing scripts/mksd.sh and skin PNG')
    parser.add_argument('--isolate', choices=FAMILIES, default='all')
    parser.add_argument('--hat-choke', choices=['native', 'off'], default='native',
                        help='Diagnostic: disable native mutual hat choke in the private test copy only')
    parser.add_argument('--choke-probe', action='store_true',
                        help='With --isolate open-hat: long OH at tick0, silent active CH at tick6')
    args = parser.parse_args()
    if args.choke_probe and args.isolate != 'open-hat':
        parser.error('--choke-probe requires --isolate open-hat')
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    content = out / 'card-content'
    shutil.copytree(ROOT / 'sdcard', content, dirs_exist_ok=True)
    # Keep exactly one initial song, so the load browser cannot select an unrelated fixture.
    for path in (content / 'SONGS').iterdir():
        if path.name != 'DELXR JAM.XML' and path.is_file():
            path.unlink()
    original = content / 'SONGS/DELXR JAM.XML'
    document = ET.parse(original)
    if args.hat_choke == 'off':
        for sound in document.findall('./instruments/kit/soundSources/sound')[6:]:
            sound.set('polyphonic', 'mono')
    if args.isolate != 'all':
        for i, row in enumerate(document.findall('./sessionClips/instrumentClip/noteRows/noteRow')):
            if i not in FAMILIES[args.isolate]:
                row.find('soundParams').set('oscAVolume', '0x80000000')
    if args.choke_probe:
        rows = document.findall('./sessionClips/instrumentClip/noteRows/noteRow')
        rows[7].find('soundParams').set('oscAPulseWidth', '0x79999999')  # .95 unipolar decay
        rows[7].set('noteDataWithSplitProb', '0x0000000000000003644014000000')
        rows[6].set('noteDataWithSplitProb', '0x0000000600000003644014000000')
        rows[6].find('soundParams').set('oscAVolume', '0x7FFFFFFF')
        rows[6].find('soundParams').set('volume', '0x80000000')  # silent, but a source still triggers
    document.write(original, encoding='UTF-8', xml_declaration=True)
    baseline = read_song(original, args.hat_choke == 'native')
    edit_rows = [] if args.choke_probe else FAMILIES[args.isolate]
    card, raw = out / 'test-sd.img', out / 'qemu-output.wav'
    subprocess.run([str(args.emulator_root.resolve() / 'scripts/mksd.sh'), str(content), str(card)], check=True, timeout=60)
    windows = []
    def audio_position():
        return max(0, raw.stat().st_size - 44) // 4 * 4
    with tempfile.TemporaryDirectory(prefix='delxr-voices-') as tmp, (out / 'emulator.log').open('w') as log:
        qmp = Path(tmp) / 'qmp.sock'
        process = subprocess.Popen([str(args.qemu.resolve()), '-M', 'deluge', '-kernel', str(args.firmware.resolve()),
            '-drive', f'if=sd,format=raw,file={card}', '-display', 'none',
            '-global', f'deluge-skin.image={args.emulator_root.resolve()}/Delugemu_Normal.png',
            '-audiodev', f'wav,id=audio0,path={raw}', '-global', 'rza1l-ssif.audiodev=audio0',
            '-serial', 'null', '-monitor', 'none', '-qmp', f'unix:{qmp},server=on,wait=off'], stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 20
            while not qmp.exists():
                if process.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError('QEMU failed to start; inspect emulator.log')
                time.sleep(.1)
            with socket.socket(socket.AF_UNIX) as sock:
                sock.settimeout(20)
                sock.connect(str(qmp))
                panel = Panel(sock, process, out)
                panel.pause(10)
                # Prime QEMU's quadrature encoder state before loading the fixture.
                # These harmless startup edits are discarded by the subsequent song load.
                for _ in range(3):
                    panel.click(575, 469); panel.click(801, 196)
                panel.pause(1)
                panel.key('tab'); panel.pause(2); panel.key('l'); panel.pause(3); panel.shot('01-load-browser')
                panel.key('ret'); panel.pause(5); panel.shot('02-loaded')
                start = audio_position()
                panel.key('spc'); panel.pause(10); panel.shot('03-sequencing')
                windows.append((start, audio_position()))
                panel.key('spc'); panel.pause(2)
                # Native CLIP view; row audition selects its sound. Only the two
                # gold parameter encoders on the LEFT are rotated below.
                panel.key('c'); panel.pause(1); panel.shot('04-clip')
                for row in edit_rows:
                    panel.key_state(str(row + 1), True)
                    panel.shot(f'row-{row}-selected')
                    for page, button_x, lower_param, upper_param in ((1, 412, 3, 1), (2, 503, 0, 2)):
                        panel.click(button_x, 332)
                        panel.click(575, 469); panel.click(575, 469)
                        panel.shot(f'row-{row}-control-{lower_param}')
                        panel.click(801, 196); panel.click(801, 196)
                        panel.shot(f'row-{row}-control-{upper_param}')
                    panel.key_state(str(row + 1), False)
                panel.key('tab'); panel.pause(2); panel.key('s'); panel.pause(1); panel.shot('05-save-dialog')
                panel.key('ret'); panel.pause(3); panel.shot('06-saved')
                panel.key('l'); panel.pause(2); panel.shot('07-reload-browser')
                panel.key('ret'); panel.pause(5); panel.shot('08-reloaded')
                start = audio_position()
                panel.key('spc'); panel.pause(10); panel.shot('09-reloaded-sequencing')
                windows.append((start, audio_position()))
                panel.key('spc'); panel.pause(1)
                # A second firmware serialization verifies reloaded live state.
                panel.key('s'); panel.pause(1); panel.key('ret'); panel.pause(3); panel.shot('10-resaved')
                assert panel.cmd('query-status')['running']
                panel.cmd('quit'); process.wait(timeout=15)
                assert process.returncode == 0, f'QEMU exit status {process.returncode}'
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait(timeout=5)
    saved_dir = out / 'saved-songs'
    saved_dir.mkdir(exist_ok=True)
    subprocess.run(['mcopy', '-o', '-i', str(card), '::/SONGS/*.XML', str(saved_dir)], check=True, timeout=20)
    saved = sorted(p for p in saved_dir.glob('*.XML') if p.name != 'DELXR JAM.XML')
    assert len(saved) == 2, f'Expected two firmware saves, got {[p.name for p in saved]}'
    first, reloaded = [read_song(p, args.hat_choke == 'native') for p in saved]
    assert first == reloaded, 'Engine control values changed across save/reload/resave'
    for row in range(8):
        assert first[row]['_notes'] == baseline[row]['_notes'], f'Row {row} notes changed/dropped at load/save'
        assert first[row]['oscAVolume'] == baseline[row]['oscAVolume'], 'Isolation/mixing state changed'
        for param in PARAMS:
            if row in edit_rows:
                assert first[row][param] != baseline[row][param], f'Gold encoder did not change row {row} {param}'
            else:
                assert first[row][param] == baseline[row][param], f'Unselected row {row} was changed'
    assert len({tuple(first[row][param] for param in PARAMS) for row in range(3)}) == 3, 'Drum rows lost independent control states'
    # Individual values can legitimately converge after native knob quantization/acceleration;
    # the isolation passes additionally require every other row to remain byte-identical.
    capture = raw.read_bytes()
    assert capture[:4] == b'RIFF' and capture[8:16] == b'WAVEfmt '
    assert struct.unpack('<IHHIIHH', capture[16:36]) == (16, 1, 2, 44100, 176400, 4, 16)
    with wave.open(str(out / 'audio.wav'), 'wb') as wav:
        wav.setparams((2, 2, 44100, 0, 'NONE', 'not compressed')); wav.writeframes(capture[44:])
    audio_results = []
    for start, end in windows:
        samples = array.array('h', capture[44 + start:44 + end])
        if os.sys.byteorder != 'little':
            samples.byteswap()
        peak, nonzero = max(map(abs, samples), default=0), sum(s != 0 for s in samples)
        assert peak > 100 and nonzero > 4410, 'No meaningful synthesized audio during sequencer window'
        audio_results.append({'start_frame': start // 4, 'end_frame': end // 4, 'frames': len(samples) // 2, 'peak_s16': peak, 'nonzero_samples': nonzero, 'rms_s16': math.sqrt(sum(s * s for s in samples) / max(1, len(samples)))})
    log_text = (out / 'emulator.log').read_text()
    assert not any(marker in log_text.lower() for marker in ('assertion failed', 'aborted', 'fatal:')), 'Emulator fault in log'
    result = {'isolation': args.isolate, 'hat_choke': args.hat_choke, 'choke_probe': args.choke_probe,
        'firmware_sha256': hashlib.sha256(args.firmware.read_bytes()).hexdigest(), 'passed': ['eight native kit rows and engine types persisted',
        'independent drum controls and tuning persisted', 'both hat polyphony modes persisted',
        ('long-decay OH and silent CH probe preserved' if args.choke_probe else
         'four left gold encoder controls changed on each selected row'), 'two-bar sequencer audio before and after reload',
        'exact note positions/lengths/velocities/lift/probability/iterance/fill persisted',
        'reloaded controls persisted in second firmware save', 'clean QEMU exit'],
        'saved_songs': [p.name for p in saved], 'note_counts': [len(row['_notes']) for row in first], 'audio_windows': audio_results,
        'manual_review': ['Inspect PPM OLED captures for engine/control labels.',
                          'Listen to audio.wav; audio checks do not establish hardware feel or subjective quality.']}
    (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

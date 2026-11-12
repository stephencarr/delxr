#!/usr/bin/env python3
"""Exercise the native LXR editor using only the LEFT parameter encoders.

Capture an isolated JAM row before and after live waveform/FM/filter/transient
edits, verify a measurable signal change, and save/reload/resave all 28 fields.
Also checks muted OSC2 editing, partial/default/clamped/unknown XML fields and
an unchanged legacy source with no lxrParams tag. Requires the same QEMU/mtools
setup as delxr-voices-smoke.py, plus Pillow for OLED page readback.
No hardware or subjective fidelity assertion.
"""
import argparse
import array
from copy import deepcopy
import hashlib
import json
import math
import os
from pathlib import Path
import runpy
import shutil
import socket
import subprocess
import tempfile
import time
import wave
import xml.etree.ElementTree as ET

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
LEGACY = runpy.run_path(str(ROOT / 'scripts/delxr-voices-smoke.py'))
Panel = LEGACY['Panel']
PRESETS = json.loads((ROOT / 'tests/delxr/jam_presets.json').read_text())


def patches(path):
    document = ET.parse(path)
    return [[dict(sound.find(f'osc{osc}/lxrParams').attrib) if sound.find(f'osc{osc}/lxrParams') is not None else {}
             for osc in (1, 2)] for sound in document.findall('./instruments/kit/soundSources/sound')]


def features(samples):
    mono = [(samples[i] + samples[i + 1]) / 2 for i in range(0, len(samples) - 1, 2)]
    rms = math.sqrt(sum(s * s for s in mono) / max(1, len(mono)))
    frequencies = [55, 80, 110, 160, 220, 320, 440, 640, 880, 1280, 1760, 2560, 3520, 5120, 7040, 10240]
    size = 1024
    window = [.5 - .5 * math.cos(2 * math.pi * i / (size - 1)) for i in range(size)]
    energy = [0.] * len(frequencies)
    for segment in range(32):
        start = round(segment * max(0, len(mono) - size) / 31)
        block = [mono[start + i] * window[i] for i in range(size)]
        for j, frequency in enumerate(frequencies):
            coeff = 2 * math.cos(2 * math.pi * frequency / 44100)
            s1 = s2 = 0.
            for sample in block:
                s0 = sample + coeff * s1 - s2
                s2, s1 = s1, s0
            energy[j] += max(0., s1 * s1 + s2 * s2 - coeff * s1 * s2)
    total = sum(energy)
    profile = [e / max(total, 1.) for e in energy]
    return {'rms_s16': rms, 'peak_s16': max(map(abs, samples), default=0),
            'band_centroid_hz': sum(f * p for f, p in zip(frequencies, profile)), 'band_profile': profile}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', type=Path, required=True)
    parser.add_argument('--firmware', type=Path, required=True)
    parser.add_argument('--emulator-root', type=Path, default=ROOT / 'upstream/delugemu')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--row', type=int, choices=[1, 5], default=1, help='FM zap (1) or FM bell (5)')
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    content = out / 'card-content'
    shutil.copytree(ROOT / 'sdcard', content, dirs_exist_ok=True)
    for file in (content / 'SONGS').glob('*.XML'):
        if file.name != 'DELXR JAM.XML': file.unlink()
    song_file = content / 'SONGS/DELXR JAM.XML'
    document = ET.parse(song_file)
    sounds = document.findall('./instruments/kit/soundSources/sound')
    rows = document.findall('./sessionClips/instrumentClip/noteRows/noteRow')
    for index, row in enumerate(rows):
        if index != args.row: row.find('soundParams').set('oscAVolume', '0x80000000')
    # Legacy source must remain opt-out; newer defaults must not rewrite old patches.
    legacy_row = (args.row + 1) % 8
    sounds[legacy_row].find('osc1').remove(sounds[legacy_row].find('osc1/lxrParams'))
    # OSC2 is silent, but is a separately editable LXR source. Place its type AFTER
    # lxrParams to exercise deferred family/default parsing, including invalid values.
    osc2 = sounds[args.row].find('osc2'); osc2.clear()
    source2 = deepcopy(sounds[args.row].find('osc1/lxrParams'))
    source2.set('carrierWave', '999'); source2.set('filterType', '-7'); source2.set('futureIgnored', '42')
    del source2.attrib['carrierFine']
    del source2.attrib['carrierTune']
    osc2.append(source2)
    ET.SubElement(osc2, 'type').text = PRESETS[args.row]['engine']
    ET.SubElement(osc2, 'transpose').text = '0'
    ET.SubElement(osc2, 'cents').text = '0'
    document.write(song_file, encoding='UTF-8', xml_declaration=True)
    baseline = patches(song_file)
    expected2 = {k: str(v) for k, v in PRESETS[args.row]['patch'].items()}
    expected2.update(carrierWave='5', filterType='0', carrierFine='64', carrierTune='33' if args.row == 1 else '70')
    card, raw = out / 'test-sd.img', out / 'qemu-output.wav'
    subprocess.run([str(args.emulator_root.resolve() / 'scripts/mksd.sh'), str(content), str(card)], check=True, timeout=60)
    captures = []
    def audio_frame(): return max(0, raw.stat().st_size - 44) // 4
    with tempfile.TemporaryDirectory(prefix='delxr-sculpt-') as tmp, (out / 'emulator.log').open('w') as log:
        qmp = Path(tmp) / 'qmp.sock'
        process = subprocess.Popen([str(args.qemu.resolve()), '-M', 'deluge', '-kernel', str(args.firmware.resolve()),
            '-drive', f'if=sd,format=raw,file={card}', '-display', 'none',
            '-global', f'deluge-skin.image={args.emulator_root.resolve()}/Delugemu_Normal.png',
            '-audiodev', f'wav,id=audio0,path={raw}', '-global', 'rza1l-ssif.audiodev=audio0',
            '-serial', 'null', '-monitor', 'none', '-qmp', f'unix:{qmp},server=on,wait=off'], stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 20
            while not qmp.exists():
                if process.poll() is not None or time.monotonic() > deadline: raise RuntimeError('QEMU startup failed')
                time.sleep(.1)
            with socket.socket(socket.AF_UNIX) as sock:
                sock.settimeout(20); sock.connect(str(qmp))
                panel = Panel(sock, process, out); panel.pause(10)
                # Initialize quadrature states before loading; startup edits are discarded.
                for _ in range(3):
                    panel.click(575, 469); panel.click(801, 196); panel.click(347, 196); panel.click(120, 469)
                panel.pause(1); panel.key('tab'); panel.pause(2); panel.key('l'); panel.pause(2)
                panel.shot('01-load'); panel.key('ret'); panel.pause(5); panel.shot('02-loaded')
                panel.key('spc'); panel.pause(4.2)
                start = audio_frame(); panel.pause(8.3); captures.append((start, audio_frame()))
                # Playback remains running while entering and manipulating the editor.
                panel.key('c'); panel.pause(2); panel.key(str(args.row + 1)); panel.pause(.5)
                panel.key_state('shift', True); panel.click(321, 332); panel.key_state('shift', False)
                panel.pause(.5); panel.shot('03-editor-play-page')
                def oled_pixels(region):
                    panel.shot('navigation-readback')
                    with Image.open(out / 'navigation-readback.ppm') as screen:
                        return screen.crop(region).tobytes()
                def toggle_source():
                    region = (1171, 268, 1427, 282)
                    before = oled_pixels(region)
                    for attempt in range(4):
                        panel.click(120, 469); panel.pause(.5)
                        if oled_pixels(region) != before:
                            return
                    raise AssertionError('Y encoder did not change the OLED source header')
                def goto(page):
                    # First eight small MOD buttons are documented editor shortcuts;
                    # use only the LEFT X encoder to reach subsequent pages.
                    button = min(page, 7)
                    panel.click([321, 412, 503, 593, 684, 775, 865, 956][button], 332)
                    def title_pixels():
                        return oled_pixels((1171, 286, 1427, 302))
                    previous = title_pixels()
                    for _ in range(page - button):
                        # The emulator can drop a quadrature pulse while audio is
                        # busy. Count observed OLED page changes, not sent clicks.
                        for attempt in range(4):
                            panel.click(347, 196); panel.pause(.25)
                            current = title_pixels()
                            if current != previous:
                                previous = current
                                break
                        else:
                            raise AssertionError('X encoder did not advance the OLED page')
                def turn(which, steps):
                    cx, cy = (775, 196) if which == 'upper' else (549, 469)
                    for _ in range(abs(steps)): panel.click(cx + (26 if steps > 0 else -26), cy)
                edits = [(2, 'upper', 3, 'carrierWave'), (4, 'lower', 24, 'mod1Gain'),
                         (5, 'upper', 12, 'mod1Tune'), (9, 'lower', 24, 'transientLevel'),
                         (12, 'upper', 18, 'filterResonance')]
                for page, knob, amount, name in edits:
                    goto(page); panel.shot('page-before-' + name); turn(knob, amount); panel.shot('edited-' + name)
                # Source selection is the far-left vertical black encoder; OSC2 remains muted.
                goto(2); toggle_source(); panel.shot('04-osc2-before')
                turn('upper', -2); panel.shot('05-osc2-after')
                toggle_source(); panel.shot('06-osc1-unchanged')
                panel.pause(4.2)
                start = audio_frame(); panel.pause(8.3); captures.append((start, audio_frame()))
                panel.shot('07-playing-edited'); panel.key('spc'); panel.key('backspace'); panel.pause(.5)
                panel.key('tab'); panel.pause(2); panel.key('s'); panel.pause(1); panel.key('ret'); panel.pause(3)
                panel.shot('08-saved'); panel.key('l'); panel.pause(2); panel.key('ret'); panel.pause(5)
                panel.shot('09-reloaded'); panel.key('s'); panel.pause(1); panel.key('ret'); panel.pause(3)
                panel.shot('10-resaved'); assert panel.cmd('query-status')['running']
                panel.cmd('quit'); process.wait(timeout=15); assert process.returncode == 0
        finally:
            if process.poll() is None:
                process.terminate()
                try: process.wait(timeout=5)
                except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=5)
    saved_dir = out / 'saved-songs'; saved_dir.mkdir(exist_ok=True)
    subprocess.run(['mcopy', '-o', '-i', str(card), '::/SONGS/*.XML', str(saved_dir)], check=True, timeout=20)
    saved = sorted(p for p in saved_dir.glob('*.XML') if p.name != 'DELXR JAM.XML')
    assert len(saved) == 2, [p.name for p in saved]
    first, reloaded = map(patches, saved)
    assert first == reloaded, 'Native fields did not survive save/reload/resave'
    assert first[legacy_row][0] == {}, 'Legacy absent patch was silently enabled'
    changed = [name for _, _, _, name in edits]
    for index in range(8):
        if index == args.row:
            for key in baseline[index][0]:
                if key in changed: assert first[index][0][key] != baseline[index][0][key], f'Editor failed to change {key}'
                else: assert first[index][0][key] == baseline[index][0][key], f'Unexpected OSC1 edit: {key}'
            assert set(first[index][1]) == set(expected2), 'Unknown XML field retained or partial fields lost'
            assert first[index][1]['carrierWave'] != expected2['carrierWave'], 'Y encoder did not select/edit OSC2'
            for key in expected2:
                if key != 'carrierWave': assert first[index][1][key] == expected2[key], f'OSC2 field/default/clamp mismatch: {key}'
        else: assert first[index] == baseline[index], f'Unselected row {index} changed'
    notes_before = LEGACY['read_song'](song_file)
    for path in saved:
        notes_after = LEGACY['read_song'](path)
        assert notes_before == notes_after, 'Deep edits changed sequencer notes or standard primary controls'
    data = raw.read_bytes()
    with wave.open(str(out / 'audio.wav'), 'wb') as wav:
        wav.setparams((2, 2, 44100, 0, 'NONE', 'not compressed')); wav.writeframes(data[44:])
    measured = []
    for label, (start, end) in zip(('before', 'after'), captures):
        samples = array.array('h', data[44 + start * 4:44 + end * 4])
        if os.sys.byteorder != 'little': samples.byteswap()
        assert len(samples) >= 44100 * 2, 'Insufficient captured playback'
        metrics = features(samples); assert metrics['rms_s16'] > 10, f'{label}: synthesized row is silent'
        metrics.update(start_frame=start, end_frame=end)
        measured.append(metrics)
        with wave.open(str(out / (label + '.wav')), 'wb') as wav:
            wav.setparams((2, 2, 44100, 0, 'NONE', 'not compressed')); wav.writeframes(data[44 + start * 4:44 + end * 4])
    distance = sum(abs(a - b) for a, b in zip(measured[0]['band_profile'], measured[1]['band_profile']))
    rms_ratio = measured[1]['rms_s16'] / measured[0]['rms_s16']
    assert distance > .12 or abs(math.log(rms_ratio)) > .3, 'Native editor changes produced no meaningful measured signal difference'
    result = {'row': args.row, 'name': PRESETS[args.row]['name'], 'firmware_sha256': hashlib.sha256(args.firmware.read_bytes()).hexdigest(),
              'changed_native_controls': changed,
              'native_edits': {key: {'before': int(baseline[args.row][0][key]), 'after': int(first[args.row][0][key])}
                               for key in changed},
              'source2_changed': {'carrierWave': {'before': int(expected2['carrierWave']),
                                                 'after': int(first[args.row][1]['carrierWave'])}},
              'saved_songs': [path.name for path in saved], 'audio': measured,
              'normalized_band_distance': distance, 'rms_ratio_after_before': rms_ratio,
              'passed': ['left gold/black editor controls changed waveform/FM/filter/transient while sequencer played',
                         'OSC2 remained independently editable while muted', 'all28 fields saved/reloaded/resaved',
                         'partial XML default/clamp/unknown handling', 'legacy absent patch preserved',
                         'other rows and primary controls/notes unchanged', 'measurable non-silent PCM change', 'clean QEMU exit'],
              'manual_review': 'Inspect editor OLED screenshots; no subjective timbre or hardware-feel claim.'}
    (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n'); print(json.dumps(result, indent=2))


if __name__ == '__main__': main()

#!/usr/bin/env python3
"""Boot actual firmware, exercise panel input, and capture its audio over QMP.

Uses only Python's standard library. Inspect session.ppm to verify the DELXR
marker; this test does not pretend to OCR the OLED. Never touches hardware.
"""
import argparse
import array
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import time
import wave


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', type=Path, required=True)
    parser.add_argument('--firmware', type=Path, required=True)
    parser.add_argument('--sd', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    wav = out / 'audio.wav'
    raw_wav = out / 'qemu-output.wav'
    wav.unlink(missing_ok=True)
    raw_wav.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='delxr-') as tmp, (out / 'emulator.log').open('w') as log:
        sockpath = str(Path(tmp) / 'qmp.sock')
        env = dict(os.environ, DELUGE_INPUT_DEBUG='1')
        process = subprocess.Popen([
            str(args.qemu.resolve()), '-M', 'deluge', '-kernel', str(args.firmware.resolve()),
            '-drive', f'if=sd,format=raw,file={args.sd.resolve()}', '-display', 'none',
            '-global', f'deluge-skin.image={root}/upstream/delugemu/Delugemu_Normal.png',
            '-audiodev', f'wav,id=deluge0,path={raw_wav}', '-global', 'rza1l-ssif.audiodev=deluge0',
            '-serial', 'null', '-monitor', 'none', '-qmp', f'unix:{sockpath},server=on,wait=off',
        ], stdout=log, stderr=log, env=env)
        try:
            deadline = time.monotonic() + 20
            while not Path(sockpath).exists():
                if process.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError(f'QEMU did not start; see {out}/emulator.log')
                time.sleep(.1)
            with socket.socket(socket.AF_UNIX) as sock:
                sock.settimeout(15)
                sock.connect(sockpath)
                stream = sock.makefile('rw')
                json.loads(stream.readline())

                def command(name, arguments=None):
                    stream.write(json.dumps({'execute': name, 'arguments': arguments or {}}) + '\n')
                    stream.flush()
                    while True:
                        line = stream.readline()
                        if not line:
                            raise RuntimeError('QMP disconnected')
                        reply = json.loads(line)
                        if 'error' in reply:
                            raise RuntimeError(reply['error'])
                        if 'return' in reply:
                            return reply['return']

                def events(items):
                    command('input-send-event', {'events': items})

                def key(code):
                    for down in (True, False):
                        events([{'type': 'key', 'data': {'down': down, 'key': {'type': 'qcode', 'data': code}}}])
                        time.sleep(.15)

                def pointer(x, y, button):
                    events([{'type': 'abs', 'data': {'axis': 'x', 'value': round(x * 32767 / 2255)}},
                            {'type': 'abs', 'data': {'axis': 'y', 'value': round(y * 32767 / 1583)}}])
                    for down in (True, False):
                        events([{'type': 'btn', 'data': {'down': down, 'button': button}}])
                        time.sleep(.15)

                def capture(name):
                    command('screendump', {'filename': str(out / f'{name}.ppm')})
                    return out / f'{name}.ppm'

                def crop(path, box):
                    magic, size, depth, pixels = path.read_bytes().split(b'\n', 3)
                    width, height = map(int, size.split())
                    assert magic == b'P6' and depth == b'255' and (width, height) == (2256, 1584)
                    x0, y0, x1, y1 = box
                    return b''.join(pixels[(y * width + x0)*3:(y * width + x1)*3] for y in range(y0, y1))

                command('qmp_capabilities')
                time.sleep(10)
                assert command('query-status')['running']
                capture('boot')
                key('tab')
                time.sleep(1)
                session = capture('session')
                # Use several turns, as upstream enc_test.py does: a single
                # turn can be lost during startup while the encoder settles.
                for _ in range(8):
                    pointer(1811, 196, 'wheel-up')
                time.sleep(1)
                tempo = capture('tempo')
                assert crop(session, (1171, 268, 1427, 364)) != crop(tempo, (1171, 268, 1427, 364)), 'Tempo encoder did not change OLED'
                key('c')
                time.sleep(1)
                before = capture('before-pad')
                pointer(98, 1465, 'left')
                time.sleep(1)
                pad = capture('pad')
                assert crop(before, (60, 1427, 136, 1503)) != crop(pad, (60, 1427, 136, 1503)), 'Step pad did not change LED'
                key('spc')
                time.sleep(5)
                playing = capture('playing')
                assert crop(pad, (2050, 310, 2094, 354)) != crop(playing, (2050, 310, 2094, 354)), 'Play LED did not respond'
                key('spc')
                time.sleep(1)
                stopped = capture('stopped')
                assert crop(playing, (2050, 310, 2094, 354)) != crop(stopped, (2050, 310, 2094, 354)), 'Stop did not clear Play LED'
                command('quit')
                process.wait(timeout=15)
                assert process.returncode == 0, f'QEMU exited {process.returncode}'
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()

    # QEMU 11's WAV backend can leave RIFF/data sizes at zero on QMP quit.
    # Retain its original output and rewrap the unchanged, validated PCM payload.
    captured = raw_wav.read_bytes()
    assert captured[:4] == b'RIFF' and captured[8:16] == b'WAVEfmt '
    assert struct.unpack('<IHHIIHH', captured[16:36]) == (16, 1, 2, 44100, 176400, 4, 16)
    assert captured[36:40] == b'data' and (len(captured) - 44) % 4 == 0
    with wave.open(str(wav), 'wb') as audio:
        audio.setparams((2, 2, 44100, 0, 'NONE', 'not compressed'))
        audio.writeframes(captured[44:])
    with wave.open(str(wav)) as audio:
        assert (audio.getnchannels(), audio.getsampwidth(), audio.getframerate()) == (2, 2, 44100)
        samples = array.array('h', audio.readframes(audio.getnframes()))
        if sys.byteorder != 'little':
            samples.byteswap()
        peak = max(map(abs, samples), default=0)
        active = sum(s != 0 for s in samples)
        assert peak > 100 and active > 4410, 'No meaningful synthesized audio captured'
        summary = {'sample_rate': 44100, 'channels': 2, 'frames': len(samples)//2,
                   'peak_s16': peak, 'nonzero_samples': active,
                   'passed': ['OLED tempo encoder response', 'step pad LED feedback', 'play/stop LED feedback', 'non-silent stereo firmware audio'],
                   'manual': ['Inspect session.ppm for UNSAVED or DELXR', 'Listen to audio.wav; real-time output latency is not measured']}
    (out / 'result.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()

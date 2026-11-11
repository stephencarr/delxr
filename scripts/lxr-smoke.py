#!/usr/bin/env python3
"""Load the two-instrument LXR fixture, play it, save/reload, and inspect persisted engine types.

Requires mtools and the supplied experiments/001 card fixture. Never accesses hardware.
The rendered OLED captures need visual inspection; no OCR success is assumed.
"""
import argparse
import array
import json
import os
from pathlib import Path
import socket
import shutil
import struct
import subprocess
import tempfile
import time
import wave
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', type=Path, required=True)
    parser.add_argument('--firmware', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--isolate', choices=['both','low','high'], default='both', help='Mute the opposite instrument in a private fixture copy')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    card = out / 'test-sd.img'
    content=root/'experiments/001/sdcard'
    if args.isolate != 'both':
        variant=out/'card-content'
        shutil.copytree(content,variant,dirs_exist_ok=True)
        song=ET.parse(variant/'SONGS/LXR000.XML')
        muted_index=1 if args.isolate=='low' else 0
        song.findall('./sessionClips/instrumentClip')[muted_index].find('soundParams').set('oscAVolume','0x80000000')
        song.write(variant/'SONGS/LXR000.XML',encoding='UTF-8',xml_declaration=True)
        content=variant
    subprocess.run([str(root/'upstream/delugemu/scripts/mksd.sh'), str(content), str(card)], check=True)
    raw = out/'qemu-output.wav'
    with tempfile.TemporaryDirectory(prefix='delxr-lxr-') as tmp, (out/'emulator.log').open('w') as log:
        qmp = str(Path(tmp)/'qmp.sock')
        proc = subprocess.Popen([str(args.qemu.resolve()), '-M','deluge','-kernel',str(args.firmware.resolve()),
            '-drive',f'if=sd,format=raw,file={card}','-display','none',
            '-global',f'deluge-skin.image={root}/upstream/delugemu/Delugemu_Normal.png',
            '-audiodev',f'wav,id=audio0,path={raw}','-global','rza1l-ssif.audiodev=audio0',
            '-serial','null','-monitor','none','-qmp',f'unix:{qmp},server=on,wait=off'],stdout=log,stderr=log)
        try:
            deadline=time.monotonic()+20
            while not Path(qmp).exists():
                if proc.poll() is not None or time.monotonic()>deadline: raise RuntimeError('QEMU failed to start')
                time.sleep(.1)
            with socket.socket(socket.AF_UNIX) as sock:
                sock.settimeout(15); sock.connect(qmp); stream=sock.makefile('rw'); stream.readline()
                def cmd(name, arguments=None):
                    stream.write(json.dumps({'execute':name,'arguments':arguments or {}})+'\n'); stream.flush()
                    while True:
                        line=stream.readline()
                        if not line: raise RuntimeError('QMP disconnected')
                        reply=json.loads(line)
                        if 'error' in reply: raise RuntimeError(reply['error'])
                        if 'return' in reply: return reply['return']
                def key(code):
                    for down in (True,False):
                        cmd('input-send-event',{'events':[{'type':'key','data':{'down':down,'key':{'type':'qcode','data':code}}}]}); time.sleep(.15 if down else .3)
                def shot(name): cmd('screendump',{'filename':str(out/(name+'.ppm'))})
                cmd('qmp_capabilities'); time.sleep(10)
                key('tab'); time.sleep(2); key('l'); time.sleep(3); shot('load-browser'); key('ret'); time.sleep(5); shot('loaded')
                play_start=max(0, raw.stat().st_size-44)
                key('spc'); time.sleep(6); shot('playing')
                play_end=max(0, raw.stat().st_size-44)
                key('spc'); time.sleep(1)
                key('s'); time.sleep(1); shot('save-dialog'); key('ret'); time.sleep(3); shot('saved')
                # Reload the persisted song through the actual firmware browser.
                key('l'); time.sleep(1); key('ret'); time.sleep(4); shot('reloaded')
                assert cmd('query-status')['running']
                cmd('quit'); proc.wait(timeout=15); assert proc.returncode==0
        finally:
            if proc.poll() is None:
                proc.terminate()
                try: proc.wait(timeout=5)
                except subprocess.TimeoutExpired: proc.kill(); proc.wait()
    saved=out/'saved-LXR000-2.XML'
    subprocess.run(['mcopy','-o','-i',str(card),'::/SONGS/LXR000 2.XML',str(saved)],check=True)
    assert saved.read_bytes() != (content/'SONGS/LXR000.XML').read_bytes(), 'Song was not rewritten by firmware save'
    document=ET.parse(saved); sounds=document.findall('./instruments/sound'); clips=document.findall('./sessionClips/instrumentClip')
    assert len(sounds)==2 and len(clips)==2, 'Expected two independent instruments and clips'
    assert all(s.find('osc1').get('type')=='lxr' for s in sounds), 'Firmware failed to recognize/persist LXR engine'
    assert {s.get('presetName') for s in sounds}=={'LXR LOW','LXR HIGH'}
    sweeps={c.find('soundParams').get('oscAPulseWidth') for c in clips}
    assert len(sweeps)==2, 'Independent sweep parameters did not survive save/load'
    fm={c.find('soundParams').get('carrier1Feedback') for c in clips}
    assert len(fm)==2, 'Independent FM parameters did not survive save/load'
    if args.isolate != 'both':
        expected=['0x7FFFFFFF','0x80000000'] if args.isolate=='low' else ['0x80000000','0x7FFFFFFF']
        assert [c.find('soundParams').get('oscAVolume') for c in clips]==expected, 'Opposite instrument was not muted in firmware-written state'
    for c in clips:
        assert c.get('length')=='96'
        assert c.find('noteRows/noteRow') is not None
    capture=raw.read_bytes()
    assert capture[:4]==b'RIFF' and capture[8:16]==b'WAVEfmt '
    assert struct.unpack('<IHHIIHH',capture[16:36])==(16,1,2,44100,176400,4,16)
    with wave.open(str(out/'audio.wav'),'wb') as w:
        w.setparams((2,2,44100,0,'NONE','not compressed')); w.writeframes(capture[44:])
    samples=array.array('h',capture[44:]); peak=max(map(abs,samples),default=0); active=sum(x!=0 for x in samples)
    assert peak>100 and active>4410, 'No meaningful synthesized audio captured'
    playback=array.array('h',capture[44+(play_start//4)*4:44+(play_end//4)*4])
    playback_peak=max(map(abs,playback),default=0)
    playback_active=sum(x!=0 for x in playback)
    assert playback_peak>100 and playback_active>4410, 'No meaningful audio during sequencer playback window'
    result={'isolation':args.isolate,'passed':['two independent LXR instruments persisted','two 16-step clips persisted','distinct sweep and FM values persisted','song reloaded without crash','non-silent stereo PCM'],
            'peak_s16':peak,'nonzero_samples':active,'frames':len(samples)//2,
            'playback_frames':len(playback)//2,'playback_peak_s16':playback_peak,'playback_nonzero_samples':playback_active,
            'manual':['Inspect loaded.ppm for LXR000 and reloaded.ppm for LXR000 2; listen to audio.wav', 'Individual track audibility and live playing feel are not automatically asserted']}
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n'); print(json.dumps(result,indent=2))

if __name__=='__main__': main()

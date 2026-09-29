"""Bounded, window-only capture of the isolated submission runtime."""
import argparse
import json
import subprocess
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=['record', 'mark', 'stop'])
    parser.add_argument('directory', type=Path)
    parser.add_argument('--label', default='')
    args = parser.parse_args()
    root = args.directory.resolve()
    meta = root / 'recording.json'
    if args.action == 'mark':
        start = json.loads(meta.read_text())['start_epoch']
        event = {'seconds': round(time.time() - start, 2), 'label': args.label}
        with (root / 'events.jsonl').open('a', encoding='utf-8') as out:
            out.write(json.dumps(event, ensure_ascii=False) + '\n')
        print(json.dumps(event, ensure_ascii=False))
        return
    if args.action == 'stop':
        (root / 'stop_recording').touch()
        return
    if meta.exists():
        raise RuntimeError('Use a new directory for each recording.')
    root.mkdir(parents=True, exist_ok=True)
    command = ['ffmpeg', '-hide_banner', '-loglevel', 'warning', '-f', 'lavfi',
               '-i', 'gfxcapture=window_exe=SuidoNogyo.exe:max_framerate=30',
               '-vf', 'hwdownload,format=bgra',
               '-t', '1800', '-an', '-c:v', 'libx264', '-preset', 'veryfast',
               '-crf', '18', '-pix_fmt', 'yuv420p', str(root / 'raw.mp4')]
    started = time.time()
    meta.write_text(json.dumps({'start_epoch': started, 'command': command}, ensure_ascii=False), encoding='utf-8')
    with (root / 'capture.log').open('w', encoding='utf-8') as log:
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=log, stderr=log)
        try:
            while process.poll() is None:
                if (root / 'stop_recording').exists() or time.time() - started > 1800:
                    process.communicate(b'q', timeout=30)
                    break
                time.sleep(0.25)
        finally:
            if process.poll() is None:
                process.communicate(b'q', timeout=30)
    print(json.dumps({'exit_code': process.returncode, 'elapsed': time.time()-started}))
    if process.returncode:
        raise SystemExit(process.returncode)


if __name__ == '__main__':
    main()

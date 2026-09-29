"""Validate the exported MP4 and keep a local, reproducible evidence report."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('timeline', type=Path)
    args = parser.parse_args()
    timeline = json.loads(args.timeline.read_text(encoding='utf-8'))
    video = Path(timeline['output'])
    probe = json.loads(subprocess.check_output([
        'ffprobe', '-v', 'error', '-show_format', '-show_streams', '-of', 'json', str(video)]))
    streams = probe['streams']
    assert len(streams) == 1, 'Expected captioned video without audio'
    stream = streams[0]
    assert (stream['codec_name'], stream['width'], stream['height']) == ('h264', 1920, 1080)
    assert stream['pix_fmt'] == 'yuv420p' and stream['avg_frame_rate'] == '30/1'
    expected = sum((c['end'] - c['start']) / c.get('speed', 1) for c in timeline['clips'])
    assert abs(float(probe['format']['duration']) - expected) < 0.1
    decoded = subprocess.run(['ffmpeg', '-v', 'error', '-xerror', '-i', str(video),
                              '-map', '0:v:0', '-f', 'null', '-'], capture_output=True,
                             text=True, encoding='utf-8', errors='replace')
    assert decoded.returncode == 0 and not decoded.stderr.strip(), decoded.stderr
    black = subprocess.run(['ffmpeg', '-hide_banner', '-i', str(video), '-an', '-vf',
                            'blackdetect=d=0.25:pix_th=0.10', '-f', 'null', '-'],
                           capture_output=True, text=True, encoding='utf-8', errors='replace', check=True)
    intervals = [line for line in black.stderr.splitlines() if 'black_start:' in line]
    assert not intervals, intervals
    data = video.read_bytes()
    report = {
        'video': str(video), 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
        'duration_seconds': float(probe['format']['duration']), 'width': 1920, 'height': 1080,
        'codec': 'H.264', 'fps': 30, 'audio': 'none; captions only',
        'full_decode': 'pass', 'black_intervals_over_0_25s': intervals,
        'expected_timeline_seconds': expected, 'clip_count': len(timeline['clips']),
        'visual_review': 'Separate timestamped contact sheets; not an automatic gameplay certification',
    }
    target = video.parent / 'verification.json'
    target.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()

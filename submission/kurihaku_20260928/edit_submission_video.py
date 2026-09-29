"""Reproducible edits of actual gameplay footage, without modifying gameplay frames."""
import argparse
import json
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def run(command):
    subprocess.run(command, check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('timeline', type=Path)
    args = parser.parse_args()
    timeline = json.loads(args.timeline.read_text(encoding='utf-8'))
    raw = Path(timeline['source']).resolve()
    output = Path(timeline['output']).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    work = output.parent / 'edit_work'
    work.mkdir(exist_ok=True)
    font = 'C:/Windows/Fonts/meiryo.ttc'
    title_font = ImageFont.truetype(font, 27)
    caption_font = ImageFont.truetype(font, 31)
    files = []
    offset = 0.0
    evidence = []
    for index, clip in enumerate(timeline['clips']):
        duration = clip['end'] - clip['start']
        if duration <= 0:
            raise ValueError('Invalid clip duration')
        footer = Image.new('RGB', (1920, 72), '#182522')
        draw = ImageDraw.Draw(footer)
        draw.rectangle((0, 0, 1919, 3), fill='#97d5b2')
        if draw.textlength(clip['chapter'], font=title_font) > 380:
            raise ValueError('Chapter overflow')
        if draw.textlength(clip['caption'], font=caption_font) > 1392:
            raise ValueError('Caption overflow')
        draw.text((64, 18), clip['chapter'], font=title_font, fill='#97d5b2')
        draw.text((460, 16), clip['caption'], font=caption_font, fill='white')
        label = work / f'label_{index:02}.png'
        footer.save(label)
        target = work / f'clip_{index:02}.mp4'
        speed = clip.get('speed', 1.0)
        filters = (f'[0:v]setpts=(PTS-STARTPTS)/{speed},fps=30,'
                   'scale=1792:1008:flags=lanczos,format=yuv420p,'
                   'pad=1920:1080:64:0:color=0x182522,setsar=1[game];'
                   '[1:v]format=yuv420p[label];'
                   '[game][label]overlay=0:1008:shortest=1,format=yuv420p,setsar=1[v]')
        run(['ffmpeg', '-y', '-hide_banner', '-loglevel', 'error', '-ss', str(clip['start']),
             '-t', str(duration), '-i', str(raw), '-loop', '1', '-i', str(label),
             '-filter_complex', filters, '-map', '[v]', '-an', '-c:v', 'libx264',
             '-preset', 'fast', '-crf', '20', '-threads', '4', '-t', str(duration / speed), str(target)])
        files.append(target)
        evidence.append(dict(clip, output_start=round(offset, 3), output_end=round(offset+duration/speed, 3)))
        offset += duration / speed
        print(f'Encoded {index+1}/{len(timeline["clips"])}', flush=True)
    concat = work / 'concat.txt'
    concat.write_text('\n'.join("file '"+p.as_posix()+"'" for p in files), encoding='utf-8')
    run(['ffmpeg', '-y', '-hide_banner', '-loglevel', 'error', '-f', 'concat', '-safe', '0',
         '-i', str(concat), '-c', 'copy', '-movflags', '+faststart',
         '-metadata', 'title=水道農業 作品実演動画', '-metadata', 'comment=Actual gameplay; idle time edited; no audio.', str(output)])
    (output.parent / 'video_evidence.json').write_text(json.dumps(evidence, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'Output: {output}; duration about {offset:.2f}s', flush=True)


if __name__ == '__main__':
    main()

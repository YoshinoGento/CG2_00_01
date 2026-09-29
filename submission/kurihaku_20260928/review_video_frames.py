"""Create timestamped review sheets from a recorded video, not live UI capture."""
import argparse
import io
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('video', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--start', type=float, default=0)
    parser.add_argument('--end', type=float)
    parser.add_argument('--step', type=float, default=10)
    parser.add_argument('--ranges', help='Comma-separated start:end:step intervals')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    font = ImageFont.truetype('C:/Windows/Fonts/consola.ttf', 20)
    times = []
    ranges = ([tuple(map(float, item.split(':'))) for item in args.ranges.split(',')]
              if args.ranges else [(args.start, args.end, args.step)])
    for start, end, step in ranges:
        if end is None or step <= 0 or end <= start:
            raise ValueError('Invalid review interval')
        value = start
        while value < end:
            times.append(value)
            value += step
    for page in range(0, len(times), 20):
        batch = times[page:page + 20]
        sheet = Image.new('RGB', (1920, 300 * ((len(batch) + 3) // 4)), '#182522')
        draw = ImageDraw.Draw(sheet)
        for index, timestamp in enumerate(batch):
            data = subprocess.check_output([
                'ffmpeg', '-v', 'error', '-ss', str(timestamp), '-i', str(args.video),
                '-frames:v', '1', '-vf', 'scale=480:270', '-f', 'image2pipe', '-vcodec', 'png', '-'])
            frame = Image.open(io.BytesIO(data)).convert('RGB')
            x, y = (index % 4) * 480, (index // 4) * 300
            sheet.paste(frame, (x, y))
            draw.text((x + 8, y + 273), f'{timestamp:.2f}s', font=font, fill='white')
        target = args.output / f'sheet_{page // 20:02}.jpg'
        sheet.save(target, quality=90)
        print(target, flush=True)


if __name__ == '__main__':
    main()

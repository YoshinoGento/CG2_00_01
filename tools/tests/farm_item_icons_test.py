"""Offline item thumbnail regression checks; does not certify the DX12 frame."""
import json
from hashlib import sha256
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "tools/ui"))
import farm_item_icons as items
import generate_farm_hud_icons as atlas


class ItemIconTests(unittest.TestCase):
    def test_exact_sources_and_output_hash(self):
        manifest = json.loads(atlas.MANIFEST_PATH.read_text(encoding="utf-8"))
        self.assertEqual(manifest["sources"], items.source_hashes())
        self.assertEqual(manifest["atlas_sha256"], sha256(atlas.ATLAS_PATH.read_bytes()).hexdigest())

    def test_item_cells_use_baked_images_and_keep_seed_fruit_distinct(self):
        with Image.open(atlas.ATLAS_PATH) as sheet:
            for fruit, crop_slot, seed_slot in (("carrot", 4, 18), ("tomato", 5, 19), ("pumpkin", 6, 20)):
                images = []
                for name, slot in ((fruit, crop_slot), ("seed_" + fruit, seed_slot)):
                    x, y = slot % 8 * 64, slot // 8 * 64
                    baked = items.item_icon(name)
                    self.assertEqual(baked.tobytes(), sheet.crop((x, y, x+64, y+64)).tobytes())
                    self.assertGreater(np.count_nonzero(np.array(baked)[:, :, 3]), 250)
                    images.append(baked.tobytes())
                self.assertNotEqual(*images)

    def test_packet_front_matches_shop_uv(self):
        for name, (filename, slot) in items.PACKETS.items():
            with Image.open(ROOT / "project/Resources/ui" / filename) as source:
                expected = source.convert("RGBA").crop((slot*768, 0, (slot+1)*768, 1024))
                expected = expected.resize((39, 52), Image.Resampling.LANCZOS)
            actual = items.packet_icon(name).crop((12, 6, 51, 58))
            self.assertEqual(actual.tobytes(), expected.tobytes())

    def test_model_geometry_actually_changes_thumbnail(self):
        original = items.read_triangles
        def narrow(path):
            mesh = original(path)
            mesh[:, :, 0] *= .2
            return mesh
        before = items.crop_icon("pumpkin")
        with patch.object(items, "read_triangles", narrow):
            self.assertNotEqual(before.tobytes(), items.crop_icon("pumpkin").tobytes())

    def test_crop_hue_and_small_size_visibility(self):
        for name, dominant in (("carrot", 0), ("tomato", 0), ("pumpkin", 1)):
            image = items.crop_icon(name)
            for side in (48, 32):
                pixels = np.array(image.resize((side, side), Image.Resampling.LANCZOS))
                opaque = pixels[:, :, 3] > 200
                self.assertGreater(np.count_nonzero(opaque), 50)
                rgb = pixels[:, :, :3][opaque].mean(axis=0)
                self.assertEqual(int(np.argmax(rgb)), dominant)

    def test_malformed_obj_rejected(self):
        cases = ("", "v nan 0 0", "v 0 0 0\nf 1 2 3", "v 0 0 0\nf 1 1 1 1", "mtllib missing.mtl")
        for contents in cases:
            with self.subTest(contents=contents), patch.object(Path, "read_text", return_value=contents):
                with self.assertRaises(ValueError):
                    items.read_triangles(Path("invalid.obj"))

    def test_utility_art_is_unchanged_by_item_route(self):
        sheet = atlas.generate_atlas()
        for slot, (name, paint) in enumerate(atlas.ICONS):
            if name in items.PARTS or name in items.PACKETS:
                continue
            expected = Image.new("RGBA", (64, 64))
            paint(atlas.ImageDraw.Draw(expected))
            x, y = slot % 8 * 64, slot // 8 * 64
            self.assertEqual(expected.tobytes(), sheet.crop((x, y, x+64, y+64)).tobytes())


if __name__ == "__main__":
    unittest.main(verbosity=2)

"""Bounds, topology, texture availability and audio checks for the staged opening."""
import json
import math
import unittest
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'project/Resources/farm/arrival'


class ArrivalAssets(unittest.TestCase):
    def test_rooted_leaf_no_backward_hook(self):
        vertices=[]
        for line in (OUT/'leaf.obj').read_text().splitlines():
            if line.startswith('v '):
                x,y,z=map(float,line.split()[1:])
                vertices.append((-x,y,z)) # Match runtime handedness.
        self.assertGreaterEqual(min(p[1] for p in vertices),0)
        self.assertLessEqual(max(p[1] for p in vertices),.912)
        self.assertTrue(any(math.dist(p,(.64,.8,0))<.001 for p in vertices))
        previous=-1.0
        for section in range(16):
            segment=vertices[section*6:(section+1)*6]
            lower=(segment[0][0]+segment[4][0])*.5
            self.assertGreaterEqual(lower,previous-.00001)
            previous=(segment[1][0]+segment[2][0])*.5
        normals=[tuple(map(float,line.split()[1:])) for line in (OUT/'leaf.obj').read_text().splitlines() if line.startswith('vn ')]
        self.assertTrue(all(abs(n[2])>.99 for n in normals[:32]))
        self.assertTrue(all(n[2]*normals[0][2]>.99 for n in normals[:32]))
        # Each of six blades reaches the same ground plane; none is detached.
        for blade in range(6):
            base=vertices[blade*96:blade*96+6]
            self.assertTrue(any(abs(p[1])<.00001 for p in base))

    def test_meshes_are_bounded_finite_triangles(self):
        manifest = json.loads((OUT/'manifest.json').read_text())
        for name, count in manifest['triangles'].items():
            vertices, faces = [], []
            for line in (OUT/(name+'.obj')).read_text().splitlines():
                if line.startswith('v '):
                    v=tuple(map(float,line.split()[1:]))
                    self.assertTrue(all(math.isfinite(x) and abs(x)<200 for x in v))
                    vertices.append(v)
                elif line.startswith('f '):
                    faces.append([int(x.split('/')[0])-1 for x in line.split()[1:]])
            self.assertEqual(count,len(faces))
            self.assertLess(count,10000)
            for face in faces:
                self.assertEqual(len(face),3)
                self.assertTrue(all(0<=i<len(vertices) for i in face))
                a,b,c=(vertices[i] for i in face)
                u,v=tuple(b[i]-a[i] for i in range(3)),tuple(c[i]-a[i] for i in range(3))
                cross=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
                self.assertGreater(sum(x*x for x in cross),1e-14)

    def test_grass_excludes_plot_road_and_props(self):
        layout=json.loads((OUT/'grass_layout.json').read_text())
        self.assertLessEqual(len(layout['patches']),1024)
        self.assertLessEqual(layout['shadowCount'],len(layout['patches']))
        for patch in layout['patches']:
            x,_,z=patch['position']
            self.assertFalse(-6.8<x<5.8 and 1.8<z<13.2)
            self.assertFalse(5.4<x<10.8)
            self.assertTrue(.75<=patch['scale']<=1.2)

    def test_private_audio_is_short_pcm(self):
        for name,maximum in [('dew',.3),('morning',16.01)]:
            with wave.open(str(OUT/(name+'.wav'))) as audio:
                self.assertEqual(audio.getsampwidth(),2)
                self.assertGreater(audio.getnframes(),0)
                self.assertLessEqual(audio.getnframes()/audio.getframerate(),maximum)

    def test_shared_textures_exist(self):
        for path in ['title/countryside_atlas.png','title/sky.png','ui/farm_seed_packets.png','ui/farm_seed_packets_formal.png']:
            self.assertTrue((ROOT/'project/Resources'/path).is_file())


if __name__=='__main__':
    unittest.main()

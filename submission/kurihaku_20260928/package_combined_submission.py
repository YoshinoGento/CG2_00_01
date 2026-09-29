"""Assemble a local one-game submission with byte-preserved runtime04 artifacts."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET
import zipfile

from pypdf import PdfReader
from prepare_submission import OUT, ROOT, NS, copy_file, digest, write_json
from package_demo import original_saves, require

LOCAL = Path(__file__).parent
STAGE = OUT / 'combined_submission_01'
QA = OUT / 'combined_source_check_01'
EXTRACT = OUT / 'combined_zip_check_01'
GAME = '実行ファイル（水道農業）'
SOURCE = OUT / 'provenance_build/work/source'
RUNTIME = OUT / 'provenance_runtime/SuidoNogyo'
ZIP_HASH = 'dcd01a745ca9e7f140d5b5d661950f946e4881ea93ee6339be691239afe66042'
VIDEO_HASH = 'b8908891a2f1564e935d5d301202889a4739f6a275b34f1c89a913adc5143021'


def manifest(root):
    return [{'path': p.relative_to(root).as_posix(), 'bytes': p.stat().st_size,
             'sha256': digest(p)} for p in sorted(root.rglob('*')) if p.is_file()]


def check_paths(root):
    forbidden = {'.git', '.vs', '.codex', '__pycache__', 'node_modules'}
    for p in root.rglob('*'):
        require(not p.is_symlink() and not p.is_junction(), f'Link: {p.name}')
        if not p.is_file():
            continue
        rel = p.relative_to(root)
        require(not any(part in forbidden for part in rel.parts), 'Private/tool folder')
        require(p.suffix.lower() not in {'.pdb', '.ilk', '.obj', '.tlog', '.user', '.log', '.zip'}
                or ('Resources' in rel.parts and p.suffix.lower() == '.obj'), f'Build artifact: {rel}')
        require('human' not in [part.lower() for part in rel.parts], 'School model folder')
        if p.suffix.lower() in {'.vcxproj', '.props', '.targets', '.sln'}:
            text = p.read_text(encoding='utf-8-sig')
            require(not re.search(r'[A-Za-z]:[\\/](?:CG|Users)[\\/]', text), f'Host path: {rel}')
    require(len(list((root / GAME / 'Settings/farm').rglob('farm_*.json'))) == 2,
            'Expected one save and one catalog only')


def portable_project(project):
    tree = ET.parse(project)
    ET.register_namespace('', NS['m'])
    # Only distribution metadata changes: game code and resources remain identical.
    for parent in list(tree.iter()):
        for node in list(parent):
            condition = node.get('Condition', '')
            if any(c in condition for c in ('Debug|x64', 'Development|x64')):
                parent.remove(node)
            elif node.tag.endswith('ProjectConfiguration') and node.get('Include') != 'Release|x64':
                parent.remove(node)
    for node in tree.findall('.//m:PostBuildEvent/m:Command', NS):
        node.text = '\n'.join('copy /Y "$(ProjectDir)..\\..\\' + name + '" "$(TargetDir)' + name + '"'
                              for name in ('dxcompiler.dll', 'dxil.dll'))
    for node in tree.findall('.//m:Import[@Label="LocalAppDataPlatform"]', NS):
        # No dependency on reviewer-specific user property sheets.
        node.set('Condition', 'false')
    tree.write(project, encoding='utf-8', xml_declaration=True)
    for kind in ('ClCompile', 'ClInclude', 'FxCompile', 'ProjectReference'):
        for node in tree.findall(f'.//m:{kind}[@Include]', NS):
            rel = node.get('Include').replace('\\', '/')
            require('$' not in rel and (project.parent / rel).is_file(), f'Missing {kind}: {rel}')


def prepare(profile):
    require(not STAGE.exists() and not QA.exists(), 'Refusing to overwrite staging or QA')
    require(profile.is_file(), 'Completed profile missing')
    require(digest(OUT / 'SuidoNogyo_FreeFarming_20260928_04.zip') == ZIP_HASH, 'Runtime ZIP changed')
    saved = original_saves()
    built = json.loads((OUT / 'provenance_build/build_result.json').read_text(encoding='utf-8'))
    require(built['exit_code'] == 0, 'Source snapshot was not built successfully')
    source_manifest = json.loads((OUT / 'provenance_build/source_manifest.json').read_text(encoding='utf-8'))
    for entry in source_manifest['files']:
        require(digest(SOURCE / entry['path']) == entry['sha256'], f'Source changed: {entry["path"]}')
    runtime_manifest = json.loads((RUNTIME / 'MANIFEST.json').read_text(encoding='utf-8'))
    for entry in runtime_manifest['files']:
        require(digest(RUNTIME / entry['path']) == entry['sha256'], f'Runtime changed: {entry["path"]}')
        copy_file(RUNTIME / entry['path'], STAGE / GAME / entry['path'])
    copy_file(RUNTIME / 'MANIFEST.json', STAGE / GAME / 'MANIFEST.json')
    require(digest(SOURCE / 'generated/outputs/Release/CG2_00_01.exe') == digest(RUNTIME / 'SuidoNogyo.exe'),
            'Snapshot binary does not match shipped runtime')
    allowed = ('project/application/', 'project/engine/', 'project/externals/assimp-official/',
               'project/externals/DirectXTex/', 'project/externals/imgui/', 'project/externals/nlohmann/')
    root_files = {'project/CG2_00_01.sln', 'project/CG2_00_01.vcxproj',
                  'project/CG2_00_01.vcxproj.filters', 'project/.editorconfig'}
    source_target = STAGE / GAME / 'Source'
    count = 0
    for entry in source_manifest['files']:
        rel = entry['path']
        if rel.startswith(allowed) or rel in root_files:
            copy_file(SOURCE / rel, source_target / rel)
            count += 1
    for p in (RUNTIME / 'Resources').rglob('*'):
        if p.is_file():
            rel = p.relative_to(RUNTIME)
            # Runtime04 replaced this notice with the upstream font copyright text.
            if rel.as_posix() != 'Resources/fonts/OFL.txt':
                require(digest(SOURCE / 'project' / rel) == digest(p), f'Resource/source mismatch: {rel}')
            copy_file(p, source_target / 'project' / rel)
    portable_project(source_target / 'project/CG2_00_01.vcxproj')
    solution = source_target / 'project/CG2_00_01.sln'
    lines = solution.read_text(encoding='utf-8-sig').splitlines()
    solution.write_text('\n'.join(line for line in lines if 'Debug|x64' not in line and 'Development|x64' not in line)
                        + '\n', encoding='utf-8-sig')
    for name in ('BUILD_SOURCE.cmd', 'RunBuilt.cmd'):
        copy_file(LOCAL / name, source_target / name)
    copy_file(LOCAL / 'COMBINED_README.txt', STAGE / GAME / 'SUBMISSION_README.txt')
    pdf = OUT / 'program_guide/プログラム説明資料（水道農業）.pdf'
    video = OUT / 'gameplay_video/作品実演動画（水道農業）.mp4'
    require(len(PdfReader(pdf).pages) == 8, 'Unexpected PDF')
    require(digest(video) == VIDEO_HASH, 'Video changed')
    with zipfile.ZipFile(profile) as workbook:
        require(workbook.testzip() is None, 'Damaged profile XLSX')
        require(not any('vbaProject' in n or n.startswith('xl/externalLinks/') for n in workbook.namelist()),
                'Unexpected profile macro/external workbook')
    for p in (pdf, video, profile):
        copy_file(p, STAGE / p.name)
    check_paths(STAGE)
    write_json(OUT / 'combined_manifest_01.json', {'files': manifest(STAGE), 'runtime_sha256': ZIP_HASH,
               'source_snapshot_files': count, 'original_saves': saved,
               'source_changes': ['Release-only solution/project configurations', 'Relative DXC PostBuild paths',
                                  'No user property sheet dependency', 'Use runtime04 official font notice'],
               'known_crop_menu_issue': 'Retained by explicit user request'})
    shutil.copytree(STAGE, QA)
    require(original_saves() == saved, 'Original saves changed')
    print(f'Prepared {len(manifest(STAGE))} files; source entries {count}', flush=True)


def build():
    script = QA / GAME / 'Source/BUILD_SOURCE.cmd'
    require(script.is_file(), 'Run prepare first')
    with (OUT / 'combined_source_build_01.log').open('wb') as log:
        result = subprocess.run(['cmd.exe', '/d', '/c', str(script)], cwd=script.parent,
                                stdout=log, stderr=subprocess.STDOUT)
    write_json(OUT / 'combined_source_build_01.json', {'exit_code': result.returncode,
               'configuration': 'Release|x64', 'path': str(script.parent),
               'note': 'Only distribution metadata adapted; shipped runtime remains original04'})
    print(f'Clean source build exit={result.returncode}', flush=True)
    if result.returncode:
        raise SystemExit(result.returncode)


def archive(name):
    require(name.endswith('.zip') and Path(name).name == name, 'Expected a ZIP basename')
    require(not any(c in name for c in '/\\:*?"<>|'), 'Invalid archive basename')
    result = json.loads((OUT / 'combined_source_build_01.json').read_text(encoding='utf-8'))
    require(result['exit_code'] == 0, 'Matching clean source build required')
    saved = json.loads((OUT / 'combined_manifest_01.json').read_text(encoding='utf-8'))
    require(manifest(STAGE) == saved['files'], 'Staging modified after preparation')
    for entry in saved['files']:
        require(digest(QA / entry['path']) == entry['sha256'], 'Build-check input changed')
    check_paths(STAGE)
    target = OUT / 'ready' / name
    require(not target.exists() and not EXTRACT.exists(), 'Refusing archive/extraction overwrite')
    target.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(target, 'x', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for entry in saved['files']:
            z.write(STAGE / entry['path'], entry['path'])
    with zipfile.ZipFile(target) as z:
        require(z.testzip() is None, 'ZIP CRC failure')
        require(len(z.namelist()) == len(set(z.namelist())), 'Duplicate ZIP paths')
        for path in z.namelist():
            require(not Path(path).is_absolute() and '..' not in Path(path).parts, 'Unsafe ZIP path')
        z.extractall(EXTRACT)
    require(manifest(EXTRACT) == saved['files'], 'Extraction hash mismatch')
    require(original_saves() == saved['original_saves'], 'Original saves changed')
    require(digest(OUT / 'SuidoNogyo_FreeFarming_20260928_04.zip') == ZIP_HASH, 'Runtime ZIP changed')
    write_json(OUT / 'combined_result_01.json', {'archive': str(target), 'bytes': target.stat().st_size,
               'sha256': digest(target), 'files': len(saved['files']), 'crc': 'pass',
               'extraction_hashes': 'pass', 'source_release_build': 'pass', 'original_saves_unchanged': True,
               'upload': 'not performed', 'known_issue': 'Crop selection menu; unchanged by user request',
               'submission_round': 'initial assumed; add _02 etc. if resubmission'})
    print(f'ZIP complete: {target}; {target.stat().st_size} bytes', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=['prepare', 'build', 'archive'])
    parser.add_argument('--profile', type=Path)
    parser.add_argument('--name')
    args = parser.parse_args()
    if args.action == 'prepare':
        require(args.profile is not None, '--profile required')
        prepare(args.profile)
    elif args.action == 'build':
        build()
    else:
        require(args.name is not None, '--name required')
        archive(args.name)

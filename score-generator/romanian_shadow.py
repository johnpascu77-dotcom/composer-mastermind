#!/usr/bin/env python3
"""Composition bundle for the "romanian_shadow" style profile: "Joc de Umbre", 42 bars.

The form is the six sections of the Python quartet of the same name (v3.1):

    I   Joc strâmb         mm.  1-6    presets RO 01 02 03   root C
    II  Strigături         mm.  7-12   presets RO 05 06 07   root D
    III Joc cu pedală      mm. 13-18   presets RO 09 10 12   root Eb
    IV  Cântec de jale     mm. 19-28   presets RO 13 14 15 16 17   root Ab
    V   Joc posedat        mm. 29-36   presets RO 18 19 20 21   root F#
    VI  Umbre              mm. 37-42   presets RO 22 23 24   root Bb

Every 2 bars Mastermind tells AME which chain to enter: CC 85 (preset number, sent one bar ahead so AME
has time to prepare) and CC 90 (retrigger on the bar line). The preset NUMBERS are looked up here from the
user presets actually installed (AME numbers its lane: 7 factory presets, then user presets sorted by
name), so install the profile first:  python <AME repo>/profiles/romanian_shadow/make_presets.py --install

Optionally every section also broadcasts its pitch field (the Romanian minor set on the section root,
C D Eb F# G A Bb transposed) as Mastermind's 12-bit pitch-class mask on CC 110 / 111, for an
OrchNoteFilter in the chain. Nothing listens to it otherwise; leave it out with --no-field.

    python romanian_shadow.py                      # writes ../docs/example_score_ro_joc_de_umbre.json
    python romanian_shadow.py --channel 15 --no-field

Docs: docs/style_profile_romanian_shadow.md.
"""
import argparse
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
FACTORY = ['Init', 'Mirror', 'Retrograde inversion', 'Canon at the fifth', 'Horn entrance',
           'Augmented echo', 'Widen and drop']
ROMANIAN_MINOR = (0, 2, 3, 6, 7, 9, 10)  # relative to the section root

# (macro section, scene, archetype, root pitch class, first bar, [RO preset numbers, 2 bars each])
FORM = [
    ('Joc strâmb', 'ro-joc', 'presentation', 0, 0, [1, 2, 3]),
    ('Strigături', 'ro-strigaturi', 'build', 2, 6, [5, 6, 7]),
    ('Joc cu pedală', 'ro-pedala', 'build', 3, 12, [9, 10, 12]),
    ('Cântec de jale', 'ro-jale', 'release', 8, 18, [13, 14, 15, 16, 17]),
    ('Joc posedat', 'ro-posedat', 'peak', 6, 28, [18, 19, 20, 21]),
    ('Umbre', 'ro-umbre', 'release', 10, 36, [22, 23, 24]),
]


def upper_key(text):
    return ''.join(' ' if c == '_' else c.upper() for c in text)


def lane_names(directory):
    names = []
    for entry in os.listdir(directory):
        if not entry.endswith('.amechain'):
            continue
        with open(os.path.join(directory, entry), encoding='utf-8') as f:
            try:
                name = json.load(f).get('name') or entry[:-len('.amechain')]
            except ValueError:
                continue
        names.append(name)
    return FACTORY + sorted(names, key=upper_key)


def field_values(root):
    """The section's pitch-class set as MC's two-CC mask: low 7 bits = pcs 0-6, high 5 bits = pcs 7-11."""
    mask = 0
    for pc in ROMANIAN_MINOR:
        mask |= 1 << ((pc + root) % 12)
    return mask & 0x7F, (mask >> 7) & 0x1F


def scene(sid, name, mpl, overrides=(), patterns=(), rate=1, grid=0):
    return {
        'schema': 'ComposerMastermindScene.v1', 'id': sid, 'name': name, 'durationBars': 2, 'quantize': 'bar',
        'targets': ['MPL1', 'MPL2', 'MPL3'],
        'global': {'activePattern': 2, 'gridMode': grid, 'swing': 0.0, 'rate': rate},
        'instanceOverrides': [dict(targetInstance=t, activePattern=0 if t not in mpl else -1,
                                   gridMode=-1, swing=-1.0, rate=-1) for t in ('MPL1', 'MPL2', 'MPL3')
                              if t not in mpl] + list(overrides),
        'patterns': list(patterns), 'mutations': [], 'nextSceneId': '', 'transitionStyle': 'hard', 'rampBars': 0,
    }


def pattern(inst, transpose, inversion=False):
    return dict(targetInstance=inst, patternIndex=1, transpose=transpose, rotation=0, length=16,
                inversion=inversion)


def build(args):
    lane = lane_names(args.presets_dir)
    index = {}
    for i, name in enumerate(lane):
        if name.startswith('RO ') and name[3:5].isdigit():
            index[int(name[3:5])] = i
    sections = []
    for macro, scene_id, archetype, root, first_bar, presets in FORM:
        for k, number in enumerate(presets):
            if number not in index:
                raise SystemExit('preset "RO %02d ..." is not installed in %s' % (number, args.presets_dir))
            values = [{'modulatorTargetId': 'ame-preset', 'value': index[number]},
                      {'modulatorTargetId': 'ame-retrigger', 'value': 127}]
            if k == 0 and not args.no_field:
                lo, hi = field_values(root)
                values += [{'modulatorTargetId': 'ro-field-lo', 'value': lo},
                           {'modulatorTargetId': 'ro-field-hi', 'value': hi}]
            sections.append({
                'id': 'ro-%02d-%s' % (len(sections) + 1, scene_id), 'name': '%s %d' % (macro, k + 1),
                'sceneId': scene_id, 'startBar': first_bar + 2 * k, 'durationBars': 2, 'archetype': archetype,
                'layerRoles': [], 'budgetOverrides': [], 'reservedValues': [], 'modulatorValues': values,
            })
    scenes = [
        scene('ro-joc', 'Joc strâmb', ['MPL1', 'MPL2'], patterns=[pattern('MPL2', 7)]),
        scene('ro-strigaturi', 'Strigături', ['MPL1', 'MPL3'], rate=2,
              patterns=[pattern('MPL3', -12)]),
        scene('ro-pedala', 'Joc cu pedală', ['MPL1', 'MPL2', 'MPL3'],
              overrides=[dict(targetInstance='MPL3', activePattern=-1, gridMode=-1, swing=-1.0, rate=0)],
              patterns=[pattern('MPL3', -12), pattern('MPL2', 3)]),
        scene('ro-jale', 'Cântec de jale', ['MPL1'], rate=0),
        scene('ro-posedat', 'Joc posedat', ['MPL1', 'MPL2', 'MPL3'], grid=1,
              overrides=[dict(targetInstance='MPL2', activePattern=-1, gridMode=-1, swing=-1.0, rate=2)],
              patterns=[pattern('MPL2', 6, True), pattern('MPL3', -5)]),
        scene('ro-umbre', 'Umbre', ['MPL3'], rate=0, patterns=[pattern('MPL3', -12)]),
    ]
    targets = [
        {'id': 'ame-preset', 'ccNumber': 85, 'midiChannel': args.channel, 'mode': 'section',
         'arcDimension': '', 'sendAhead': True},
        {'id': 'ame-retrigger', 'ccNumber': 90, 'midiChannel': args.channel, 'mode': 'section',
         'arcDimension': '', 'sendAhead': True},
    ]
    if not args.no_field:
        targets += [
            {'id': 'ro-field-lo', 'ccNumber': 110, 'midiChannel': args.field_channel, 'mode': 'section',
             'arcDimension': '', 'sendAhead': False},
            {'id': 'ro-field-hi', 'ccNumber': 111, 'midiChannel': args.field_channel, 'mode': 'section',
             'arcDimension': '', 'sendAhead': False},
        ]
    instances = [
        {'id': 'MPL1', 'name': 'MPL1', 'midiChannel': 1, 'role': 'anchor', 'enabled': True, 'lastKnownScene': ''},
        {'id': 'MPL2', 'name': 'MPL2', 'midiChannel': 2, 'role': 'motif', 'enabled': True, 'lastKnownScene': ''},
        {'id': 'MPL3', 'name': 'MPL3', 'midiChannel': 3, 'role': 'counterpoint', 'enabled': True,
         'lastKnownScene': ''},
    ]
    return {
        'schema': 'ComposerMastermindComposition.v3',
        'blueprint': {'id': 'ro-joc-de-umbre', 'name': 'Joc de Umbre (romanian_shadow, 42 bars)',
                      'sections': sections},
        'scenes': scenes, 'instances': instances, 'motifPresets': [], 'modulatorTargets': targets,
        'modulationRoutes': [],
    }, index


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    appdata = os.environ.get('APPDATA', '')
    ap.add_argument('--presets-dir', default=os.environ.get('AME_CHAIN_PRESET_DIR') or
                    os.path.join(appdata, 'AtonalMotiveEngine', 'ChainPresets'))
    ap.add_argument('--channel', type=int, default=1, help="MIDI channel of the AME bridge CCs (AME accepts any)")
    ap.add_argument('--field-channel', type=int, default=1, help='channel of the pitch-field CCs 110/111')
    ap.add_argument('--no-field', action='store_true', help='leave the pitch-field broadcast out')
    ap.add_argument('--out', default=os.path.join(HERE, '..', 'docs', 'example_score_ro_joc_de_umbre.json'))
    args = ap.parse_args()
    bundle, index = build(args)
    with open(args.out, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(bundle, f, indent=2, ensure_ascii=False)
        f.write('\n')
    print('wrote %s: %d sections, lane numbers RO 01 = %d .. RO 24 = %d' %
          (os.path.normpath(args.out), len(bundle['blueprint']['sections']), index[1], index[24]))


if __name__ == '__main__':
    main()

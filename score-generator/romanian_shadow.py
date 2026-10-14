#!/usr/bin/env python3
"""Composition bundle for the "romanian_shadow" style profile: "Joc de Umbre", 42 bars, one AME per voice.

Mastermind is a conductor: it sends MIDI control changes. Here every VOICE is one Atonal Motive Transformer
(AME) instance on its own MIDI channel (V1 = channel 1 ... V4 = channel 4). For each two-bar sub-section the
score tells each voice which chain to enter (CC 85 = preset number, CC 90 = retrigger on the bar line, both sent
one bar ahead) - or leaves the voice silent (no retrigger). Optionally every voice's OrchNoteFilter also gets the
section's pitch field on CC 110 / 111.

The form is the six movements of the Python quartet of the same name:

    I   Joc strâmb       mm.  1-6    root C        III Joc cu pedală   mm. 13-18   root Eb
    II  Strigături       mm.  7-12   root D        IV  Cântec de jale  mm. 19-28   root Ab
    V   Joc posedat      mm. 29-36   root F#       VI  Umbre           mm. 37-42   root Bb

and the voices follow the quartet's characters (a plan you can read with --list and change with --plan):

    V1 high, agile    ornaments, calls, diminution        V3 mediator    canons, sequences, long lines
    V2 answering      related answers (DERIVE)            V4 low         drones, pedals, shadows

USE IT YOURSELF
    python romanian_shadow.py --list                      # what each voice plays, section by section
    python romanian_shadow.py                             # write ../docs/example_score_ro_joc_de_umbre.json
    python romanian_shadow.py --dump-plan my_plan.json    # write the plan as an editable file ...
    python romanian_shadow.py --plan my_plan.json         # ... and build the score from your edited copy
    python romanian_shadow.py --voices 2 --no-field       # fewer voices, no pitch-field CCs
    python romanian_shadow.py --channels 5,6,7,8          # other MIDI channels for the voices

A PLAN is the table "which preset does voice V play in sub-section k of movement M": a number is a preset
"RO nn ..." (1-24), a text is a preset name or the start of one ("16 Close", "RO 13"), 0 or null = the voice
rests (no retrigger). A movement's lists must all have one entry per two bars (its "subs").

Preset numbers are looked up from the AME presets actually installed, so install the profile first:
    python <AME repo>/profiles/romanian_shadow/make_presets.py --install

Docs: docs/style_profile_romanian_shadow.md.
"""
import argparse
import copy
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FACTORY = ['Init', 'Mirror', 'Retrograde inversion', 'Canon at the fifth', 'Horn entrance',
           'Augmented echo', 'Widen and drop']
ROMANIAN_MINOR = (0, 2, 3, 6, 7, 9, 10)  # relative to the section root
NOTE_NAMES = ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B']
VOICES = [('V1', 'high, agile'), ('V2', 'answering'), ('V3', 'mediator'), ('V4', 'low')]

# The default plan. Each movement: name, scene name, archetype, section root (pitch class), first bar (0-based),
# and per voice one entry per two bars (a preset number, a name, or 0 = rest).
DEFAULT_PLAN = {
    'title': 'Joc de Umbre (romanian_shadow, 42 bars)',
    'movements': [
        {'name': 'Joc strâmb', 'archetype': 'presentation', 'root': 0, 'firstBar': 0,
         'voices': {'V1': [1, 2, 1], 'V2': [0, 3, 3], 'V3': [0, 0, 4], 'V4': [0, 0, 0]}},
        {'name': 'Strigături', 'archetype': 'build', 'root': 2, 'firstBar': 6,
         'voices': {'V1': [7, 5, 7], 'V2': [8, 6, 8], 'V3': [0, 5, 6], 'V4': [8, 0, 8]}},
        {'name': 'Joc cu pedală', 'archetype': 'build', 'root': 3, 'firstBar': 12,
         'voices': {'V1': [10, 12, 10], 'V2': [12, 0, 12], 'V3': [0, 10, 0], 'V4': [9, 11, 9]}},
        {'name': 'Cântec de jale', 'archetype': 'release', 'root': 8, 'firstBar': 18,
         'voices': {'V1': [0, 0, 0, 15, 0], 'V2': [13, 0, 0, 0, 16], 'V3': [0, 14, 0, 0, 0],
                    'V4': [0, 0, 17, 0, 0]}},
        {'name': 'Joc posedat', 'archetype': 'peak', 'root': 6, 'firstBar': 28,
         'voices': {'V1': [18, 21, 18, 21], 'V2': [19, 19, 20, 19], 'V3': [20, 18, 19, 20],
                    'V4': [0, 12, 0, 12]}},
        {'name': 'Umbre', 'archetype': 'release', 'root': 10, 'firstBar': 36,
         'voices': {'V1': [0, 0, 23], 'V2': [24, 0, 0], 'V3': [0, 23, 0], 'V4': [22, 0, 24]}},
    ],
}


def upper_key(text):
    # exactly AME's ordering: '_' is a space and only a-z are upper-cased (other characters,
    # diacritics included, are compared as they are, by their UTF-8 / code point value)
    return ''.join(' ' if c == '_' else (c.upper() if 'a' <= c <= 'z' else c) for c in text)


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


def resolve(spec, lane, where):
    """A plan entry -> (lane index, preset name), or None for a rest."""
    if spec in (0, None, '', '-'):
        return None
    if isinstance(spec, int):
        prefix = 'RO %02d ' % spec
        matches = [i for i, n in enumerate(lane) if n.startswith(prefix)]
    else:
        exact = [i for i, n in enumerate(lane) if n.lower() == str(spec).lower()]
        matches = exact or [i for i, n in enumerate(lane) if n.lower().startswith(str(spec).lower())]
    if len(matches) != 1:
        raise SystemExit('%s: preset %r matches %d presets in the lane (need exactly one); '
                         'is the profile installed?' % (where, spec, len(matches)))
    return matches[0], lane[matches[0]]


def field_values(root):
    """The section's pitch-class set as MC's two-CC mask: low 7 bits = pcs 0-6, high 5 bits = pcs 7-11."""
    mask = 0
    for pc in ROMANIAN_MINOR:
        mask |= 1 << ((pc + root) % 12)
    return mask & 0x7F, (mask >> 7) & 0x1F


def load_plan(path):
    if not path:
        return copy.deepcopy(DEFAULT_PLAN)
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def check_plan(plan, voice_ids):
    for m in plan['movements']:
        subs = {len(m['voices'].get(v, [])) for v in voice_ids if v in m['voices']}
        if len(subs) != 1:
            raise SystemExit('movement "%s": every voice needs the same number of entries (found %s)' %
                             (m['name'], sorted(subs)))
        for v in voice_ids:
            if v not in m['voices']:
                m['voices'][v] = [0] * subs.copy().pop()


def scene(sid, name):
    return {'schema': 'ComposerMastermindScene.v1', 'id': sid, 'name': name, 'durationBars': 2,
            'quantize': 'bar', 'targets': [], 'global': {'activePattern': 1, 'gridMode': 0, 'swing': 0.0,
                                                          'rate': 1},
            'instanceOverrides': [], 'patterns': [], 'mutations': [], 'nextSceneId': '',
            'transitionStyle': 'hard', 'rampBars': 0}


def blueprint_id(plan, voice_count):
    """Unique per title and voice count, so a 2-voice score never replaces the 4-voice one on load."""
    import re, unicodedata
    title = re.sub(r'\([^)]*\)', '', (plan.get('title') or DEFAULT_PLAN['title']).lower())
    title = ''.join(c for c in unicodedata.normalize('NFD', title) if not unicodedata.combining(c))
    slug = re.sub(r'[^a-z0-9]+', '-', title).strip('-') or 'ame-score'
    return slug + '-' + str(voice_count) + 'v'


def build(plan, lane, voice_ids, channels, with_field):
    check_plan(plan, voice_ids)
    sections, scenes, used = [], [], set()
    for mi, m in enumerate(plan['movements']):
        sid = 'ro-m%d' % (mi + 1)
        scenes.append(scene(sid, m['name']))
        subs = len(m['voices'][voice_ids[0]])
        for k in range(subs):
            values = []
            for v in voice_ids:
                pick = resolve(m['voices'][v][k], lane, 'movement "%s", voice %s, step %d' % (m['name'], v, k + 1))
                if pick:
                    values += [{'modulatorTargetId': '%s-preset' % v.lower(), 'value': pick[0]},
                               {'modulatorTargetId': '%s-retrigger' % v.lower(), 'value': 127}]
                    used.add(pick[1])
                if k == 0 and with_field:
                    lo, hi = field_values(m['root'])
                    values += [{'modulatorTargetId': '%s-field-lo' % v.lower(), 'value': lo},
                               {'modulatorTargetId': '%s-field-hi' % v.lower(), 'value': hi}]
            sections.append({
                'id': 'ro-%02d-%s' % (len(sections) + 1, sid), 'name': '%s %d' % (m['name'], k + 1),
                'sceneId': sid, 'startBar': m['firstBar'] + 2 * k, 'durationBars': 2,
                'archetype': m['archetype'], 'layerRoles': [], 'budgetOverrides': [], 'reservedValues': [],
                'modulatorValues': values,
            })
    targets = []
    for v, ch in zip(voice_ids, channels):
        n = v.lower()
        targets += [
            {'id': n + '-preset', 'ccNumber': 85, 'midiChannel': ch, 'mode': 'section', 'arcDimension': '',
             'sendAhead': True},
            {'id': n + '-retrigger', 'ccNumber': 90, 'midiChannel': ch, 'mode': 'section', 'arcDimension': '',
             'sendAhead': True},
        ]
        if with_field:
            targets += [
                {'id': n + '-field-lo', 'ccNumber': 110, 'midiChannel': ch, 'mode': 'section',
                 'arcDimension': '', 'sendAhead': False},
                {'id': n + '-field-hi', 'ccNumber': 111, 'midiChannel': ch, 'mode': 'section',
                 'arcDimension': '', 'sendAhead': False},
            ]
    bundle = {
        'schema': 'ComposerMastermindComposition.v3',
        'blueprint': {'id': blueprint_id(plan, len(voice_ids)), 'name': plan.get('title', DEFAULT_PLAN['title']),
                      'sections': sections},
        'scenes': scenes, 'instances': [], 'motifPresets': [], 'modulatorTargets': targets,
        'modulationRoutes': [],
    }
    return bundle, used


def print_plan(plan, lane, voice_ids, channels):
    check_plan(plan, voice_ids)
    print('Voices: ' + ', '.join('%s = channel %d (%s)' % (v, ch, dict(VOICES).get(v, ''))
                                  for v, ch in zip(voice_ids, channels)))
    for m in plan['movements']:
        subs = len(m['voices'][voice_ids[0]])
        print('\n%s  (bars %d-%d, root %s, %s)' % (m['name'], m['firstBar'] + 1, m['firstBar'] + 2 * subs,
                                                   NOTE_NAMES[m['root'] % 12], m['archetype']))
        for v in voice_ids:
            cells = []
            for k, spec in enumerate(m['voices'][v]):
                pick = resolve(spec, lane, '%s %s' % (m['name'], v))
                cells.append('--' if pick is None else pick[1])
            print('  %s  ' % v + ' | '.join(cells))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    appdata = os.environ.get('APPDATA', '')
    ap.add_argument('--presets-dir', default=os.environ.get('AME_CHAIN_PRESET_DIR') or
                    os.path.join(appdata, 'AtonalMotiveEngine', 'ChainPresets'),
                    help="AME's user-preset folder (to look the preset numbers up)")
    ap.add_argument('--voices', type=int, default=4, help='how many voices, 1-4 (default 4)')
    ap.add_argument('--channels', default='', help='MIDI channel per voice, e.g. 1,2,3,4 (default)')
    ap.add_argument('--no-field', action='store_true', help='leave the pitch-field CCs 110/111 out')
    ap.add_argument('--plan', help='a plan file (see --dump-plan) instead of the built-in one')
    ap.add_argument('--dump-plan', metavar='FILE', help='write the built-in plan to FILE and stop')
    ap.add_argument('--list', action='store_true', help='print what each voice plays, then stop')
    ap.add_argument('--out', default=os.path.join(HERE, '..', 'docs', 'example_score_ro_joc_de_umbre.json'))
    args = ap.parse_args()

    if args.dump_plan:
        with open(args.dump_plan, 'w', encoding='utf-8', newline='\n') as f:
            json.dump(DEFAULT_PLAN, f, indent=2, ensure_ascii=False)
            f.write('\n')
        print('wrote', args.dump_plan, '- edit it, then:  python romanian_shadow.py --plan', args.dump_plan)
        return
    if not 1 <= args.voices <= 4:
        sys.exit('--voices must be 1..4')
    voice_ids = [v for v, _ in VOICES[:args.voices]]
    channels = [int(c) for c in args.channels.split(',') if c.strip()] or list(range(1, args.voices + 1))
    if len(channels) != len(voice_ids) or any(not 1 <= c <= 16 for c in channels):
        sys.exit('--channels needs %d numbers between 1 and 16' % len(voice_ids))
    lane = lane_names(args.presets_dir)
    plan = load_plan(args.plan)
    if args.list:
        print_plan(plan, lane, voice_ids, channels)
        return
    bundle, used = build(plan, lane, voice_ids, channels, not args.no_field)
    with open(args.out, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(bundle, f, indent=2, ensure_ascii=False)
        f.write('\n')
    print('wrote %s: %d sections, %d voices on channels %s, %d targets, %d different presets used' %
          (os.path.normpath(args.out), len(bundle['blueprint']['sections']), len(voice_ids),
           ','.join(map(str, channels)), len(bundle['modulatorTargets']), len(used)))


if __name__ == '__main__':
    main()

"""Specs for the newer plug-ins (PolyGate, SeedRandomizer, MorphQuantizer, ChordHarmonizer, MpeBend, MidiSampler).

Every control text is read from the plug-in's own editor source (the tooltip strings) and parameter layout
(ranges, choices, defaults), so the manual and the tooltips come from one place.
Run:  python make_guides.py [name ...]
"""
import os
import re

from guide_lib import parse_params, describe_param, split_top, strings

REPOS = 'C:/AudioDev/Repos'


def read(path):
    with open(path, encoding='utf-8') as f:
        return f.read()


def add_calls(source, names=('addCombo', 'addSlider', 'addToggle', 'addHeader', 'addMask')):
    """Yields (kind, [string args]) for editor helper calls like addSlider ("id", "Label", "tip")."""
    pattern = r'\b(%s)\s*\(' % '|'.join(names)
    for m in re.finditer(pattern, source):
        i = m.end()
        depth, j, quote = 1, i, None
        while j < len(source) and depth:
            c = source[j]
            if quote:
                if c == '\\':
                    j += 1
                elif c == quote:
                    quote = None
            elif c in '"\'':
                quote = c
            elif c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
            j += 1
        yield m.group(1), split_top(source[i:j - 1])


def lit(arg):
    s = strings(arg)
    return s[0] if len(s) == 1 and arg.strip().startswith('"') and arg.strip().endswith('"') else None


# ---------------------------------------------------------------------------------------------------------- PolyGate
def polygate():
    root = REPOS + '/PolyGate'
    params = parse_params(read(root + '/Source/PolyGateProcessor.cpp'))
    editor = read(root + '/Source/PolyGateEditor.cpp')
    sections, current = [], None
    notes_by_section = {
        'Limit': 'Lookahead is a constant lateness of everything after the plug-in (a MIDI plug-in cannot report latency): record the result to a clip or shift the track. CC, pedal and pitch bend are delayed by the same amount, so they stay in step with the notes.',
        'Debris': 'Drop-exact debris removal needs Lookahead ≥ Min length + Event window; with less lookahead short notes are lengthened instead of dropped.',
    }
    for kind, args in add_calls(editor):
        if kind == 'addHeader':
            current = {'title': lit(args[0]), 'controls': [], 'notes': []}
            if current['title'] in notes_by_section:
                current['notes'].append(notes_by_section[current['title']])
            sections.append(current)
            continue
        pid, label, tip = lit(args[0]), lit(args[1]), lit(args[2])
        if pid is None or current is None:
            continue
        choices, default = describe_param(params.get(pid))
        current['controls'].append({'name': label, 'id': pid, 'choices': choices, 'default': default, 'desc': tip})
    sections[0]['note'] = ('Thins dense polyphonic MIDI by choosing which notes live, instead of chopping the oldest one. '
                           'Controls run top to bottom: the voice limit, vertical events (chords and clusters), debris (tiny notes), and register bands.')
    sections.insert(0, {'title': 'The Editor', 'note': 'One column of settings, then a live counter line. Hover any control for a short reminder of what it does.',
                        'image': 'assets/polygate-editor.png', 'caption': 'The editor, rendered by PolyGateProcessorCheck.', 'controls': []})
    sections.append({'title': 'Watch This Live', 'notes': [
        'The line at the bottom counts **notes in**, **out**, **removed** and **sounding now**. If a track thins out more than you expect, watch “removed” while you change Max voices, Event window and Min length; **Reset counters** zeroes it without touching any setting.',
        'Measured on a dense stream (MidiSampler cascade output, Notes hand-over, 12 voices: 6635 notes, 17 % debris, up to 31 notes at once): a classic 6-voice limiter kept 4094 notes (43 % debris); PolyGate realtime with Keep sounding kept 1642 (13 %); PolyGate with lookahead 1/4, event 1/8 max 3, min 1/16 kept 718 (0 % debris).']})
    return {
        'name': 'PolyGate', 'crumbs': 'Ecosystem → MIDI effects → <b>PolyGate</b>',
        'role': 'Polyphony gate: keeps the notes worth keeping when a stream gets too dense.',
        'badges': [('chain', 'MIDI effect'), ('live', 'VST3 · Standalone'), ('plugin-code', 'no latency report')],
        'lede': 'A MIDI effect that thins dense polyphonic MIDI by **choosing** which notes live, instead of chopping the oldest one. Built for **MidiSampler** output (many slices, cascades, vertical events) and polyphonic piano material. With a lookahead it sees whole chords and each note’s real length, so it can drop debris exactly; without one it still follows the same rules in real time.',
        'chain': ['MidiSampler / source', ('PolyGate', True), 'SeedRandomizer / MorphQuantizer', 'instrument'],
        'sections': sections,
        'source': 'Source: <span class="mono">Source/PolyGateEditor.cpp</span> (the tooltips) and <span class="mono">PolyGateProcessor.cpp</span> (the parameters). Design notes: <span class="mono">Docs/polygate_scoping.md</span>.',
        'out': root + '/Docs/PolyGate_Guide.html',
    }


SPECS = {'PolyGate': polygate}

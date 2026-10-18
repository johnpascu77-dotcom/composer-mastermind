"""Specs for SeedRandomizer, MorphQuantizer and ChordHarmonizer (same method as specs_new.polygate)."""
import re

from guide_lib import parse_params, describe_param, strings
from specs_new import REPOS, read, add_calls, lit


def string_array(source, name):
    m = re.search(r'%s\s*\[[^\]]*\]\s*=\s*\{(.*?)\};' % re.escape(name), source, re.S)
    return strings(m.group(1)) if m else []


def global_controls(editor, params, stop_marker):
    """Controls added by addCombo / addSlider / addToggle calls before `stop_marker` (the per-band rows start there)."""
    head = editor.split(stop_marker)[0]
    out = []
    for kind, args in add_calls(head, ('addCombo', 'addSlider', 'addToggle')):
        pid = lit(args[0])
        label = lit(args[1])
        tip = lit(args[2]) if len(args) > 2 else None
        if pid is None or not label or tip is None or re.match(r'.*_(bar|val|curve)$', pid):
            continue
        choices, default = describe_param(params.get(pid))
        out.append({'name': label, 'id': pid, 'choices': choices, 'default': default, 'desc': tip})
    return out


def band_controls(editor, params):
    ids = string_array(editor, 'rowIds')
    names = string_array(editor, 'rowNames')
    tips = string_array(editor, 'rowTips')
    out = []
    for i, pid in enumerate(ids):
        if not pid or i >= len(names) or i >= len(tips):
            continue
        choices, default = describe_param(params.get(pid))
        out.append({'name': names[i], 'id': 'b1_ ... b4_' + pid, 'choices': choices, 'default': default, 'desc': tips[i]})
    return out


# ---------------------------------------------------------------------------------------------------- SeedRandomizer
def seedrandomizer():
    root = REPOS + '/SeedRandomizer'
    params = parse_params(read(root + '/Source/SeedRandomizerProcessor.cpp'))
    editor = read(root + '/Source/SeedRandomizerEditor.cpp')
    return {
        'name': 'SeedRandomizer', 'crumbs': 'Ecosystem → MIDI effects → <b>SeedRandomizer</b>',
        'role': 'Randomize that can be replayed: the same notes always get the same randomization.',
        'badges': [('chain', 'MIDI effect'), ('live', 'VST3 · Standalone'), ('plugin-code', 'seeded · replayable')],
        'lede': 'Like Bitwig’s Randomize, but every random value is a **hash of (seed, which note it is, which parameter)**, never a running random sequence. A note keeps its random values whatever happens to the notes around it (PolyGate drops one, MorphQuantizer moves one), and the result does not depend on block size or tempo. Replay a section and it comes out the same.',
        'chain': ['MidiSampler / source', 'PolyGate', ('SeedRandomizer', True), 'MorphQuantizer', 'instrument'],
        'sections': [
            {'title': 'The Editor', 'note': 'A scrolling page: the seeds and global settings first, then four register bands side by side in one table, and a histogram of the pitch shifts so far. Hover any control for a reminder.',
             'image': 'assets/seedrandomizer-editor.png', 'caption': 'The editor, rendered by SeedRandomizerProcessorCheck.', 'controls': []},
            {'title': 'Seeds, Evolve and key mode', 'note': '**Seed A / Seed B** are saved with the project and automatable; the **Reroll** buttons pick a new one. **Evolve** crossfades between the two draws note by note, so a variation can change slowly and stay replayable.',
             'controls': global_controls(editor, params, 'rerollA.onClick')},
            {'title': 'Register bands', 'note': 'Up to four bands split by pitch; **every band has its own complete set of settings**. The rows below exist once per band (Band 1 to Band 4). What a band is allowed to do to a note: pitch (a range, not a value), velocity, length, delay, how many notes are affected, how many sound at all, and which MIDI channel the notes of the band leave on.',
             'controls': band_controls(editor, params),
             'notes': ['A range is not a value: **+/-3** allows any whole semitone from -3 to +3, **0 included**, each equally likely in the Uniform shape. Use the **Steps** shape for exact values such as -3 / 0 / +3 only.',
                       'Note-offs follow the shifted pitch, so nothing hangs. Transport stop, a jump back and all-notes-off end every sounding note.']},
            {'title': 'Proof tools', 'notes': [
                '`python Tools/seedrand_tools.py take.mid --apply out.mid --seed-a 7 --pitch 3 --vel 10 --check` applies the same engine to a file and checks that a second run is identical and that dropping 25 % of the input notes changes **none** of the remaining notes (on a 512-note take: 383 notes compared, 0 changed).',
                '`SeedRandomizerCheck` covers the range semantics, block-size independence, stability when other notes are removed or added, evolve endpoints, key modes, constraints and a 600-run fuzz. `SeedRandomizerProcessorCheck out.png` renders this editor without a host.']},
        ],
        'source': 'Source: <span class="mono">Source/SeedRandomizerEditor.cpp</span> (the tooltips) and <span class="mono">SeedRandomizerProcessor.cpp</span> (the parameters). Design notes: <span class="mono">Docs/scoping.md</span>.',
        'out': root + '/Docs/SeedRandomizer_Guide.html',
    }


# ---------------------------------------------------------------------------------------------------- MorphQuantizer
def morphquantizer():
    root = REPOS + '/MorphQuantizer'
    params = parse_params(read(root + '/Source/MorphQuantProcessor.cpp'))
    editor = read(root + '/Source/MorphQuantEditor.cpp')
    scenes = [
        {'name': 'Keyframe bar', 'id': 'sc1_bar ... sc8_bar', 'desc': 'The bar where this keyframe’s Morph value is reached.'},
        {'name': 'Keyframe Morph value', 'id': 'sc1_val ... sc8_val', 'desc': 'The Morph value at that bar (0 = grid A, 100 % = grid B).'},
        {'name': 'Keyframe curve', 'id': 'sc1_curve ... sc8_curve', 'desc': 'How Morph travels to the next keyframe: Linear, Ease (smooth), or Step (hold, jump at the next keyframe).'},
    ]
    return {
        'name': 'MorphQuantizer', 'crumbs': 'Ecosystem → MIDI effects → <b>MorphQuantizer</b>',
        'role': 'A quantizer with two grids per register band that you morph between: binary to ternary, over time.',
        'badges': [('chain', 'MIDI effect'), ('live', 'VST3 · Standalone'), ('plugin-code', 'lookahead')],
        'lede': 'Register bands (1 to 4), **two grids per band (A and B) that you can morph between**, and lookahead. Draw Morph as an automation lane, let the **Scenes** keyframes drive it from the transport, or let the plug-in **listen** and follow the feel of what is played. Chords can stay together, a rolled chord can stay rolled, and a deliberately off-beat note can survive (Tolerance).',
        'chain': ['MidiSampler / source', 'PolyGate', 'SeedRandomizer', ('MorphQuantizer', True), 'instrument'],
        'sections': [
            {'title': 'The Editor', 'note': 'A scrolling page: the global controls, the Morph scenes (a timeline picture with eight keyframes), then the bands side by side, each with a small curve of where a note played at x within a beat lands: **blue = grid A, orange = grid B, white = what the band does now**.',
             'image': 'assets/morphquantizer-editor.png', 'caption': 'The editor, rendered by MorphQuantProcessorCheck.', 'controls': []},
            {'title': 'Morph, snap and chords', 'note': 'The controls that apply to every band. The line under them tells whether the current settings need a lookahead (the whole output is then that many beats late, like PolyGate) or only move notes later.',
             'controls': global_controls(editor, params, 'morph scenes: 8 keyframes in columns')},
            {'title': 'Morph scenes', 'note': 'With **Morph source = Scenes**, up to eight keyframes drive Morph from the transport position: “A until bar 9, morph to B by bar 17, hold” needs no automation lane. The timeline picture shows the curve and the playhead; the bar length follows the host time signature; without a playing transport the slider rules.',
             'controls': scenes},
            {'title': 'Register bands', 'note': 'These rows exist once per band (Band 1 to Band 4). Each band has its own grid A and grid B, swing, offset, strength and tolerance, and its own part of the global Morph during which it morphs, so the bass can finish before the melody starts.',
             'controls': band_controls(editor, params),
             'notes': ['**Pattern grids.** A grid can be a custom pattern instead of equal cells: 3+3+2, 3+2+3, 2+3+3, 3+2+2, 2+3+2, 2+2+3, 3+2, 2+3, 4+3, 3+4, 2+2+2+3, 3+3+3+2+2 (cell lengths in units of 1/(Grid per beat); with Grid 2, 3+2+2 puts points at 0, 1.5 and 2.5 beats and repeats every 3.5 beats).']},
            {'title': 'Three ways to drive the morph', 'notes': [
                '**Manual:** the Morph slider or host automation.  **Scenes:** the keyframes above.',
                '**Auto grid choice** (Auto grid amount above 0): the plug-in listens to what is played and moves each band toward the grid, A or B, the notes fit better (it measures how close the played notes sit to each grid relative to that grid’s own spacing; notes that sit on both grids give no evidence and change nothing). Set A = the binary grid and B = the ternary grid and the plug-in follows the music from a binary section to a ternary one by itself.']},
            {'title': 'Offline tool', 'notes': ['`python Tools/quantmorph_tools.py take.mid --apply out.mid --bands 2 --split 60 90 120 --band "1: a=2 b=3 start=0.5 width=0.5" --morph-ramp 17:33` runs the same engine with no latency left in the output and prints, per band and per stretch of bars, the share of note starts on the straight grid, on the triplet grid and on neither. `--scenes "1:0 9:0 25:1"` and `--auto-amount 1` work too.']},
        ],
        'source': 'Source: <span class="mono">Source/MorphQuantEditor.cpp</span> (the tooltips) and <span class="mono">MorphQuantProcessor.cpp</span> (the parameters). Design notes: <span class="mono">Docs/scoping.md</span>.',
        'out': root + '/Docs/MorphQuantizer_Guide.html',
    }


# --------------------------------------------------------------------------------------------------- ChordHarmonizer
def chordharmonizer():
    root = REPOS + '/ChordHarmonizer'
    params = parse_params(read(root + '/Source/ChordHarmonizerProcessor.cpp'))
    editor = read(root + '/Source/ChordHarmonizerEditor.cpp')
    controls = []
    for kind, args in add_calls(editor, ('addRow', 'addPick')):
        if len(args) < 4 or lit(args[1]) is None or lit(args[2]) is None:
            continue
        pid = lit(args[2])
        tip = lit(args[3])
        if tip is None or pid.startswith(('w_', 't_', 'c_')):
            continue
        choices, default = describe_param(params.get(pid))
        controls.append({'name': lit(args[1]), 'id': pid, 'choices': choices, 'default': default, 'desc': tip})
    controls += [
        {'name': 'Major 7 / Dominant 7 / Minor 7', 'id': 'qMaj · qDom · qMin', 'desc': 'Which chord qualities of the pool may be used (voicings and their functions).'},
        {'name': 'Load pool folder...', 'desc': 'Choose your Sick Chords folder (the one holding Individual Chords (MIDI)). The voicings are kept inside the plug-in state; the licensed files are never copied. Until a pool is loaded, notes pass through unchanged.'},
        {'name': 'Function chances', 'id': 'w_0 ... w_n', 'desc': 'Random mode: a chance for every function (root, 3rd, b9th, 13th ...). 0 = never; the chances are relative to each other.'},
        {'name': 'Table: one function per pitch class', 'id': 't_0 ... t_11', 'desc': 'Table mode: the function every C, C#, D ... of the melody gets. Leave alone keeps the plain note.'},
        {'name': 'Cycle: functions in turn', 'id': 'c_0 ... c_7', 'desc': 'Cycle mode: the function of each of up to eight steps. Rest leaves that note alone. Only the first Cycle length steps are used.'},
    ]
    return {
        'name': 'ChordHarmonizer', 'crumbs': 'Ecosystem → MIDI effects → <b>ChordHarmonizer</b>',
        'role': 'Harmonizes a melody with voicings from the Sick Chords pool: every note becomes a chord with the note on top.',
        'badges': [('chain', 'MIDI effect'), ('live', 'VST3 · Standalone'), ('plugin-code', 'zero latency')],
        'lede': 'Every melody note is given a **function** (root, 3rd, b9th, 13th ...), a voicing of that function (top voice = the function) is picked from the pool and shifted so its top note is the melody note; the chord replaces the note with the same start and length. The result is a “choral” block-chord line, ready for the musical delay, arpeggiators or a divisi patch. The pool is licensed content (Jeff Schneider’s *Sick Chords Vol. 1.1*): it is read from your folder and never copied into the repository.',
        'chain': ['melody', ('ChordHarmonizer', True), 'SeedRandomizer / delay', 'instrument'],
        'sections': [
            {'title': 'The Editor', 'note': 'The left column holds the mode and its options; on the right are the chances of each function (random mode); below them the table and cycle settings. The controls of the modes that are not selected are greyed out.',
             'image': 'assets/chordharmonizer-editor.png', 'caption': 'The editor, rendered by ChordHarmonizerProcessorCheck.', 'controls': []},
            {'title': 'Controls', 'note': 'Hover any control for a reminder.', 'controls': controls},
            {'title': 'Three ways to choose the function', 'notes': [
                '**Random:** a chance slider per function (0 = never). The function is picked first (so functions with many voicings are not favoured), then the quality, then the voicing, all keyed by the seed and the note’s own position + pitch, so removing other notes never changes a chord.',
                '**Table:** one function per pitch class of the melody; “leave alone” keeps the plain note.  **Cycle:** up to eight functions in turn; “rest” leaves a note alone.']},
            {'title': 'Voice leading and divisi', 'notes': [
                '**Voice leading** chooses voicings by how little they move from the previous chord (only the lower voices; the top voice is the melody and the bass move counts double). It depends on the previous chord, so unlike the seeded choice it is not stable under removing earlier notes, and it restarts after a transport stop or jump.',
                '**Voice to channel** sends every voice of a chord on its own MIDI channel (Top first: melody on channel 1; Bottom first: bass on channel 1). The channels survive SeedRandomizer, PolyGate and the musical delay, so in Bitwig an Instrument Layer with one layer per voice (a Channel Filter note effect, then MpeBend, then a divisi patch) plays a section as divided players.']},
            {'title': 'Offline tool', 'notes': ['`python chord_harmonizer.py melody.mid out.mid --pool "<Sick Chords folder>" --seed 7 --report` runs the same engine on a file (`--mode table|cycle`, `--voice-leading voicing|free`, `--voice-channel top|bottom`). `python Tools/crosscheck.py "<pool folder>"` proves the plug-in and the script produce exactly the same chords.']},
        ],
        'source': 'Source: <span class="mono">Source/ChordHarmonizerEditor.cpp</span> (the tooltips) and <span class="mono">ChordHarmonizerProcessor.cpp</span> (the parameters). See also <span class="mono">README.md</span>.',
        'out': root + '/Docs/ChordHarmonizer_Guide.html',
    }


SPECS = {'SeedRandomizer': seedrandomizer, 'MorphQuantizer': morphquantizer, 'ChordHarmonizer': chordharmonizer}

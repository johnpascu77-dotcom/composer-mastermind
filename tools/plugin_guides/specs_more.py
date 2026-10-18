"""Specs for MpeBend and MidiSampler."""
import re

from guide_lib import parse_params, describe_param, strings
from specs_new import REPOS, read


# --------------------------------------------------------------------------------------------------------- MpeBend
def mpebend():
    root = REPOS + '/MpeBend'
    params = parse_params(read(root + '/Source/MpeBendProcessor.cpp'))
    editor = read(root + '/Source/MpeBendEditor.cpp')
    tips = dict(re.findall(r'\{\s*"(\w+)",\s*"((?:[^"\\]|\\.)*)"\s*\}', editor.split('juce::String tipFor')[1].split('sectionTip')[0]))
    sec_tips = {}
    for m in re.finditer(r'title\.startsWith \("([A-Z ]+)"\)\) return "((?:[^"\\]|\\.)*)"', editor):
        sec_tips[m.group(1)] = m.group(2)
    plan_text = editor.split('const std::vector<Sec> plan')[1].split('for (const auto& sec : plan)')[0]
    sections, current = [], None
    for m in re.finditer(r'\{\s*"([A-Z][A-Z ]+[^"]*)",\s*\{|\{\s*"(\w+)",\s*"([^"]+)",\s*(true|false),\s*(true|false)\s*\}', plan_text):
        if m.group(1):
            title = m.group(1)
            note = next((v for k, v in sec_tips.items() if title.startswith(k)), '')
            current = {'title': title.split('(')[0].strip().title() if title.isupper() else title, 'raw': title, 'note': note, 'controls': []}
            sections.append(current)
        else:
            pid, label = m.group(2), m.group(3)
            p = params.get(pid)
            choices, default = describe_param(p)
            current['controls'].append({'name': label, 'id': pid, 'choices': choices, 'default': default, 'desc': tips.get(pid, '')})
    out_sections = [
        {'title': 'The Editor', 'note': 'The top rows pick a family preset, your own presets and the band; below that everything scrolls in sections, one per kind of gesture. Hover any control for a reminder. Use the **CLAP** version in Bitwig: VST3 effects cannot send pitch bend there.',
         'image': 'assets/mpebend-editor.png', 'caption': 'The editor, rendered by MpeBendProcessorCheck.', 'controls': []},
        {'title': 'Presets, bands and the top row', 'controls': [
            {'name': 'Preset', 'desc': 'Family presets (Strings, Brass, Woodwinds, Choir, FX) and your own. Choosing one sets the selected band’s parameters; it never changes the seed, the instrument bend range, the channels or the setup switch.'},
            {'name': 'Band 1 … Band 4', 'desc': 'Which register band you are editing. Every band has its own complete set of parameters; the seed, the instrument range, the member channels and the group window are shared.'},
            {'name': 'Save… / Delete / Copy to all bands', 'desc': 'Save the selected band’s settings, the instrument bend range and the seed as your own preset; delete the shown preset; copy the selected band to the other bands.'},
            {'name': 'Send MPE setup messages', 'id': 'setup', 'desc': 'At start, send the MPE configuration (RPN 6) and the bend range (RPN 0) to the instrument, so it does not have to be set up by hand.'}]},
    ]
    for s in sections:
        out_sections.append({'title': s['title'], 'note': s['note'], 'controls': s['controls']})
    out_sections.append({'title': 'How it works', 'notes': [
        'Every note is put on its own MPE member channel (2..16) and gets its own pitch-bend life. All values are drawn from the **seed** and the note’s own start and pitch (hash-keyed), so a replay is identical and independent of block size. A channel’s bend is reset just before the next note starts on it, never at note-off, so release tails do not snap.',
        'Set **Instrument bend range** to the range the instrument uses (Opus: its Pitchbend Range; SWAM MPE default 48); the sum of all layers is clamped to it.',
        'Put it **last in the chain**, directly before the instrument. With an automatic cluster-morph anchor the plug-in holds every note back by the group window (default 1/16 beat), so everything sounds about 30 ms later at 120 bpm.',
        'Offline: `python Tools/mpebend_tools.py in.mid out.mid …` runs the same engine and can write a notation file (a glissando as two notes) and a text report; `Tools/run_mpebend_tools.bat` opens a window for it. `calibration/` holds ear-test files to find how far each patch can bend.']})
    return {
        'name': 'MpeBend', 'crumbs': 'Ecosystem → MIDI effects → <b>MpeBend</b>',
        'role': 'Gives every note its own MPE channel and its own pitch-bend life: detune, drift, vibrato, trills, glides, scoops, sweeps, cluster morphs.',
        'badges': [('chain', 'MIDI effect · MPE'), ('live', 'CLAP (Bitwig) · VST3 · Standalone'), ('plugin-code', 'last in chain')],
        'lede': 'For MPE instruments (SWAM, Opus Hollywood Strings …): per note, a fixed microtonal **offset**, a slow **drift**, a real **vibrato** in Hz, a **trill**, a **glide** from the previous note, **scoops** and **falls**, long **sweeps** and **cluster morphs** from one note to a chord (or back), with **ensemble imperfection** so the voices never arrive together. Everything is seeded and replayable.',
        'chain': ['…effects', ('MpeBend', True), 'MPE instrument'],
        'sections': out_sections,
        'source': 'Source: <span class="mono">Source/MpeBendEditor.cpp</span> (the tooltips) and <span class="mono">MpeBendProcessor.cpp</span> (the parameters). See also <span class="mono">README.md</span>.',
        'out': root + '/Docs/MpeBend_Guide.html',
    }


# ------------------------------------------------------------------------------------------------------- MidiSampler
def midisampler():
    root = REPOS + '/MidiSampler'
    editor = read(root + '/Source/MidiSamplerEditor.cpp')
    label_text = dict(re.findall(r'setupLabel \((\w+), "((?:[^"\\]|\\.)*)"\)', editor))
    tips = {}
    for m in re.finditer(r'(\w+)\.setTooltip \(((?:\s*"(?:[^"\\]|\\.)*")+)\s*\)', editor):
        tips[m.group(1)] = ''.join(strings(m.group(2)))
    for m in re.finditer(r'tip \(&(\w+), (?:&(\w+)|nullptr), "((?:[^"\\]|\\.)*)"\)', editor):
        tips[m.group(1)] = m.group(3)

    def c(var, name=None, extra=''):
        text = tips.get(var, '')
        return {'name': name or label_text.get(var, var), 'desc': (text + ('. ' + extra if extra else '')).replace('..', '.')}

    sections = [
        {'title': 'The Editor', 'note': 'Top bar: file and setup buttons and Rec. Under it the **keyboard strip** (every zone is a bar), the **note display** of the loaded file with the play window, loop region and slices, and the **settings in columns** with three tabs on the right (Transform, Time, Cascade). Hover any control for a reminder.',
         'image': 'assets/midisampler-editor.png', 'caption': 'The editor, rendered by MidiSamplerSetupCheck.', 'controls': []},
        {'title': 'Top bar', 'controls': [
            c('loadButton', 'Load MIDI...'), c('clearButton', 'Clear'), c('panicButton', 'Stop'),
            c('recButton', 'Rec'), c('saveSetupButton', 'Save Setup...'), c('loadSetupButton', 'Load Setup...'),
            c('addZoneButton', '+ Zone'), c('removeZoneButton', '- Zone'), c('octaveButton', 'Octaves'),
            c('captureChip', 'Capture chip'), c('strip', 'Keyboard strip'), c('display', 'Note display')]},
        {'title': 'Global settings (every zone)', 'controls': [c('speedLabel'), c('syncToggle', 'Sync to host tempo'), c('tempoLabel'), c('voicesLabel'), c('stopKeyLabel')]},
        {'title': 'Zone: what it plays', 'note': 'A zone is a keyboard region (Key low … Key high) with a source file from the pool (8 slots) and its own window, loop, sustain mode, trigger behaviour, root key and slice setup. A key inside several zones plays all of them; a key inside no zone passes through untouched. All zone settings are per-zone host parameters (`z1_start`, `z2_mode` …) and stay automatable.',
         'controls': [c('playModeLabel'), c('sourceLabel'), c('keyLoLabel'), c('keyHiLabel'), c('modeLabel'), c('gateLabel'), c('rootLabel'), c('startLabel'), c('endLabel'), c('loopStartLabel'), c('loopEndLabel')]},
        {'title': 'Slices', 'note': 'In Slice mode the Start-End range is cut into slices, one per key from the first slice key. A slice plays at its original pitch. Up to 64 slices. **Slice zone** mode plays one chosen slice and the zone’s keys transpose it (the zone’s own first key plays it unchanged).',
         'controls': [c('sliceByLabel'), c('sliceGridLabel'), c('sliceCountLabel'), c('sliceMinLabel'), c('slicePlayLabel'), c('sliceKeyLabel'), c('sliceIdxLabel'), c('shiftLabel'), c('manualHint', 'Manual slicing')]},
        {'title': 'Transform tab', 'note': 'Per zone. Output pitches are folded into 0-127 by octaves (pitch class preserved) rather than clamped.',
         'controls': [c('outChanLabel'), c('ratioLabel'), c('invertToggle', 'Invert pitches'), c('axisLabel'), c('keyMapLabel'), c('stepLabel'), c('pcLabel'), c('chanceLabel'), c('chanceSeedLabel')]},
        {'title': 'Time tab', 'note': 'Inspired by Fugue Machine. All neutral by default, so an older setup sounds as before. The tab reads “Time *” when a zone uses any of it.',
         'controls': [c('delayLabel'), c('phaseLabel'), c('wcurveLabel'), c('wdepthLabel'), c('wcycleLabel'), c('wphaseLabel')]},
        {'title': 'Cascade tab', 'note': 'Stage 1 zones play what the host sends; stage 2 zones play the notes stage 1 emits (after slicing, transposition, invert and key map), stage 3 those of stage 2. The last stage is what you hear. Every stage has an **input conditioner** in front of it (order inside it: range, pass, snap, grid). Voice limits are per stage; the Stop key releases every stage.',
         'controls': [c('cascadeToggle', 'Cascade on'), c('zoneStageLabel'), c('editStageLabel'), c('handoverLabel'), c('passLabel'), c('seedLabel'), c('gridLabel'), c('rangeLoLabel', 'Range low'), c('rangeHiLabel', 'Range high'), c('snapToggle', 'Snap to pitch set'), c('tapToggle', 'Tap to output'), c('stageInfo', 'Voices now')],
         'notes': ['**Your first cascade (ten minutes)** is in `Docs/cascade_first_steps.md`: one key makes a slice loop; every time it starts it also triggers a second zone that plays its own slice transposed by the pitch of the trigger.']},
        {'title': 'Rec: capture what MidiSampler plays', 'notes': [
            '**Rec** records the notes MidiSampler sends out (after zones, transforms and channel routing; notes passed through from unmapped keys too). When you stop it writes `Documents\\MidiSampler Captures\\Capture <date time>.mid` and shows a green **chip**: drag it into your DAW, or double-click it to reveal the file.',
            'Events are stored in host beats (not scaled by Speed), so the clip plays back at the host tempo. **Beat 0 of the file is the start of the bar in which you pressed Rec** whenever the host reports its bar position; drop the clip at that bar and everything lines up on the grid. Tempo and 4/4 are written into the file.']},
        {'title': 'Setups and checks', 'notes': [
            '**Save Setup…** writes one readable `.json` with every parameter, the manual slice boundaries, the selected zone and the loaded MIDI files (notes embedded, so the file is self-contained). Hand-editing is fine; settings a newer version does not know are ignored and reported.',
            'Keys are named with middle C = C3 (key 24 = C0). Verification tools: `MidiSamplerEngineCheck`, `MidiSamplerCascadeCheck`, `MidiSamplerCaptureCheck`, `MidiSamplerSetupCheck` (which can also render this editor to a PNG).']},
    ]
    return {
        'name': 'MidiSampler', 'crumbs': 'Ecosystem → MIDI effects → <b>MidiSampler</b>',
        'role': 'A sampler that plays MIDI instead of audio: load a .mid file, play keys, get transposed, sliced, delayed, layered sequences.',
        'badges': [('chain', 'MIDI effect'), ('live', 'VST3 · Standalone'), ('plugin-code', 'Simpler-MIDI style')],
        'lede': 'Concept: Simpler MIDI for Ableton, rebuilt as a host-agnostic MIDI effect. Load a `.mid` file; every key you play replays the sequence transposed by (key − root), or one slice of it, or a loop, at its own speed and entry delay, optionally through a **cascade** where one stage’s notes trigger the next. With nothing loaded, notes pass straight through; non-note MIDI always passes.',
        'chain': ['host keys / clip', ('MidiSampler', True), 'PolyGate', 'SeedRandomizer / MorphQuantizer', 'instrument'],
        'sections': sections,
        'source': 'Source: <span class="mono">Source/MidiSamplerEditor.cpp</span> (the tooltips). Longer notes: <span class="mono">Docs/README.md</span>, <span class="mono">Docs/cascade_first_steps.md</span>.',
        'out': root + '/Docs/MidiSampler_Guide.html',
    }


SPECS = {'MpeBend': mpebend, 'MidiSampler': midisampler}

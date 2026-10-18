"""Spec for the Atonal Motive Engine (AME): the control texts are read from host/common/ui/src/UiModel.cpp
(parameterName, parameterTooltip, operationName, operationBlurb), the same strings the editor shows as tooltips."""
import re

from specs_new import read

AME = 'C:/atonal-motive-engine'


def ui_model():
    src = read(AME + '/host/common/ui/src/UiModel.cpp')
    names = {}
    body = src.split('const char *parameterName')[1].split('ControlKind controlKind')[0]
    tip_body = body.split('std::string parameterTooltip')[1] if 'std::string parameterTooltip' in body else ''
    name_body = body.split('std::string parameterTooltip')[0]
    for m in re.finditer(r'case (\d+): return "((?:[^"\\]|\\.)*)";', name_body):
        names[int(m.group(1))] = m.group(2)
    tips = {}
    for m in re.finditer(r'case (\d+): return "((?:[^"\\]|\\.)*)";', tip_body):
        tips[int(m.group(1))] = m.group(2).replace('\\"', '"')
    ops = re.findall(r'"([A-Z ]+)"', src.split('const char *operationName')[1].split('operationBlurb')[0])
    blurbs = [m.group(2) for m in re.finditer(r'case (\d+): return "((?:[^"\\]|\\.)*)";', src.split('const char *operationBlurb')[1])]
    return names, tips, ops, blurbs


def ctl(names, tips, pid, name=None):
    return {'name': name or names.get(pid, str(pid)), 'id': str(pid), 'desc': tips.get(pid, '')}


def ame():
    names, tips, ops, blurbs = ui_model()
    operations = [{'name': ops[i], 'id': 'operation %d' % i, 'desc': blurbs[i] if i < len(blurbs) else ''} for i in range(min(len(ops), 14))]
    return {
        'name': 'Atonal Motive Engine', 'crumbs': 'Ecosystem → Motive tools → <b>AME</b>',
        'role': 'Captures a motive and transforms it through a chain of up to four operations: static versions to drag out, or live material to launch.',
        'badges': [('chain', 'CLAP · VST3 · motive transformer'), ('live', 'Live mode · companion'), ('plugin-code', 'AME')],
        'lede': 'Capture a short motive, then build a chain of up to four operations on it: **transpose, invert, retrograde, rotate, fragment, repeat, sequence, augment, diminish, interval expand and contract, delay, derive, ornament**. Hear the result on the host timeline, **launch** it by hand, by CC or from the **typing-keyboard companion** (Live mode), or **drag any stage out as a MIDI file** to build a pool of versions of one motive for a piece or for notation. The chain is steered from Composer Mastermind with ordinary CCs.',
        'chain': ['notes / clip', ('AME', True), 'instrument / Orch chain'],
        'sections': [
            {'title': 'The Editor', 'note': 'Left: the source and its capture. Right: the transform chain (four stage chips, the operations, the settings of the selected stage, Scope, Output, Tap). Bottom: the preview of source, pass and transform lanes, and the now-playing strip. Hover any control for a reminder.',
             'image': 'assets_guide/09_chain_four_stages.png', 'image_width': '980px', 'caption': 'A four-stage chain (rendered by the UI preview tool).', 'controls': []},
            {'title': 'Source and capture', 'note': 'AME works on a **motive**: a short run of notes captured from the incoming MIDI, cut into **cells** on a grid you choose. The cell length is fixed at capture time (change it, then Recapture); cells are what Fragment, Rotate and the CELL scope work on.',
             'controls': [ctl(names, tips, i) for i in (1, 3, 4, 5, 6, 12, 7, 10, 11, 8, 9)]},
            {'title': 'The chain', 'note': 'Up to four stages, each applied to the result of the one before. Click a stage chip to edit it, click its dot to switch it on or off. A stage chosen with an operation that has no meaning at the current Scope makes the chain INVALID until you change the scope or the stage.',
             'controls': operations},
            {'title': 'Operation settings', 'note': 'Only the settings of the selected operation are shown. Stages 2 to 4 have the same settings.',
             'controls': [ctl(names, tips, i) for i in (200, 201, 210, 211, 230, 240, 241, 242, 250, 251, 252, 310, 311, 320, 321, 322, 260, 270, 271, 272, 300)],
             'notes': ['**Fragment needs MOTIVE scope, and a range must exist.** If the preview shows INVALID, check that Scope is MOTIVE and that the end cell is not beyond the last cell of the captured motive (cells are fixed at capture; use a smaller cell length and Recapture for more of them).',
                       '**Delay in beats.** The old 1/4 and 1/2 bar stops fall between eighth notes in 7/8; the stops 1 to 8 BEATS count beats of the time signature (an eighth in 7/8) and always stay on the beat grid. The same beat-based cell lengths exist for capture.']},
            {'title': 'Scope, output and tap', 'note': 'These apply to the whole chain.',
             'controls': [ctl(names, tips, i) for i in (101, 102, 104, 103)],
             'notes': ['**Scope table.** TRANSPOSE: all scopes alike. INVERT with a fixed or user axis: all alike; first/last note axis: one axis per cell at CELL scope, unsupported at NOTE. RETROGRADE: reverses inside each cell at CELL scope, the whole timeline at MOTIVE, unsupported at NOTE. AUGMENT and DIMINISH: each note\'s length at NOTE scope, the whole timeline at MOTIVE, unsupported at CELL. ORNAMENT: all alike. ROTATE, FRAGMENT, REPEAT, SEQUENCE, DERIVE and DELAY: MOTIVE only.']},
            {'title': 'Presets and exporting versions', 'note': 'A chain preset is a named snapshot of the four stages and the Tap (it never touches capture, source, scope or the Output mode). The preset box at the top of the chain card holds the factory presets and yours.',
             'controls': [
                 {'name': 'Save / Delete', 'desc': 'Save the current chain as a user preset (type a name first), or delete the user preset that is shown.'},
                 {'name': 'Drag a Tap button out', 'desc': 'Press a Tap button (FINAL or STAGE 1-4) and drag: the result of that stage goes out as a .mid file with the tempo and meter, ready to drop on a Bitwig track or into Dorico\'s piano roll. A plain click still selects the Tap.'},
                 {'name': 'Right-click a Tap button', 'desc': 'Save that result, every stage, or the source motive to the export folder. The files are named from the preset or from the operations (AME_RETROGRADE_TRANSPOSE+3-st.mid). Whole bars of silence before the first note are left out.'},
                 {'name': 'Companion keys Q / Y / U', 'desc': 'The same saves from the typing-keyboard companion: Q the final result, Y the source motive, U every stage and the source.'}]},
            {'title': 'Live mode', 'image': 'assets_guide/13_now_playing.png', 'image_width': '980px', 'caption': 'The now-playing strip in the editor: launched entrances, the playhead, layers.', 'note': 'Switch **LIVE** on and AME silences its timeline: nothing sounds until you launch it. Everything launched starts on the next line of the **launch grid** and can be gated, latched into a repeating **layer**, paused or frozen. The typing keyboard is the instrument: the **AME Live companion** starts when you click LIVE.',
             'controls': [ctl(names, tips, 330, 'LIVE'), ctl(names, tips, 301, 'RETRIGGER'),
                          {'name': 'Operation keys', 'desc': 'T I R F A D E C S O V, the arrows (Rotate), / (Repeat), Tab (Delay) and Enter (the initial motive) make a chord of operations that launches on the grid. Hold Shift or Space to type values: Shift + A 2 T 3 = Augment x2 and Transpose +3 in one chain. Digits recall presets; a digit typed first in a build starts from that preset.'},
                          {'name': 'Grid keys G H J K L B', 'desc': 'The launch grid: triplet toggle, 1/2, 1/4, 1/8, 1/16, bar.'},
                          {'name': 'M (momentary) and Z (latch)', 'desc': 'M: a launch sounds only while its key is held. Z: a launch becomes a layer that relaunches itself every cycle until you press it again (Esc stops every layer).'},
                          {'name': 'Ctrl, = and Caps Lock', 'desc': 'Ctrl held: sustain (the sounding notes keep ringing). = held: fermata (the flow freezes, what it would have played is skipped). Caps Lock: a true pause: the music\'s clock stops and everything goes on later by the length of the pause. All three land on the launch grid.'},
                          {'name': 'N, P, X, W', 'desc': 'N cycles the Scope. P plays and stops Bitwig\'s transport, X stops it (through the Transport Companion script). W keeps the companion window on top.'},
                          {'name': 'GLOBAL KEYS', 'desc': 'The companion\'s checkbox (or Scroll Lock) sends the keys to it even when another window has the focus, so you can edit AME with the mouse while playing.'}],
             'notes': ['The companion shows a **now-playing panel**: one row per launched entrance with the note starts as ticks, the playhead, the layers, the scope and the number of cells of the captured motive, and a red warning when what you typed cannot sound (for example Fragment at CELL scope).']},
            {'title': 'Steering AME from Composer Mastermind', 'note': 'AME is controlled by ordinary MIDI CCs on its channel: 85 preset, 86 scan, 87 stages, 88 stage mask, 89 output tap, 90 retrigger on the bar line, 91 launch on a grid tick, 92-95 the operation of stage 1-4, 96 asserted preset, 97 Live mode, 9 launch lanes, 14 and 15 layers, 100 and 101 gates, NRPN 99/98/6/38 for values, 102 fermata, 103 sustain, 104 pause, 105 scope, 106 export. The full list with the exact encodings is in `docs/MASTERMIND_BRIDGE.md`.'},
        ],
        'source': 'Source: <span class="mono">host/common/ui/src/UiModel.cpp</span> (parameterTooltip, operationBlurb). Longer notes: <span class="mono">docs/LIVE_MODE.md</span>, <span class="mono">docs/MASTERMIND_BRIDGE.md</span>, <span class="mono">docs/TRANSFORMATION_SEMANTICS.md</span>.',
        'out': AME + '/docs/AME_Guide.html',
    }


SPECS = {'AME': ame}

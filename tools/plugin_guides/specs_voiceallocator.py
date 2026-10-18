"""Spec for VoiceAllocator (MIDI voice allocator). Control texts are read from the editor source, like the other specs; the player table is added by hand."""
from guide_lib import describe_param, parse_params
from specs_new import REPOS, add_calls, lit, read


def voiceallocator():
    root = REPOS + '/VoiceAllocator'
    params = parse_params(read(root + '/Source/VoiceAllocatorProcessor.cpp'))
    editor = read(root + '/Source/VoiceAllocatorEditor.cpp')
    sections, current = [], None
    for kind, args in add_calls(editor):
        if kind == 'addHeader':
            current = {'title': lit(args[0]), 'controls': [], 'notes': []}
            sections.append(current)
            continue
        pid, label, tip = lit(args[0]), lit(args[1]), lit(args[2])
        if pid is None or current is None:
            continue
        choices, default = describe_param(params.get(pid))
        current['controls'].append({'name': label, 'id': pid, 'choices': choices, 'default': default, 'desc': tip})
    by = {s['title']: s for s in sections}
    by['Allocation']['note'] = 'How the notes of one performance are shared out among the players.'
    by['Allocation']['notes'] = [
        '**What a player is:** one MIDI channel with a playable range, one instrument of a section. Players are sorted by register, so the order of the rows does not matter; the channels go downstream to the instruments (OrchNoteMapper presets, Bitwig channel filters, or one Opus / SWAM instance per channel).',
        '**How a chord is assigned:** the cheapest *non-crossing* assignment of the chord\'s notes to the free players (a dynamic program over the players sorted by register). The cost is the pitch movement from the player\'s last note, with a memory that fades out, plus octave folding into the player\'s range and a pull towards its own register. When there are more notes than free players, the top, the bottom and the loudest notes survive and the rest is dropped (counted as "dropped").',
        '**Modes:** Multi Poly: one note per player. Complete: every player of the event plays, so fewer notes than players are thickened by doubling (the extra players go to the upper notes). Multi Mono: every note on every player in range, no analysis. Bottom Up / Top Down: the chord notes fill the players in a fixed order.',
        '**Latency:** everything after the plug-in is delayed by the Lookahead (a MIDI plug-in cannot report latency to the host): record the result or shift the track. The part of the lookahead above the capture window is the legato reach.',
    ]
    by['Legato']['note'] = 'Legato-capable libraries expect a note-off AFTER the next note-on. Generators write clean cuts on the grid, so the allocator makes the overlap itself.'
    by['Legato']['notes'] = [
        'A note that continues a line stays on the same player and the previous note is made to ring into it by the Overlap. This also covers a sole held note that an overlapping note continues (a human legato). A note that follows within the legato reach (lookahead minus capture window) of the previous note-off counts as the continuation; a longer gap is not joined.',
        'A new voice in a chord is never mistaken for legato: only a sole held note, or a note that ended just before, is continued. The "legato joins" counter at the bottom shows how many overlaps were made.',
    ]
    by['Polyphony Lock']['note'] = 'Freeze the voicing, as a pedal: the notes that sound when the lock goes on keep sounding.'
    by['Polyphony Lock']['notes'] = [
        'Only the notes still held in the input at that moment are locked (a note that had already ended is not). Their players are taken out of the allocation, so new notes are played by the remaining free players and are dropped when there are none. Releasing the lock ends the held notes. A transport stop also releases everything.',
        'The default pedal is CC 66 (sostenuto: it holds exactly the notes that sound when pressed). The pedal message is consumed and not passed to the instrument. CC 113 and the Lock button do the same.',
    ]
    players = {'title': 'Players', 'note': 'Up to eight players, one per row.', 'controls': [
        {'name': 'Player n (on)', 'id': 'p1en ... p8en', 'choices': 'On · Off', 'default': 'players 1-5 on',
         'desc': 'A player is one MIDI channel with a playable range. The defaults are a string section: Violin 1 (channel 1, G2-E6), Violin 2 (channel 2, G2-C6), Viola (channel 3, C2-E5), Cello (channel 4, C1-E4) and a Bass (channel 5, E0-C3) in this editor\'s note names (C3 = middle C).'},
        {'name': 'Channel', 'id': 'p1ch ... p8ch', 'choices': 'Channel 1 to 16', 'default': 'Channel n',
         'desc': 'The MIDI channel this player sends on. Route it to its instrument downstream.'},
        {'name': 'Low key / High key', 'id': 'p1lo ... p8hi', 'choices': '0 to 127 (note names)', 'default': 'see above',
         'desc': 'The playable range of the player. A note outside it is moved by octaves into it when Fold into range is on; otherwise the player does not take it.'}]}
    idx = [s['title'] for s in sections].index('Polyphony Lock') + 1
    sections.insert(idx, players)
    by['Remote control (Composer Mastermind)']['note'] = 'Control CCs, so an arc in Composer Mastermind can steer the section.'
    by['Remote control (Composer Mastermind)']['notes'] = [
        '**CC 112** sets the Mode (0-127 split in five: Multi Poly, Complete, Multi Mono, Bottom Up, Top Down). **CC 113** the Polyphony Lock (64 and up = on). **CC 114** Legato (64 and up = on). **CC 115** the legato Overlap (0-127 split in four: 1/64, 1/32, 1/16, 1/8 beat). **CC 116** the Voice affinity (0-127 = 0-100 %).',
        'The parameter follows the CC (and the editor shows it), so the setting is saved with the project. The CCs are consumed. They avoid the numbers the ecosystem already uses (MPL 20-64, AME 85-106, the pitch field 110 / 111). In Composer Mastermind create a Modulator Target on the CC with the mode **Arc (continuous)** for amounts (affinity) or **Section (stepped)** for switches (mode, lock, legato).',
    ]
    sections.insert(0, {'title': 'The Editor', 'note': 'One column of settings, then the player table. Hover any control for a short reminder. The line at the bottom counts notes in, out, dropped, legato joins and notes sounding.',
                        'image': 'assets/voiceallocator-editor.png', 'caption': 'The editor, rendered by VoiceAllocatorProcessorCheck.', 'controls': []})
    sections.append({'title': 'Offline use', 'notes': [
        '`Tools/va_apply.py take.mid --apply out.mid` runs the same engine on a MIDI file and writes the notes on the players\' channels with the latency removed: the result lines up with the input and can be captured, edited or imported into Dorico per channel. `--preset quartet|quintet|violins`, `--players "1:55-100,2:55-96,..."`, `--mode`, `--no-legato`, `--no-fold`.',
        'Live use is the same engine with a lookahead; a downstream MorphQuantizer can guard the result.']})
    return {
        'name': 'VoiceAllocator', 'crumbs': 'Ecosystem → MIDI effects → <b>VoiceAllocator</b>',
        'role': 'A MIDI-only voice allocator: spreads a performance over the players of a section, with legato overlaps.',
        'badges': [('chain', 'MIDI effect'), ('live', 'VST3 · CLAP · Standalone'), ('plugin-code', 'players = channels')],
        'lede': 'The notes of one performance go in, and each note comes out on the MIDI channel of the **player** that should play it, like the voice allocation of an orchestral section instrument, but as plain MIDI that can be recorded, edited and notated. Chords are shared out with **no voice crossing** and good voice leading, a line stays on its player, **legato overlaps** are written for clean-cut generator output, and a **Polyphony Lock** pedal freezes the voicing.',
        'chain': ['keyboard / generator', 'PolyGate', ('VoiceAllocator', True), 'OrchNoteMapper', 'instruments'],
        'sections': sections,
        'source': 'Source: <span class="mono">Source/VoiceAllocatorEditor.cpp</span> (the tooltips) and <span class="mono">VoiceAllocatorProcessor.cpp</span> (the parameters); the allocation is in <span class="mono">VoiceAllocatorEngine.h</span>. Concept: <span class="mono">ComposerMastermind/docs/voice_allocator_concept.md</span>.',
        'out': root + '/Docs/VoiceAllocator_Guide.html',
    }


SPECS = {'VoiceAllocator': voiceallocator}

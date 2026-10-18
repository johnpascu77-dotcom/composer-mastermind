"""Spec for ChannelFan (MIDI channel router + controller fan-out). Control texts are read from the editor source, like the other specs."""
from guide_lib import describe_param, parse_params
from specs_new import REPOS, add_calls, lit, read


def channelfan():
    root = REPOS + '/ChannelFan'
    params = parse_params(read(root + '/Source/ChannelFanProcessor.cpp'))
    editor = read(root + '/Source/ChannelFanEditor.cpp')
    first = {'title': 'Routing', 'controls': [], 'notes': []}
    sections, current = [first], first
    for kind, args in add_calls(editor):
        if kind == 'addHeader':
            current = {'title': lit(args[0]), 'controls': [], 'notes': []}
            sections.append(current)
            continue
        pid, label, tip = lit(args[0]), lit(args[1]), lit(args[2])
        if pid is None:
            continue
        choices, default = describe_param(params.get(pid))
        current['controls'].append({'name': label, 'id': pid, 'choices': choices, 'default': default, 'desc': tip})
    first['note'] = 'Which messages are changed at all, and how the notes are re-channelled.'
    sections[-1]['controls'].append({
        'name': 'Target channels', 'id': 'targets', 'choices': '16 toggles', 'default': 'all on',
        'desc': 'The channels that receive the fanned messages. Click or drag to switch channels on and off; All and None switch every channel. The original message is replaced: it only still reaches its own channel if that channel is on here.'})
    sections[-1]['notes'] = [
        '**Default behaviour:** with nothing changed, the mod wheel (CC 1) arriving on any channel is copied to all 16 channels and notes pass untouched. Put ChannelFan before the instrument or rig in a track\'s MIDI chain.',
        '**Notes in One channel mode:** every message that is not fanned (pedal, bend, program change) follows the notes to the chosen channel, so a keyboard stuck on channel 1 behaves as if it had been set to that channel. In Key split mode and in Keep mode the other messages stay on the channel they arrived on.',
        '**Note-offs** always leave on the channel their note-on left on, even if a setting changed in between, and an all-notes-off (CC 123 or 120) releases the held notes on the channels they were really sent to.',
    ]
    sections.append({'title': 'In the ecosystem: what a fanned wheel does', 'notes': [
        '**CC 1 is safe.** None of the ecosystem plug-ins uses CC 1 as a control: MPL\'s External Control CCs are 20 to 64, AME\'s are 85 to 106 (plus 9, 14, 15), the Orch plug-ins\' are 106 and 107 and a few others. A fanned wheel passes through them and reaches the instrument, where it usually means dynamics or expression.',
        '**Other numbers are not.** Fanning a controller that one of those plug-ins uses as a control (say CC 106, or one of MPL\'s pattern CCs) would operate that control on every target channel at once. The Fan CC slots show the number, so check it against the CC tables before adding one.',
        '**Channel-specific plug-ins.** An MPL or AME that is set to listen to one channel only will ignore the copies on the other channels, which is the point: the copy on its own channel is the one it hears. A plug-in set to All channels hears the same wheel once per target channel, so switch off the target channels it does not need.',
        '**Where to place it.** Before the plug-ins that should see the wheel in the track\'s chain (a MIDI effect passes what it receives to the next device). Place it after a plug-in that generates notes only if the wheel should reach what that plug-in plays.',
    ]})
    sections.insert(0, {'title': 'The Editor', 'note': 'One column of settings. Hover any control for a short reminder. The line at the bottom counts messages in and out.',
                        'image': 'assets/channelfan-editor.png', 'caption': 'The editor, rendered by ChannelFanProcessorCheck.', 'controls': []})
    return {
        'name': 'ChannelFan', 'crumbs': 'Ecosystem → MIDI effects → <b>ChannelFan</b>',
        'role': 'Makes a one-channel keyboard speak on many channels: re-channels notes and fans the mod wheel out.',
        'badges': [('chain', 'MIDI effect'), ('live', 'VST3 · Standalone'), ('plugin-code', 'channel router')],
        'lede': 'A hardware keyboard sends its notes and its mod wheel on one MIDI channel, usually channel 1. **ChannelFan** moves the notes to the channel you choose (or cuts the keyboard into zones, each on its own channel) and **copies chosen controllers to many channels at once**, so the wheel reaches every instrument of a multi-channel rig such as MPL, AME or the Orch chain.',
        'chain': ['keyboard', ('ChannelFan', True), 'MPL / AME / Orch chain', 'instrument'],
        'sections': sections,
        'source': 'Source: <span class="mono">Source/ChannelFanEditor.cpp</span> (the tooltips) and <span class="mono">ChannelFanProcessor.cpp</span> (the parameters); the routing is in <span class="mono">ChannelFanEngine.h</span>.',
        'out': root + '/Docs/ChannelFan_Guide.html',
    }


SPECS = {'ChannelFan': channelfan}

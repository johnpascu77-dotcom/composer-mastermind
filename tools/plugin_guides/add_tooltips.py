"""Adds tooltips to a JUCE editor from the plug-in's own reference guide.

  python add_tooltips.py <repo dir> [--editor Source/XEditor.cpp] [--guide Docs/X_Guide.html] [--dry]

For every `xAttachment = std::make_unique<...Attachment> (apvts, "paramId", component)` in the editor it looks up the
guide's entry for that parameter id (the `c-id` spans), and writes `component.setTooltip ("...")` (and the same text
on the component's `...Label` when there is one) in one generated block, plus a juce::TooltipWindow member (without
one no tooltip ever shows). Controls it could not match are listed, to be done by hand.
"""
import html
import os
import re
import sys


def clean(text):
    text = re.sub(r'<[^>]+>', '', text)
    text = html.unescape(text)
    text = re.sub(r'\s+', ' ', text).strip()
    return text


def guide_entries(path):
    """{param id: (name, description)}: .ctrl blocks first, then table rows (td.pid), then the region blocks."""
    src = open(path, encoding='utf-8').read()
    out = {}
    for block in re.findall(r'<div class="ctrl">(.*?)</div>\s*</div>\s*(?=<div class="ctrl">|</div>)', src, re.S):
        name = re.search(r'class="c-name">(.*?)</span>', block, re.S)
        ids = re.search(r'class="c-id[^"]*">(.*?)</span>', block, re.S)
        desc = re.search(r'class="c-desc">(.*?)</div>', block, re.S)
        if not (ids and desc):
            continue
        text = clean(desc.group(1))
        for pid in re.findall(r'[A-Za-z_][A-Za-z0-9_]*', clean(ids.group(1))):
            out.setdefault(pid, (clean(name.group(1)) if name else pid, text))
    for row in re.findall(r'<tr>(.*?)</tr>', src, re.S):
        cells = re.findall(r'<td class="([a-z]+)">(.*?)</td>', row, re.S)
        pid_cells = [c for k, c in cells if k == 'pid']
        descs = [c for k, c in cells if k == 'desc']
        if not pid_cells or len(descs) < 2:
            continue
        for pid in re.findall(r'[A-Za-z_][A-Za-z0-9_]*', clean(pid_cells[0])):
            out.setdefault(pid, (clean(descs[0]), clean(descs[-1])))
    for block in re.findall(r'<div class="region">(.*?)</div></div>', src, re.S):
        name = re.search(r'class="r-name">(.*?)</div>', block, re.S)
        mem = re.search(r'class="r-members[^"]*">(.*?)</div>', block, re.S)
        desc = re.search(r'class="r-desc">(.*?)</div>', block, re.S)
        if not (mem and desc):
            continue
        title = clean(name.group(1)) if name else ''
        text = (title + ': ' if title else '') + clean(desc.group(1))
        for pid in re.findall(r'[A-Za-z_][A-Za-z0-9_]*', clean(mem.group(1))):
            out.setdefault(pid, (title or pid, text))
    return out


def md_entries(docs_dir):
    """{param id: (label, description)} from markdown table rows like | `id` | Label | default | description |."""
    out = {}
    if not os.path.isdir(docs_dir):
        return out
    for f in os.listdir(docs_dir):
        if not f.endswith('.md'):
            continue
        for line in open(os.path.join(docs_dir, f), encoding='utf-8', errors='replace'):
            m = re.match(r'\|\s*\*{0,2}`(\w+)`\*{0,2}\s*\|(.*)\|\s*$', line)
            if not m:
                continue
            cols = [c.strip() for c in m.group(2).split('|')]
            if len(cols) < 2 or not cols[-1] or len(cols[-1]) < 20:
                continue
            text = re.sub(r'[`*]', '', cols[-1])
            out.setdefault(m.group(1), (cols[0], text))
    return out


def cpp_string(text):
    return '"' + text.replace('\\', '\\\\').replace('"', '\\"') + '"'


def main():
    args = sys.argv[1:]
    dry = '--dry' in args
    args = [a for a in args if a != '--dry']
    if '--anchor' in args:
        k = args.index('--anchor')
        del args[k:k + 2]
    if '--extra' in args:
        k = args.index('--extra')
        del args[k:k + 2]
    repo = args[0]
    editor = guide = None
    for i, a in enumerate(args):
        if a == '--editor':
            editor = args[i + 1]
        if a == '--guide':
            guide = args[i + 1]
    if not editor:
        cands = [os.path.join(dp, f) for dp, _, fs in os.walk(os.path.join(repo, 'Source')) for f in fs if re.search(r'Editor\.cpp$', f)]
        editor = cands[0]
    else:
        editor = os.path.join(repo, editor)
    if not guide:
        docs = os.path.join(repo, 'Docs')
        cands = [os.path.join(docs, f) for f in os.listdir(docs) if f.endswith('_Guide.html')]
        guide = cands[0]
    else:
        guide = os.path.join(repo, guide)
    header = editor[:-4] + '.h'
    entries = guide_entries(guide)
    for k, v in md_entries(os.path.join(repo, 'Docs')).items():
        entries.setdefault(k, v)
    raw = open(editor, newline='', encoding='utf-8-sig').read()
    nl = '\r\n' if '\r\n' in raw else '\n'
    src = raw.replace('\r\n', '\n')
    hdr_raw = open(header, newline='', encoding='utf-8-sig').read()
    hdr = hdr_raw.replace('\r\n', '\n')
    pairs = [] if (ctor_pre := ('--ctor-end' in sys.argv)) else re.findall(r'Attachment\s*=\s*std::make_unique<[^>]*>\s*\(\s*[^,]+,\s*"(\w+)"\s*,\s*\*?(\w+)\s*\)', src)
    lines, missing, seen = [], [], set()
    for pid, comp in pairs:
        if comp in seen:
            continue
        seen.add(comp)
        entry = entries.get(pid)
        if not entry:
            missing.append((pid, comp))
            continue
        text = entry[1]
        lines.append('    %s.setTooltip (%s);' % (comp, cpp_string(text)))
        base = re.sub(r'(Slider|Box|Button|Toggle|Combo)$', '', comp)
        for label in (base + 'Label',):
            if re.search(r'\b%s\b' % label, hdr):
                lines.append('    %s.setTooltip (%s);' % (label, cpp_string(text)))
    extra = None
    for i, a in enumerate(sys.argv):
        if a == '--extra':
            import json
            extra = json.load(open(sys.argv[i + 1], encoding='utf-8'))
    if extra:
        lines.append('    // written by hand (controls the guide has no row for, labels, read-outs)')
        for comp, text in extra.items():
            lines.append('    %s.setTooltip (%s);' % (comp, cpp_string(text)))
            base = re.sub(r'(Slider|Box|Button|Toggle|Combo)$', '', comp)
            if base != comp and re.search(r'%sLabel' % base, hdr) and (base + 'Label') not in extra:
                lines.append('    %sLabel.setTooltip (%s);' % (base, cpp_string(text)))
    if '--dry' in sys.argv or not lines:
        print('would add %d lines; unmatched: %s' % (len(lines), missing))
        return
    block = ('\n    // Tooltips (text from Docs/%s): hovering a control or its label shows what it does.\n' % os.path.basename(guide)
             + '\n'.join(lines) + '\n')
    anchor = sys.argv[sys.argv.index('--anchor') + 1] if '--anchor' in sys.argv else None
    marker = (re.search(r'\n[ \t]*' + re.escape(anchor), src) if anchor
              else re.search(r'\n[ \t]*\w+Attachment\s*=\s*std::make_unique<', src))
    if 'Tooltips (text from' in src:
        print('already has a generated tooltip block; remove it first')
        return
    src = src[:marker.start()] + block + src[marker.start():]
    # the tooltip window: a member of the editor class
    if 'TooltipWindow' not in hdr and '--no-window' not in sys.argv:
        m = re.search(r'(\n[ \t]*)(\w+AudioProcessor&\s+\w+;)', hdr)
        add = '\n    juce::TooltipWindow tooltipWindow { this, 700 };   // without one, no setTooltip() text would ever show\n'
        hdr = hdr[:m.end()] + add + hdr[m.end():]
    open(editor, 'w', newline='', encoding='utf-8').write(src.replace('\n', nl))
    open(header, 'w', newline='', encoding='utf-8').write(hdr.replace('\n', nl))
    print('%s: %d tooltip lines; unmatched: %s' % (os.path.basename(editor), len(lines), missing))


if __name__ == '__main__':
    main()

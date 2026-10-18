"""Renders a plug-in reference page in the look of the Orch System guides (OrchGate_Guide.html and friends).

A page is a plain dict (see the specs in specs_*.py): name, role, badges, crumbs, lede, chain, sections (each with a
title, a note and a list of controls: name / id / choices / desc / default), notes and a footer. The control texts are
extracted from the plug-in's own source (the tooltip strings and the parameter layout), so the manual and the tooltips
cannot drift apart: run the spec again after changing either.
"""
import html
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))


def esc(text):
    return html.escape(str(text), quote=False)


def para(text):
    """Escapes text but keeps a little inline markup: **bold** and `code`."""
    t = esc(text)
    t = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', t)
    t = re.sub(r'`(.+?)`', r'<span class="mono">\1</span>', t)
    return t


def split_top(text, sep=','):
    """Split on `sep` outside quotes, (), {}, [] and <>."""
    parts, depth, quote, cur = [], 0, None, ''
    i = 0
    while i < len(text):
        c = text[i]
        if quote:
            cur += c
            if c == '\\' and i + 1 < len(text):
                cur += text[i + 1]
                i += 1
            elif c == quote:
                quote = None
        elif c in '"\'':
            quote = c
            cur += c
        elif c in '({[':
            depth += 1
            cur += c
        elif c in ')}]':
            depth -= 1
            cur += c
        elif c == sep and depth == 0:
            parts.append(cur.strip())
            cur = ''
        else:
            cur += c
        i += 1
    if cur.strip():
        parts.append(cur.strip())
    return parts


def strings(text):
    return [s.replace('\\"', '"') for s in re.findall(r'"((?:[^"\\]|\\.)*)"', text)]


def parse_params(source):
    """Reads juce AudioParameter{Float,Int,Choice,Bool} declarations: {id: {name, kind, choices, min, max, default}}."""
    out = {}
    for m in re.finditer(r'AudioParameter(Float|Int|Choice|Bool)>?\s*\(', source):
        kind = m.group(1)
        i = m.end()
        depth, j = 1, i
        quote = None
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
        args = split_top(source[i:j - 1])
        if len(args) < 3:
            continue
        ids = strings(args[0])
        names = strings(args[1])
        if not ids:
            continue
        entry = {'kind': kind, 'name': names[-1] if names else ids[0]}
        try:
            if kind == 'Float':
                rng = re.search(r'NormalisableRange<float>\s*\(([^)]*)\)', args[2])
                nums = [x.strip().rstrip('f') for x in split_top(rng.group(1))] if rng else []
                entry['min'], entry['max'] = float(nums[0]), float(nums[1])
                entry['default'] = float(args[3].rstrip('f'))
            elif kind == 'Int':
                entry['min'], entry['max'], entry['default'] = int(args[2]), int(args[3]), int(args[4])
            elif kind == 'Choice':
                entry['choices'] = strings(args[2])
                entry['default'] = int(args[3])
            elif kind == 'Bool':
                entry['default'] = args[2].strip() == 'true'
        except (ValueError, IndexError, AttributeError):
            pass
        out[ids[0]] = entry
    return out


def fmt_number(v):
    return ('%g' % v)


def describe_param(p):
    """(choices text, default text) for a parsed parameter."""
    if not p:
        return '', ''
    kind = p.get('kind')
    if kind == 'Choice' and p.get('choices'):
        d = p.get('default')
        dflt = p['choices'][d] if isinstance(d, int) and 0 <= d < len(p['choices']) else ''
        return ' · '.join(p['choices']), ('default: %s' % dflt) if dflt else ''
    if kind in ('Float', 'Int') and 'min' in p:
        return '%s to %s' % (fmt_number(p['min']), fmt_number(p['max'])), ('default: %s' % fmt_number(p['default'])) if 'default' in p else ''
    if kind == 'Bool':
        return 'On · Off', 'default: %s' % ('On' if p.get('default') else 'Off')
    return '', ''


def control_html(n, c):
    choices = '<div class="c-choices">%s</div>' % esc(c['choices']) if c.get('choices') else ''
    default = '<div class="c-default">%s</div>' % esc(c['default']) if c.get('default') else ''
    cid = '<span class="c-id mono">%s</span>' % esc(c['id']) if c.get('id') else ''
    return ('      <div class="ctrl">\n        <div class="badge-num">%d</div>\n        <div><span class="c-name">%s</span>%s\n'
            '          %s<div class="c-desc">%s</div>\n          %s</div>\n      </div>\n') % (
        n, esc(c['name']), cid, choices, para(c.get('desc', '')), default)


def render(spec, out_path):
    css = open(os.path.join(HERE, 'guide.css'), encoding='utf-8').read()
    fonts = open(os.path.join(HERE, 'fonts_link.html'), encoding='utf-8').read()
    badges = ''.join('<span class="badge %s">%s</span>' % (b[0], esc(b[1])) for b in spec.get('badges', []))
    chain = ''
    if spec.get('chain'):
        hops = []
        for hop in spec['chain']:
            name, here = (hop, False) if isinstance(hop, str) else hop
            hops.append('<span class="hop%s">%s</span>' % (' here' if here else '', esc(name)))
        chain = '  <div class="chain-strip">\n    ' + '<span class="arrow">→</span>\n    '.join(hops) + '\n  </div>\n'
    body = ''
    num = 0
    for sec in spec['sections']:
        num += 1
        body += '\n  <div class="section-head"><span class="num">%02d</span><h2>%s</h2><div class="rule"></div></div>\n' % (num, esc(sec['title']))
        if sec.get('note'):
            body += '  <p class="section-note">%s</p>\n' % para(sec['note'])
        if sec.get('image'):
            body += ('  <div class="shot-frame" style="max-width:%s"><img src="%s" alt="%s"></div>\n'
                     '  <div class="shot-caption">%s</div>\n') % (sec.get('image_width', '760px'), esc(sec['image']),
                                                                   esc(sec.get('image_alt', spec['name'] + ' editor')), esc(sec.get('caption', '')))
        controls = sec.get('controls', [])
        if controls:
            body += '  <div class="ctrl-list">\n'
            for i, c in enumerate(controls, 1):
                body += control_html(i, c)
            body += '  </div>\n'
        for note in sec.get('notes', []):
            body += '  <div class="readout-note">\n    <div class="dot"></div>\n    <p>%s</p>\n  </div>\n' % para(note)
    return_html = '''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>%(name)s — Reference</title>
%(fonts)s<style>
%(css)s</style>
</head>
<body>
<div class="wrap">

  <div class="crumbs">%(crumbs)s</div>

  <div class="nameplate">
    <div>
      <h1>%(name)s</h1>
      <div class="role">%(role)s</div>
    </div>
    <div class="badges">%(badges)s</div>
  </div>

  <p class="lede">%(lede)s</p>
%(chain)s%(body)s
  <footer>
    <span>%(source)s</span>
    <span>%(name)s · <span class="mono">johnpascu77-dotcom</span></span>
  </footer>

</div>
</body>
</html>
''' % dict(name=esc(spec['name']), fonts=fonts, css=css, crumbs=spec.get('crumbs', esc(spec['name'])), role=esc(spec['role']),
           badges=badges, lede=para(spec['lede']), chain=chain, body=body, source=spec.get('source', ''))
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(return_html)
    return out_path

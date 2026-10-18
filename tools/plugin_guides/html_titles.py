"""Adds hover tooltips (title="...") to the controls of a static HTML tool UI.

  python html_titles.py <file.html> <tips.json>

tips.json maps an element id to its text. The tag with that id gets a title attribute (replacing an existing one),
and so does the <label for="id"> that belongs to it, so hovering either shows the text. A key may also be a CSS-like
"tag:text contains" selector, e.g. "button:Save to folder", for controls without an id: the first <button> whose
visible text contains that string. Re-runnable. Controls listed in the JSON but not found are reported.
"""
import html
import json
import re
import sys


def attr(text):
    return html.escape(text, quote=True)


def set_title(tag, text):
    tag = re.sub(r'\s+title="[^"]*"', '', tag)
    return re.sub(r'^(<\w+)', lambda m: m.group(1) + ' title="' + attr(text) + '"', tag, count=1)


def main():
    path, tips_path = sys.argv[1:3]
    raw = open(path, 'rb').read()
    bom = raw.startswith(b'\xef\xbb\xbf')
    src = raw.decode('utf-8-sig')
    tips = json.load(open(tips_path, encoding='utf-8'))
    missing = []
    for key, text in tips.items():
        n = 0
        if ':' in key and not key.startswith('#'):
            tagname, needle = key.split(':', 1)
            m = re.search(r'(<%s\b[^>]*>)([^<]*%s)' % (tagname, re.escape(needle)), src, re.S)
            if m:
                src = src[:m.start(1)] + set_title(m.group(1), text) + src[m.end(1):]
                n += 1
        else:
            ident = key.lstrip('#')
            m = re.search(r'<\w+\b[^>]*\bid="%s"[^>]*>' % re.escape(ident), src)
            if m:
                src = src[:m.start()] + set_title(m.group(0), text) + src[m.end():]
                n += 1
            m = re.search(r'<label\b[^>]*\bfor="%s"[^>]*>' % re.escape(ident), src)
            if m:
                src = src[:m.start()] + set_title(m.group(0), text) + src[m.end():]
                n += 1
            else:
                # no <label for>: the label sits in the same <div class="field"> as the control; title the wrapper too
                pos = re.search(r'\bid="%s"' % re.escape(ident), src)
                if pos:
                    lo = max(0, pos.start() - 600)
                    w = None
                    for w in re.finditer(r'<div class="field[^"]*"[^>]*>', src[lo:pos.start()]):
                        pass
                    if w:
                        src = src[:lo + w.start()] + set_title(w.group(0), text) + src[lo + w.end():]
        if not n:
            missing.append(key)
    open(path, 'w', newline='', encoding='utf-8-sig' if bom else 'utf-8').write(src)
    print('%s: %d of %d tooltips set%s' % (path.replace('\\', '/').split('/')[-1], len(tips) - len(missing), len(tips),
                                          ('; NOT FOUND: ' + ', '.join(missing)) if missing else ''))


if __name__ == '__main__':
    main()

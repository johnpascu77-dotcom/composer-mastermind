"""Builds the HTML reference pages: python make_guides.py [PluginName ...]   (no names = every spec)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from guide_lib import render  # noqa: E402
import specs_new  # noqa: E402
import specs_rest  # noqa: E402
import specs_more  # noqa: E402
import specs_ame  # noqa: E402

SPECS = dict(specs_new.SPECS)
SPECS.update(specs_rest.SPECS)
SPECS.update(specs_more.SPECS)
SPECS.update(specs_ame.SPECS)


def main():
    names = sys.argv[1:] or list(SPECS)
    for name in names:
        spec = SPECS[name]()
        out = render(spec, spec.pop('out'))
        print(name, '->', out)


if __name__ == '__main__':
    main()

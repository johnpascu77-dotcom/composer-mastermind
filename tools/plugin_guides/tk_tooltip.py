"""Hover tooltips for tkinter / ttk windows (stdlib only). One copy of this file lives in each tool that uses it.

    from tk_tooltip import tip
    tip(entry, "What this field does.")            # one widget
    tip(frame, "What this whole group does.", children=True)   # the frame and every widget inside it that has no tip of its own

The text is stored on the widget and read when the pointer rests on it, so a later call can replace it, and a
specific tip given to a child before or after wins over the group tip given to its frame.
"""
import tkinter as tk

_DELAY_MS = 600
_WRAP_PX = 380


class _Hover:
    """The one tooltip window and its pending timer."""

    def __init__(self):
        self.job = None
        self.window = None

    def show(self, widget):
        self.job = None
        text = getattr(widget, '_tip_text', '')
        if not text:
            return
        self.window = tk.Toplevel(widget)
        self.window.wm_overrideredirect(True)
        self.window.wm_geometry('+%d+%d' % (widget.winfo_pointerx() + 14, widget.winfo_pointery() + 18))
        tk.Label(self.window, text=text, justify='left', wraplength=_WRAP_PX, background='#ffffe1', foreground='#000000',
                 relief='solid', borderwidth=1, padx=6, pady=3).pack()

    def hide(self, widget):
        if self.job is not None:
            try:
                widget.after_cancel(self.job)
            except Exception:
                pass
            self.job = None
        if self.window is not None:
            try:
                self.window.destroy()
            except Exception:
                pass
            self.window = None


_hover = _Hover()


def _bind(widget):
    if getattr(widget, '_tip_bound', False):
        return
    widget._tip_bound = True

    def enter(_e):
        _hover.hide(widget)
        if getattr(widget, '_tip_text', ''):
            _hover.job = widget.after(_DELAY_MS, lambda: _hover.show(widget))

    widget.bind('<Enter>', enter, add='+')
    widget.bind('<Leave>', lambda _e: _hover.hide(widget), add='+')
    widget.bind('<ButtonPress>', lambda _e: _hover.hide(widget), add='+')


def tip(widget, text, children=False):
    """Give `widget` a tooltip. With children=True also every descendant that has no tip of its own yet."""
    widget._tip_text = text
    _bind(widget)
    if children:
        for child in widget.winfo_children():
            if not getattr(child, '_tip_text', ''):
                tip(child, text, children=True)
    return widget

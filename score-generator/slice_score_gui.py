#!/usr/bin/env python3
"""Desktop window for slice_score.py ("composing by slices"). Double-click run_slice_score.bat, or open this file in IDLE and press F5.

Pick the .mid file you load into MidiSampler, set the slicing exactly as it is set in MidiSampler, choose how the piece should
behave, press Generate. The window shows a live preview of the slices (keys and lengths), and after generating shows the result,
the MidiSampler/MPL setup sheet and the bar-by-bar timeline. Uses only Python's built-in tkinter; the work is done by slice_score.py.
"""

from __future__ import annotations

import json
import os
import random
import sys
import tempfile
import traceback
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from tkinter.scrolledtext import ScrolledText

import arc
import slice_score as ss

HERE = os.path.dirname(os.path.abspath(__file__))
SETTINGS_FILE = os.path.join(HERE, "slice_score_gui_settings.json")
DEFAULT_SOURCE = os.path.normpath(os.path.join(HERE, "..", "docs", "slice_test", "slice_test_source.mid"))
DEFAULT_OUT_DIR = os.path.normpath(os.path.join(HERE, "..", "docs"))

# name -> (default, help text). Order is the order the fields appear in.
DEFAULTS = {
    "title": "slice_demo",
    "source": DEFAULT_SOURCE,
    "out_dir": DEFAULT_OUT_DIR,
    "minutes": "1.5", "bars": "0", "bpm": "110", "beats_per_bar": "4", "tail_bars": "2",
    "slice_by": "beat", "grid": "4", "count": "8", "min_slice": "0.25", "base_key": "24",
    "range_start": "0", "range_end": "100", "thru": False,
    "sustain": "loop", "playback": "latch", "stop_key": "23", "speed": "100", "ratio": "1", "voices": "8",
    "shape": "arch_rise_fall", "strategy": "arch", "max_layers": "3", "min_hold": "2", "max_hold": "6",
    "swap_chance": "0.5", "restlessness": "0.35", "breaths": True, "shooters": "1", "seed": "7",
}

HELP = {
    "title": "Name of the piece (also used for the file names).",
    "source": "The .mid file you load into MidiSampler. Its notes decide where the slices are.",
    "out_dir": "Folder where the .json score, the setup sheet and the timeline are written.",
    "minutes": "Length of the piece in minutes (ignored if 'Bars' is above 0).",
    "bars": "Exact length in bundle bars; 0 = work it out from minutes and tempo.",
    "bpm": "Host tempo you will play at. Slice lengths in bars follow it.",
    "beats_per_bar": "Beats per bar (4 for 4/4).",
    "tail_bars": "Silent bars at the very end so the last slices can ring out.",
    "slice_by": "MidiSampler 'Slice by': beat grid, equal parts, or transient (note onsets).",
    "grid": "Beat grid: slice length in beats (4 beats = one bar in 4/4).",
    "count": "Equal parts: how many slices.",
    "min_slice": "Transient: onsets closer together than this many beats merge into one slice.",
    "base_key": "MidiSampler 'First slice key' as a note NUMBER (24 is shown as C0 in the plugin).",
    "range_start": "MidiSampler Start, in percent of the source.",
    "range_end": "MidiSampler End, in percent of the source.",
    "thru": "MidiSampler 'Slice playback = Thru' (a slice plays on to the end of the range).",
    "sustain": "MidiSampler Sustain mode: Loop Forward (slices loop until stopped) or Forward (play once).",
    "playback": "MidiSampler Playback: Trigger/latch (press again to stop) or Start only (press always restarts).",
    "stop_key": "MidiSampler Stop-All Key as a note number (-1 = none). 23 is shown as B-1.",
    "speed": "MidiSampler Speed, percent.",
    "ratio": "The zone's speed ratio (1 = normal, 0.6667 = 2/3, 1.5 = 3/2).",
    "voices": "MidiSampler Voices setting (the score never needs more sounding slices than this).",
    "shape": "Energy shape over the whole piece. Energy drives how many slices sound at once, how fast they change, and how hard.",
    "strategy": "Which slice comes next: order, retro (backwards), arch (follows the energy), random, or row (twelve-tone row; needs 12+ slices).",
    "max_layers": "Most slices sounding at the same time.",
    "min_hold": "Fewest bars before the set of slices changes (used at peak energy).",
    "max_hold": "Most bars before the set of slices changes (used at low energy).",
    "swap_chance": "0-1: how often one slice is swapped for another while the layer count stays the same.",
    "restlessness": "0-1: random wobble added to the energy curve.",
    "breaths": "Let quiet stretches drop to silence (the opening never does).",
    "shooters": "Number of MPL trigger instances (channels 1..N). Use more than 1 only if you route several MPLs into MidiSampler.",
    "seed": "Same seed + same settings = same piece. Press 'Random' for a new one.",
}


class Tooltip:
    def __init__(self, widget: tk.Widget, text: str):
        self.widget, self.text, self.tip = widget, text, None
        widget.bind("<Enter>", self._show)
        widget.bind("<Leave>", self._hide)

    def _show(self, _event=None):
        if self.tip or not self.text:
            return
        x = self.widget.winfo_rootx() + 18
        y = self.widget.winfo_rooty() + self.widget.winfo_height() + 4
        self.tip = tk.Toplevel(self.widget)
        self.tip.wm_overrideredirect(True)
        self.tip.wm_geometry(f"+{x}+{y}")
        tk.Label(self.tip, text=self.text, justify="left", background="#fffbe6", relief="solid", borderwidth=1,
                 wraplength=380, padx=6, pady=3).pack()

    def _hide(self, _event=None):
        if self.tip:
            self.tip.destroy()
            self.tip = None


class App:
    def __init__(self, root: tk.Tk):
        self.root = root
        root.title("Composing by slices - score generator")
        root.geometry("1380x800")
        root.minsize(1100, 640)
        self.vars: dict[str, tk.Variable] = {}
        self._seq_cache: tuple[str, float, ss.Sequence] | None = None

        saved = {}
        try:
            with open(SETTINGS_FILE, encoding="utf-8") as f:
                saved = json.load(f)
        except Exception:
            pass
        if "out_dir" in saved and not os.path.isdir(str(saved["out_dir"])):
            saved.pop("out_dir")
        for name, default in DEFAULTS.items():
            value = saved.get(name, default)
            self.vars[name] = tk.BooleanVar(value=bool(value)) if isinstance(default, bool) else tk.StringVar(value=str(value))

        self._build()
        for var in self.vars.values():
            var.trace_add("write", lambda *_: self.root.after_idle(self.update_preview))
        self.update_preview()

    # ------------------------------------------------------------------ layout
    def _build(self):
        outer = ttk.Frame(self.root, padding=8)
        outer.pack(fill="both", expand=True)
        outer.columnconfigure(0, weight=0)
        outer.columnconfigure(1, weight=0)
        outer.columnconfigure(2, weight=1)
        outer.rowconfigure(0, weight=1)

        left = ttk.Frame(outer)
        left.grid(row=0, column=0, sticky="nsew", padx=(0, 8))
        middle = ttk.Frame(outer)
        middle.grid(row=0, column=1, sticky="nsew", padx=(0, 8))
        right = ttk.Frame(outer)
        right.grid(row=0, column=2, sticky="nsew")
        right.rowconfigure(1, weight=1)
        right.columnconfigure(0, weight=1)

        # ---- left column
        box = self._group(left, "1. Source and output")
        self._entry(box, "title", "Title", width=26)
        self._file_row(box, "source", "MIDI source", self.pick_source)
        self._file_row(box, "out_dir", "Output folder", self.pick_folder)

        box = self._group(left, "2. Piece")
        self._entry(box, "minutes", "Minutes")
        self._entry(box, "bars", "Bars (0 = from minutes)")
        self._entry(box, "bpm", "Tempo (BPM)")
        self._entry(box, "beats_per_bar", "Beats per bar")
        self._entry(box, "tail_bars", "Silent tail (bars)")

        box = self._group(left, "3. Slicing  (copy MidiSampler's settings)")
        self._combo(box, "slice_by", "Slice by", ["beat", "equal", "transient"])
        self._entry(box, "grid", "Grid (beats)")
        self._entry(box, "count", "Slices (equal)")
        self._entry(box, "min_slice", "Min slice (transient)")
        self._entry(box, "base_key", "First slice key (number)")
        self._entry(box, "range_start", "Range start %")
        self._entry(box, "range_end", "Range end %")
        self._check(box, "thru", "Slice playback = Thru")

        # ---- middle column
        box = self._group(middle, "4. Playback  (copy MidiSampler's settings)")
        self._combo(box, "sustain", "Sustain mode", ["loop", "forward"])
        self._combo(box, "playback", "Playback", ["latch", "start_only"])
        self._entry(box, "stop_key", "Stop-All key (number)")
        self._entry(box, "speed", "Speed %")
        self._entry(box, "ratio", "Zone speed ratio")
        self._entry(box, "voices", "Voices")

        box = self._group(middle, "5. Shape of the piece")
        self._combo(box, "shape", "Energy shape", list(arc.SHAPES))
        self._combo(box, "strategy", "Slice strategy", ["order", "retro", "arch", "random", "row"])
        self._entry(box, "max_layers", "Max slices at once")
        self._entry(box, "min_hold", "Min bars between changes")
        self._entry(box, "max_hold", "Max bars between changes")
        self._entry(box, "swap_chance", "Swap chance (0-1)")
        self._entry(box, "restlessness", "Restlessness (0-1)")
        self._check(box, "breaths", "Allow silent breaths")

        box = self._group(middle, "6. MPL and randomness")
        self._entry(box, "shooters", "MPL trigger instances")
        row = ttk.Frame(box)
        row.pack(fill="x", pady=1)
        ttk.Label(row, text="Seed", width=24).pack(side="left")
        entry = ttk.Entry(row, textvariable=self.vars["seed"], width=12)
        entry.pack(side="left")
        Tooltip(entry, HELP["seed"])
        ttk.Button(row, text="Random", width=8, command=lambda: self.vars["seed"].set(str(random.randint(1, 2 ** 31 - 1)))).pack(side="left", padx=4)

        buttons = ttk.Frame(middle)
        buttons.pack(fill="x", pady=(10, 0))
        self.generate_button = ttk.Button(buttons, text="Generate score", command=self.generate)
        self.generate_button.pack(fill="x", ipady=8)
        ttk.Button(buttons, text="Open output folder", command=self.open_folder).pack(fill="x", pady=(6, 0))
        ttk.Button(buttons, text="Reset all settings", command=self.reset).pack(fill="x", pady=(6, 0))

        # ---- right column: preview + results
        preview = ttk.LabelFrame(right, text="Live preview of the slices", padding=6)
        preview.grid(row=0, column=0, sticky="ew")
        self.preview_label = tk.Label(preview, text="", justify="left", anchor="w", wraplength=500, font=("Segoe UI", 10))
        self.preview_label.pack(fill="x")
        preview.bind("<Configure>", lambda e: self.preview_label.configure(wraplength=max(200, e.width - 24)))

        self.notebook = ttk.Notebook(right)
        self.notebook.grid(row=1, column=0, sticky="nsew", pady=(8, 0))
        self.result_text = self._tab("Result", "Segoe UI")
        self.setup_text = self._tab("Setup sheet", "Consolas")
        self.timeline_text = self._tab("Timeline", "Consolas")
        self._write(self.result_text, "Press 'Generate score'.\n\nThe score is heard 2 bars after the bundle's bar numbers, so the first sound is in Bitwig bar 3.\n"
                                       "After generating, read the 'Setup sheet' tab: it lists what to set in MidiSampler and MPL, and the score only works if they are set up exactly like that.")

    def _group(self, parent, title):
        frame = ttk.LabelFrame(parent, text=title, padding=6)
        frame.pack(fill="x", pady=(0, 8))
        return frame

    def _row(self, parent, name, label):
        row = ttk.Frame(parent)
        row.pack(fill="x", pady=1)
        lab = ttk.Label(row, text=label, width=24)
        lab.pack(side="left")
        Tooltip(lab, HELP.get(name, ""))
        return row

    def _entry(self, parent, name, label, width=14):
        row = self._row(parent, name, label)
        entry = ttk.Entry(row, textvariable=self.vars[name], width=width)
        entry.pack(side="left")
        Tooltip(entry, HELP.get(name, ""))

    def _combo(self, parent, name, label, values):
        row = self._row(parent, name, label)
        box = ttk.Combobox(row, textvariable=self.vars[name], values=values, state="readonly", width=18)
        box.pack(side="left")
        Tooltip(box, HELP.get(name, ""))

    def _check(self, parent, name, label):
        check = ttk.Checkbutton(parent, text=label, variable=self.vars[name])
        check.pack(anchor="w", pady=1)
        Tooltip(check, HELP.get(name, ""))

    def _file_row(self, parent, name, label, command):
        ttk.Label(parent, text=label).pack(anchor="w")
        row = ttk.Frame(parent)
        row.pack(fill="x", pady=(0, 3))
        entry = ttk.Entry(row, textvariable=self.vars[name], width=34)
        entry.pack(side="left", fill="x", expand=True)
        Tooltip(entry, HELP.get(name, ""))
        ttk.Button(row, text="Browse...", command=command, width=9).pack(side="left", padx=(4, 0))

    def _tab(self, title, font_name):
        frame = ttk.Frame(self.notebook)
        self.notebook.add(frame, text=title)
        text = ScrolledText(frame, wrap="none" if font_name == "Consolas" else "word", font=(font_name, 10), height=10)
        text.pack(fill="both", expand=True)
        text.configure(state="disabled")
        return text

    @staticmethod
    def _write(widget, text):
        widget.configure(state="normal")
        widget.delete("1.0", "end")
        widget.insert("1.0", text)
        widget.configure(state="disabled")

    # ------------------------------------------------------------------ actions
    def pick_source(self):
        path = filedialog.askopenfilename(title="Choose the MIDI file you load into MidiSampler",
                                          filetypes=[("MIDI files", "*.mid *.midi"), ("All files", "*.*")],
                                          initialdir=os.path.dirname(self.vars["source"].get()) or HERE)
        if path:
            self.vars["source"].set(os.path.normpath(path))

    def pick_folder(self):
        path = filedialog.askdirectory(title="Where should the score be written?", initialdir=self.vars["out_dir"].get() or HERE)
        if path:
            self.vars["out_dir"].set(os.path.normpath(path))

    def open_folder(self):
        folder = self.vars["out_dir"].get()
        if os.path.isdir(folder):
            os.startfile(folder)                       # Windows
        else:
            messagebox.showinfo("Output folder", "That folder does not exist yet. Generate a score first.")

    def reset(self):
        if messagebox.askyesno("Reset", "Put every setting back to its default?"):
            for name, default in DEFAULTS.items():
                self.vars[name].set(default if isinstance(default, bool) else str(default))

    def save_settings(self):
        try:
            data = {name: var.get() for name, var in self.vars.items()}
            with open(SETTINGS_FILE, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2)
        except Exception:
            pass

    # ------------------------------------------------------------------ settings -> argv
    def _num(self, name, kind=float):
        text = str(self.vars[name].get()).strip().replace(",", ".")
        try:
            if "/" in text:                                  # fractions like 1/2 or 2/3, as MidiSampler shows the speed ratio
                top, bottom = text.split("/", 1)
                return kind(float(top) / float(bottom))
            return kind(text)
        except (ValueError, ZeroDivisionError):
            raise ValueError(f"'{HELP_LABELS.get(name, name)}' must be a number (it is '{text}')")

    def build_argv(self, out_override: str | None = None) -> list[str]:
        v = self.vars
        title = v["title"].get().strip() or "slice_score"
        safe = "".join(c if c.isalnum() or c in "-_ " else "_" for c in title).strip().replace(" ", "_") or "slice_score"
        out = out_override or os.path.join(v["out_dir"].get().strip() or DEFAULT_OUT_DIR, safe + ".json")
        seed = v["seed"].get().strip()
        argv = [
            "--title", title, "--source", v["source"].get().strip(), "--output", out,
            "--minutes", str(self._num("minutes")), "--bars", str(self._num("bars", int)), "--bpm", str(self._num("bpm")),
            "--beats-per-bar", str(self._num("beats_per_bar", int)), "--tail-bars", str(self._num("tail_bars", int)),
            "--shape", v["shape"].get(), "--strategy", v["strategy"].get(), "--restlessness", str(self._num("restlessness")),
            "--max-layers", str(self._num("max_layers", int)), "--voices", str(self._num("voices", int)),
            "--min-hold", str(self._num("min_hold", int)), "--max-hold", str(self._num("max_hold", int)),
            "--swap-chance", str(self._num("swap_chance")), "--shooters", str(self._num("shooters", int)),
            "--slice-by", v["slice_by"].get(), "--grid", str(self._num("grid")), "--count", str(self._num("count", int)),
            "--min-slice", str(self._num("min_slice")), "--base-key", str(self._num("base_key", int)),
            "--sustain", v["sustain"].get(), "--playback", v["playback"].get(), "--ratio", str(self._num("ratio")),
            "--speed", str(self._num("speed")), "--stop-key", str(self._num("stop_key", int)),
            "--range-start", str(self._num("range_start") / 100.0), "--range-end", str(self._num("range_end") / 100.0),
        ]
        if seed:
            argv += ["--seed", str(int(float(seed)))]
        if v["thru"].get():
            argv.append("--thru")
        if not v["breaths"].get():
            argv.append("--no-breaths")
        return argv

    # ------------------------------------------------------------------ preview
    def _load_source(self, path):
        mtime = os.path.getmtime(path)
        if self._seq_cache and self._seq_cache[0] == path and self._seq_cache[1] == mtime:
            return self._seq_cache[2]
        seq = ss.read_midi_sequence(path)
        self._seq_cache = (path, mtime, seq)
        return seq

    def update_preview(self):
        try:
            path = self.vars["source"].get().strip()
            if not os.path.isfile(path):
                raise ValueError("Choose the MIDI file you load into MidiSampler (section 1).")
            args = ss.build_parser().parse_args(self.build_argv(out_override=os.path.join(tempfile.gettempdir(), "preview.json")))
            seq = self._load_source(path)
            _, zone, cfg, n_slices = ss.make_setup(args)
            lengths = ss.slice_bar_lengths(seq, zone, cfg.speed_percent, cfg.beats_per_bar)
            keys_hi = zone.base_key + n_slices - 1
            shown = ", ".join(f"{x:.2f}" for x in lengths[:12]) + (" ..." if len(lengths) > 12 else "")
            loops = "they LOOP until pressed again" if zone.loop and zone.playback == "latch" else (
                "they loop until the stop-all key" if zone.loop else "each plays once, then ends by itself")
            text = (f"Source: {len(seq.notes)} notes, {seq.length_beats:.2f} beats.   {n_slices} slices on keys {zone.base_key}-{keys_hi} "
                    f"({ss.key_name(zone.base_key)} to {ss.key_name(keys_hi)}); {loops}.\n"
                    f"Slice lengths in bars: {shown}\n"
                    f"Piece: {cfg.bars} bars, heard in Bitwig bars {1 + ss.LAG_BARS}-{cfg.bars + ss.LAG_BARS}"
                    + (f".   Stop-all key {cfg.stop_key} ({ss.key_name(cfg.stop_key)})." if cfg.stop_key is not None else ".   No stop-all key."))
            self.preview_label.configure(text=text, fg="#1a1a1a")
        except SystemExit as exc:
            self.preview_label.configure(text=str(exc.code), fg="#b00020")
        except Exception as exc:
            self.preview_label.configure(text=str(exc), fg="#b00020")

    # ------------------------------------------------------------------ generate
    def generate(self):
        self.save_settings()
        self.generate_button.configure(state="disabled")
        self.root.update_idletasks()
        try:
            args = ss.build_parser().parse_args(self.build_argv())
            result = ss.generate(args)
        except SystemExit as exc:
            self._write(self.result_text, f"Cannot generate:\n\n{exc.code}")
            self.notebook.select(0)
            self.generate_button.configure(state="normal")
            return
        except Exception as exc:
            self._write(self.result_text, f"Something went wrong:\n\n{exc}\n\n{traceback.format_exc()}")
            self.notebook.select(0)
            self.generate_button.configure(state="normal")
            return

        ok = not result["problems"]
        lines = [("DONE - verification passed" if ok else "VERIFICATION FAILED - do not use this score"), "",
                 f"Score:    {result['bundle_path']}", f"Setup:    {result['setup_path']}", f"Timeline: {result['timeline_path']}", "",
                 result["summary"], ""]
        if ok:
            lines += ["The finished score was re-read and replayed: it reproduces the planned slices bar by bar, nothing is left sounding after the end.",
                      "", "Next:", "  1. Read the 'Setup sheet' tab and set MidiSampler / MPL exactly like that.",
                      "  2. In Mastermind: Load Score, Content mode = Absolute, play from Bitwig bar 1.",
                      "  3. Remember: everything is heard 2 bars after the bundle's bar numbers (first sound in Bitwig bar 3)."]
        else:
            lines += ["Problems found:"] + [f"  - {p}" for p in result["problems"]]
        self._write(self.result_text, "\n".join(lines))
        for widget, path in ((self.setup_text, result["setup_path"]), (self.timeline_text, result["timeline_path"])):
            with open(path, encoding="utf-8") as f:
                self._write(widget, f.read())
        self.notebook.select(0)
        self.generate_button.configure(state="normal")


HELP_LABELS = {"minutes": "Minutes", "bars": "Bars", "bpm": "Tempo", "beats_per_bar": "Beats per bar", "tail_bars": "Silent tail",
               "grid": "Grid", "count": "Slices (equal)", "min_slice": "Min slice", "base_key": "First slice key",
               "range_start": "Range start", "range_end": "Range end", "stop_key": "Stop-All key", "speed": "Speed",
               "ratio": "Zone speed ratio", "voices": "Voices", "max_layers": "Max slices at once", "min_hold": "Min bars between changes",
               "max_hold": "Max bars between changes", "swap_chance": "Swap chance", "restlessness": "Restlessness",
               "shooters": "MPL trigger instances"}


def selftest() -> int:
    """Headless smoke test: builds the window hidden, generates with the defaults into a temp folder, checks the result."""
    root = tk.Tk()
    root.withdraw()
    app = App(root)
    app.save_settings = lambda: None                      # a test must not overwrite the user's saved settings
    with tempfile.TemporaryDirectory() as tmp:
        app.vars["out_dir"].set(tmp)
        app.generate()
        out = os.path.join(tmp, "slice_demo.json")
        ok = os.path.exists(out) and os.path.exists(os.path.join(tmp, "slice_demo.setup.md")) and "DONE" in app.result_text.get("1.0", "end")
        print("preview:", app.preview_label.cget("text").splitlines()[0])
        print("GUI SELFTEST", "PASSED" if ok else "FAILED")
    root.destroy()
    return 0 if ok else 1


def main() -> None:
    if "--selftest" in sys.argv:
        sys.exit(selftest())
    try:                                                    # crisp text on high-DPI screens (Windows)
        import ctypes
        ctypes.windll.shcore.SetProcessDpiAwareness(1)
    except Exception:
        pass
    root = tk.Tk()
    App(root)
    root.mainloop()


if __name__ == "__main__":
    main()

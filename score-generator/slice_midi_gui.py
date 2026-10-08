#!/usr/bin/env python3
"""Desktop window for slice_midi_score.py (MIDI clip for MidiSampler). Double-click run_slice_midi.bat, or open in IDLE and press F5.
Pick the setup .json saved with MidiSampler's "Save Setup...", choose the behaviour, press Generate. The window shows the checks and the
bar-by-bar plan; the .mid (and a .report.txt) is written to the chosen folder. Only built-in tkinter; the work is done by slice_midi_score.py."""

from __future__ import annotations

import json
import os
import random
import tkinter as tk
import traceback
from tkinter import filedialog, messagebox, ttk
from tkinter.scrolledtext import ScrolledText

import arc
import slice_midi_score as sm

HERE = os.path.dirname(os.path.abspath(__file__))
SETTINGS_FILE = os.path.join(HERE, "slice_midi_gui_settings.json")

# key -> (label, default, help)
FIELDS = [
    ("setup", "MidiSampler setup (.json)", "", "The file saved with MidiSampler's 'Save Setup...'. It carries zones, voice limit, stop key and the source."),
    ("out", "Output clip (.mid)", os.path.normpath(os.path.join(HERE, "..", "docs", "slice_clip.mid")), "The MIDI clip to write. A .report.txt is written next to it."),
    ("minutes", "Minutes", "2", "Length of the piece (ignored if Bars is above 0)."),
    ("bars", "Bars (0 = from minutes)", "0", "Exact length in bars."),
    ("bpm", "Tempo written in file", "100", "Only labels the file and turns minutes into bars; durations are in beats."),
    ("shape", "Energy shape", "arch_rise_fall", "Energy over the piece: more layers, shorter holds and harder velocity as it rises."),
    ("strategy", "Which zone next", "arch", "order / retro / arch (follows energy) / random."),
    ("max_layers", "Most layers at once", "3", "Long notes sounding together."),
    ("min_hold", "Fewest bars between changes", "2", "Used at peak energy."),
    ("max_hold", "Most bars between changes", "6", "Used at low energy."),
    ("swap_chance", "Swap chance (0-1)", "0.5", "How often one layer is replaced while the count stays."),
    ("accent_rate", "Accent notes per beat", "1.0", "Short 16th ornaments at full energy (0 = none). Only Gate zones can have them."),
    ("restlessness", "Restlessness (0-1)", "0.35", "Random wobble on the energy curve."),
    ("allowed", "Allowed transpositions", "", "Semitones mod 12 that may sound, e.g. 0,3,7. Empty = all."),
    ("zones", "Only zones", "", "e.g. 1,2,4. Empty = every enabled zone."),
    ("max_degrees", "Keys per zone", "12", "How many keys of a zone's octave the clip may use."),
    ("velocity", "Velocity range", "50,118", "lowest,highest"),
    ("tail", "Silent tail bars", "2", "Bars at the end where everything is released."),
    ("seed", "Seed", "7", "Same seed + settings = same clip."),
]
CHECKS = [("avoid", "Avoid minor-second clashes between layers", False), ("breaths", "Let quiet stretches drop to silence", True),
          ("stagger", "Stagger layer entries inside the bar", True)]


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("MidiSampler clip generator")
        self.geometry("1020x720")
        self.vars = {}
        self.status = tk.StringVar(value="Pick a setup .json, then Generate.")
        choices = {"shape": arc.SHAPES, "strategy": ["order", "retro", "arch", "random"]}
        saved = {}
        try:
            with open(SETTINGS_FILE) as f:
                saved = json.load(f)
        except Exception:
            pass
        left = ttk.Frame(self, padding=8)
        left.pack(side="left", fill="y")
        for r, (key, label, default, help_) in enumerate(FIELDS):
            ttk.Label(left, text=label).grid(row=r, column=0, sticky="w", pady=1)
            v = tk.StringVar(value=str(saved.get(key, default)))
            self.vars[key] = v
            if key in choices:
                w = ttk.Combobox(left, textvariable=v, values=choices[key], state="readonly", width=28)
            else:
                w = ttk.Entry(left, textvariable=v, width=40 if key in ("setup", "out") else 31)
            w.grid(row=r, column=1, sticky="w")
            w.bind("<Enter>", lambda e, h=help_: self.status.set(h))
            if key in ("setup", "out"):
                ttk.Button(left, text="...", width=3, command=lambda k=key: self.browse(k)).grid(row=r, column=2)
        r = len(FIELDS)
        for key, label, default in CHECKS:
            v = tk.BooleanVar(value=bool(saved.get(key, default)))
            self.vars[key] = v
            ttk.Checkbutton(left, text=label, variable=v).grid(row=r, column=0, columnspan=2, sticky="w")
            r += 1
        bt = ttk.Frame(left)
        bt.grid(row=r, column=0, columnspan=3, pady=8, sticky="w")
        ttk.Button(bt, text="Generate", command=self.generate).pack(side="left")
        ttk.Button(bt, text="Random seed", command=lambda: self.vars["seed"].set(str(random.randint(1, 99999)))).pack(side="left", padx=6)
        ttk.Label(left, textvariable=self.status, wraplength=380, foreground="#555").grid(row=r + 1, column=0, columnspan=3, sticky="w")
        self.text = ScrolledText(self, font=("Consolas", 10), wrap="none")
        self.text.pack(side="right", fill="both", expand=True)

    def browse(self, key):
        if key == "setup":
            p = filedialog.askopenfilename(filetypes=[("MidiSampler setup", "*.json"), ("All", "*.*")])
        else:
            p = filedialog.asksaveasfilename(defaultextension=".mid", filetypes=[("MIDI", "*.mid")])
        if p:
            self.vars[key].set(p)

    def generate(self, save=True):
        v = {k: x.get() for k, x in self.vars.items()}
        if save:
            try:
                with open(SETTINGS_FILE, "w") as f:
                    json.dump(v, f)
            except Exception:
                pass
        argv = ["--setup", v["setup"], "--output", v["out"], "--minutes", v["minutes"], "--bars", v["bars"], "--bpm", v["bpm"],
                "--shape", v["shape"], "--strategy", v["strategy"], "--max-layers", v["max_layers"], "--min-hold", v["min_hold"],
                "--max-hold", v["max_hold"], "--swap-chance", v["swap_chance"], "--accent-rate", v["accent_rate"],
                "--restlessness", v["restlessness"], "--allowed-transpositions", v["allowed"], "--zones", v["zones"],
                "--max-degrees", v["max_degrees"], "--velocity-range", v["velocity"], "--tail-bars", v["tail"], "--seed", v["seed"]]
        if v["avoid"]:
            argv.append("--avoid-clashes")
        if not v["breaths"]:
            argv.append("--no-breaths")
        if not v["stagger"]:
            argv.append("--no-stagger")
        self.text.delete("1.0", "end")
        self.status.set("Generating and verifying...")
        self.update_idletasks()
        try:
            r = sm.generate(sm.build_parser().parse_args(argv))
        except SystemExit as exc:
            self.status.set("Cannot generate.")
            messagebox.showerror("Cannot generate", str(exc.code) if isinstance(exc.code, str) else "invalid settings")
            return
        except Exception:
            self.text.insert("end", traceback.format_exc())
            self.status.set("Error.")
            return
        with open(r["report"], encoding="utf-8") as f:
            self.text.insert("end", f.read())
        self.status.set(("VERIFIED: " if not r["problems"] else "PROBLEMS: ") + r["summary"] + "  ->  " + r["clip"])


if __name__ == "__main__":
    App().mainloop()

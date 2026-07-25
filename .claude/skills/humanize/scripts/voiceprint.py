#!/usr/bin/env python3
"""Measure the voice features that separate CaveWhere's hand-written prose from
its AI-drafted prose, so a doc can be steered toward a target instead of a vibe.

Every feature here corresponds to a habit described in references/voice.md. The
point is that they are *counted*, not noticed.

    voiceprint.py docs/manual                     # profile a corpus
    voiceprint.py --against reference.json docs/manual/index.md
    voiceprint.py --json docs/manual > reference.json

Rates are per 1000 words of body prose. Headings and image captions are counted
separately: they are fragments, and mixing them in wrecks the sentence stats.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from slopcheck import extract_markdown  # noqa: E402

kWordRe = re.compile(r"\b[\w'-]+\b")
kSentSplit = re.compile(r"(?<=[.!?])\s+")

# Each feature: (label, regex, what habit it measures)
kFeatures: list[tuple[str, re.Pattern, str]] = [
    ("em dash",       re.compile(r"—"),                                   "aside punctuation"),
    ("parenthesis",   re.compile(r"\("),                                  "aside punctuation"),
    ("exclamation",   re.compile(r"!"),                                   "escalation"),
    ("question",      re.compile(r"\?"),                                  "asks the reader"),
    ("first person",  re.compile(r"\b(?:I|I['’]m|I['’]ve|my|me)\b"),      "owns the judgment"),
    ("second person", re.compile(r"\b(?:you|your|you['’](?:re|ll))\b", re.I),
                                                                          "addresses the reader"),
    # WordPress rewrites ' as U+2019, so both forms must match or the blog
    # scores zero contractions -- which it very much does not have.
    ("contraction",   re.compile(r"\b\w+['’](?:s|t|re|ve|ll|d|m)\b", re.I),
                                                                          "informal register"),
    ("digit token",   re.compile(r"\b\d[\d.,]*\s?(?:%|x|mb|gb|kb|s|ms|in|mi|ppi|bit)?\b", re.I),
                                                                          "checkable specifics"),
    ("distinct names", re.compile(r"(?<![.!?]\s)(?<!^)\b[A-Z][a-zA-Z0-9]{2,}\b"),
                                                                          "variety of things named"),
    ("hedge/admit",   re.compile(r"\b(?:hopefully|sometimes|unfortunately|may|might|ballpark|"
                                 r"unscientific|vary|drawback|downside|limitation|problematic|"
                                 r"undesirable|fails?|isn['’]t|caveat|approximate)\b", re.I),
                                                                          "admits limits"),
    ("sent-initial conj", re.compile(r"(?:^|(?<=[.!?]\s))(?:But|And|So|Also|Or)\b"),
                                                                          "spoken rhythm"),
    # The habit is register-independent; only its dress changes. The blog says
    # "see below", the ICS paper says "see Figure 3". Match both or the feature
    # looks like a blog quirk when it is actually voice.
    # "above" as well as "below": a caption sits under its image, so the prose
    # that points back at it from the caption legitimately says "shown above".
    ("points at figure", re.compile(r"\b(?:see (?:below|above)|shown (?:below|above)|"
                                    r"check ?out|below shows|above shows|"
                                    r"see the (?:image|video|graph)|as shown|"
                                    r"(?:see|in|from)\s+(?:figure|fig\.?|table)\s*\d+|"
                                    r"(?:figure|fig\.?|table)\s*\d+\s+(?:shows|illustrates|"
                                    r"demonstrates))\b", re.I),
                                                                          "points at the figure"),
    ("recommends",    re.compile(r"\b(?:I recommend|it's recommended|is recommended|"
                                 r"recommended to)\b", re.I),             "makes the call"),
]


def sentences(text: str) -> list[int]:
    out = []
    for s in kSentSplit.split(text):
        n = len(kWordRe.findall(s))
        if n >= 3:
            out.append(n)
    return out


def load_blog(path: str) -> tuple[list[str], list[str]]:
    """Read the tab-separated corpus produced by the scratchpad extractor."""
    body, heads = [], []
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            parts = line.rstrip("\n").split("\t")
            if len(parts) != 3:
                continue
            _, kind, text = parts
            (heads if kind == "HEAD" else body).append(text)
    return body, heads


def load_markdown(targets: list[str]) -> tuple[list[str], list[str]]:
    body, heads = [], []
    paths: list[str] = []
    for t in targets:
        if os.path.isdir(t):
            for root, dirs, files in os.walk(t):
                dirs[:] = [d for d in dirs
                           if d not in (".git", "build", "images", "_site")]
                paths += [os.path.join(root, f) for f in sorted(files) if f.endswith(".md")]
        elif t.endswith(".md"):
            paths.append(t)
    for p in paths:
        with open(p, encoding="utf-8", errors="replace") as fh:
            for _, line in extract_markdown(fh.read()):
                s = line.strip()
                if not s:
                    continue
                if s.startswith("#"):
                    heads.append(s.lstrip("# ").strip())
                elif s.startswith(("|", ">")):
                    continue
                else:
                    body.append(re.sub(r"^\s*[-*+]\s+|\s*\d+\.\s+", "", s))
    return body, heads


def profile(body: list[str], heads: list[str], name: str) -> dict:
    text = " ".join(body)
    words = len(kWordRe.findall(text))
    sents = sentences(text)
    prof = {
        "name": name,
        "words": words,
        "mean_sentence": round(statistics.mean(sents), 1) if sents else 0.0,
        "p90_sentence": round(sorted(sents)[int(len(sents) * .9)], 1) if sents else 0.0,
        "question_headings_pct": round(
            100 * sum(1 for h in heads if h.rstrip().endswith("?")) / len(heads), 1) if heads else 0.0,
        "headings": len(heads),
        "rates": {},
    }
    for label, rx, _why in kFeatures:
        # "distinct names" measures how many *different* things get named. A raw
        # count is dominated by the product name repeating and says nothing.
        n = len({m.lower() for m in rx.findall(text)}) if label == "distinct names" \
            else len(rx.findall(text))
        prof["rates"][label] = round(n * 1000.0 / words, 2) if words else 0.0
    return prof


def show(profiles: list[dict], ref: dict | None) -> None:
    cols = [p["name"] for p in profiles]
    w = max(14, *(len(c) for c in cols)) + 2
    print(f"{'':<20}" + "".join(f"{c:>{w}}" for c in cols))
    print(f"{'':<20}" + "".join(f"{'-' * (w - 2):>{w}}" for _ in cols))

    def row(label: str, vals: list, suffix: str = ""):
        print(f"{label:<20}" + "".join(f"{str(v) + suffix:>{w}}" for v in vals))

    row("words", [p["words"] for p in profiles])
    row("mean sentence", [p["mean_sentence"] for p in profiles])
    row("p90 sentence", [p["p90_sentence"] for p in profiles])
    row("? headings", [p["question_headings_pct"] for p in profiles], "%")
    print(f"\n{'per 1k words':<20}" + "".join(f"{c:>{w}}" for c in cols))
    print(f"{'':<20}" + "".join(f"{'-' * (w - 2):>{w}}" for _ in cols))
    invariants = set(ref.get("invariants", [])) if ref else set()
    for label, _rx, why in kFeatures:
        vals = [p["rates"][label] for p in profiles]
        mark = "!" if label in invariants else " "
        line = f"{mark} {label:<18}" + "".join(f"{v:>{w}.2f}" for v in vals)
        if ref and len(profiles) == 1:
            target = ref["rates"].get(label)
            if target is not None:
                off = abs(vals[0] - target) > max(1.0, target * 0.5)
                line += f"{'  <-- ' if off else '      '}target {target:.2f}"
        else:
            line += f"    {why}"
        print(line)
    if invariants:
        print("\n! = holds across all three hand-written corpora (2020 blog, ICS 2013 and\n"
              "    2021 papers), so it is voice, not genre. Unmarked rows are register:\n"
              "    judge them against the document type, not the number.")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("targets", nargs="+", help=".md files/dirs, or a .tsv blog corpus")
    ap.add_argument("--name", default=None)
    ap.add_argument("--json", action="store_true", help="emit the profile as JSON")
    ap.add_argument("--against", metavar="REF.json", help="compare to a saved profile")
    args = ap.parse_args()

    if len(args.targets) == 1 and args.targets[0].endswith(".tsv"):
        body, heads = load_blog(args.targets[0])
    else:
        body, heads = load_markdown(args.targets)
    if not body:
        print("voiceprint: no prose found", file=sys.stderr)
        return 2

    name = args.name or os.path.basename(args.targets[0].rstrip("/"))
    prof = profile(body, heads, name)
    if args.json:
        print(json.dumps(prof, indent=2))
        return 0
    ref = json.load(open(args.against)) if args.against else None
    show([prof], ref)
    return 0


if __name__ == "__main__":
    sys.exit(main())

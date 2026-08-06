#!/usr/bin/env python3
"""Flag surface-level AI-writing tells in CaveWhere prose.

Handles Markdown (docs/), QML user-facing strings, and C++ tr()/QStringLiteral
strings. Reports two kinds of thing:

  findings  - a specific span worth looking at, with file:line
  metrics   - per-file numbers (em-dash rate, sentence-length variance, ...)
              that only mean something as a rate, so they are reported as one
              line per file rather than one finding per occurrence

This catches only the mechanical tells. The structural ones -- over-explaining,
single-track procedures, unnamed specifics -- do not reduce to regex and are
the reviewer's job. A clean run here does NOT mean the text reads human.

Usage:
    slopcheck.py docs/manual/**/*.md
    slopcheck.py --metrics-only docs/manual
    slopcheck.py --baseline docs/manual        # corpus percentiles, for tuning
"""

from __future__ import annotations

import argparse
import math
import os
import re
import statistics
import sys
from dataclasses import dataclass, field

# --- thresholds -------------------------------------------------------------
# Rates are per 1000 words. Calibrated against docs/manual/ so that a flag means
# "unusual for this corpus", not "unusual for English".

# Calibrated 2026-07-25 against three hand-written corpora (cavewhere.com blog,
# ICS 2013 paper, ICS 2021 paper: 10,240 words, two registers, eight years apart)
# and the largely AI-drafted docs/manual (50,490 words). See references/voice.md.
#   hand-written : em 0.0/1k, mean sentence 14.2-16.9w
#   docs/manual  : em 19.7/1k, mean sentence 20.9w
# Sentence-length *variance* is deliberately NOT a rule: the hand-written
# reference scores 6.0 and the manual 10.3, the opposite of the usual
# "humans are burstier" claim. It would flag the real voice as fake.

# The em dash is not banned -- it is a shout. The author uses one when a clause
# needs to be screamed, which is rare enough that 10,240 words of hand-written
# reference contain none (the only two dashes in that corpus are the numeric
# "2^31 - 1"). One per page is already generous; the manual runs ~20 per 1000
# words, all of them the routine appositive, which is the same as never
# shouting at all.
kEmDashRate = 1.0           # em dashes per 1k words

# TOBE is the one threshold NOT set from the hand-written corpus, and the
# temptation to "correct" it upward should be resisted. The author runs 29.6
# per 1k in the 2020 blog, 26.7 in the ICS 2013 paper and 24.7 in ICS 2021 --
# so matching him would mean targeting ~27, and docs/manual already sits at a
# median of 28.5. He is inside his own range and still reads tight, which means
# the copula is not what makes prose flabby. What does is the subset this rule
# is aimed at: existential "there is/are", passive with a recoverable agent,
# and copula-plus-nominalization ("is the groundwork for" hiding "lays").
# why-cavewhere.md was rewritten by hand against exactly those three and landed
# at 8.9 with its definitional copulas intact, so the target is a demonstrated
# floor rather than a guess. 15.0 leaves room above it before flagging.
kToBeRate = 15.0            # to-be verbs per 1k words; see above, NOT the corpus rate
kTripleRate = 3.5           # "a, b, and c" triples per 1k words
kBoldBulletRatio = 0.70     # share of bullets shaped "- **Header:** text"
kLexRate = 1.5              # AI-vocabulary hits per 1k words
kMeanSentence = 17.0        # words; hand-written reference is 13.6
kLongSentence = 35          # a single sentence this long is worth splitting
kLongParagraphWords = 120
kBurstMinSentences = 12

# --- rule tables ------------------------------------------------------------
# (regex, rule id, message). Fired once per match, every time.

kHardRules: list[tuple[str, str, str]] = [
    # Negative parallelism -- the single highest-signal construction.
    (r"\bnot (?:just|only|merely|simply)\b[^.!?\n]{0,80}?\bbut\b",
     "PAR", "Negative parallelism 'not just X but Y' -- state the claim once, positively."),
    (r"\b(?:is|are|was|were|it's|its|that's)\s+not\s+(?:about\s+)?\w[^.!?\n]{0,60}?[,;—–]\s*(?:it'?s|they'?re|but)\b",
     "PAR", "Negative parallelism 'not X, it's Y' -- drop the discarded half."),
    (r"\brather than (?:a |an |the )?\w+[,.]",
     "PAR", "'X rather than Y' framing -- say what it is, not what it isn't."),

    # Editorializing significance.
    (r"\b(?:plays?|played|playing) (?:an? )?(?:crucial|pivotal|vital|key|central|important|significant) role\b",
     "SIG", "'plays a X role' -- state the actual effect instead."),
    (r"\b(?:is|stands|serves) as a testament\b", "SIG", "'a testament to' -- pure editorializing."),
    (r"\b(?:marking|marks) an? (?:pivotal|key|important|significant|major) (?:moment|milestone|turning point|shift)\b",
     "SIG", "Significance editorializing."),
    (r"\bit(?:'s| is) (?:worth (?:noting|mentioning)|important to (?:note|remember|understand))\b",
     "SIG", "Throat-clearing -- delete the frame and keep the fact."),
    (r"\bindelible mark\b|\benduring legacy\b|\bbroader (?:landscape|context|implications)\b",
     "SIG", "Significance boilerplate."),

    # Purple / cliche.
    (r"\bunder the hood\b", "PURPLE", "'under the hood' -- name the actual subsystem."),
    (r"\bat (?:its|the) core\b|\bthe beauty of\b|\bthe magic (?:of|happens)\b",
     "PURPLE", "Cliche framing."),
    (r"\bseamless(?:ly)?\b|\beffortless(?:ly)?\b|\bwith ease\b",
     "PURPLE", "'seamless/effortless' -- unfalsifiable; give the actual cost or time."),
    (r"\bin (?:the blink of an eye|no time|a matter of (?:seconds|moments))\b",
     "PURPLE", "Vague speed claim -- give the real number."),
    (r"\bempower(?:s|ing|ed)?\b|\bunlock(?:s|ing) (?:the |your )?(?:power|potential|full)\b",
     "PURPLE", "Marketing verb."),

    # Chatbot register.
    (r"\b(?:let'?s|we'?ll|we can) (?:dive|delve|explore|take a look|walk through|get started)\b",
     "CHAT", "Chatbot register -- the manual is not a conversation with the reader's guide."),
    (r"\b(?:in (?:today's|this modern)|in the world of|when it comes to)\b",
     "CHAT", "Filler opener."),
    (r"\b(?:as (?:an AI|a language model)|my training data|knowledge cutoff)\b",
     "ARTIFACT", "Model self-reference leaked into the text."),

    # Outline-shaped conclusions.
    (r"^\s*#{1,6}\s*(?:Conclusion|Summary|Final Thoughts?|Key Takeaways?|Wrapping Up|Looking Ahead|Future (?:Outlook|Directions)|Challenges and)\b",
     "CLOSE", "Outline-shaped closing section -- a reference page rarely needs one."),
    (r"\b(?:In conclusion|In summary|To summarize|Overall,|Ultimately,|All in all)\b",
     "CLOSE", "Wrap-up phrase -- the reader just read it."),
    (r"\bby (?:understanding|following|leveraging|mastering) (?:these|the above|this)\b",
     "CLOSE", "Closing moral -- cut the sentence that tells the reader what they learned."),

    # Vague attribution / unnamed specifics. The strongest docs-specific tell.
    (r"\b(?:experts?|observers?|critics?|many users?|some(?: users| people)?) (?:say|argue|agree|note|suggest|believe|have (?:cited|noted))\b",
     "VAGUE", "Vague attribution -- name who, or delete."),
    (r"\bthe appropriate (?:file|setting|value|option|tool|format)\b",
     "VAGUE", "'the appropriate X' -- name it."),
    (r"\byour (?:survey|mapping|cave) (?:software|program|tool)\b",
     "VAGUE", "Name the actual program (Compass, Survex, Walls)."),
    (r"\ba (?:variety|number|range) of\b|\bvarious (?:settings|options|formats|tools|ways)\b",
     "VAGUE", "Unnamed set -- list them or give the count."),
    (r"\b(?:significantly|substantially|dramatically|considerably) (?:faster|slower|better|improves?|reduces?|increases?)\b",
     "VAGUE", "Unquantified comparison -- give the number or drop the adverb."),

    # LLM output artifacts.
    (r"contentReference|oaicite|turn\d+search\d+|\[cite:\s*\d+\]|\[span_\d+\]|grok_render|ppl-ai-file-upload",
     "ARTIFACT", "LLM markup artifact left in the text."),
    # Curly quotes are counted per file, not per span: Word and WordPress insert
    # them wholesale, so a converted document yields dozens of identical hits
    # that say one thing. See kCurlyRe below.
    (r"^\s*#{1,6}\s*[\U0001F300-\U0001FAFF☀-➿]", "ARTIFACT", "Emoji in a heading."),

    # is-avoidance.
    (r"\b(?:serves as|stands as|functions as|acts as|represents) (?:a|an|the)\b",
     "ISAV", "Inflated copula -- 'is' is usually the right word."),
    (r"\bboasts (?:a|an|\d)", "ISAV", "'boasts' -- promotional."),
]

# Words that only matter in aggregate. Counted, then reported as a rate.
kLexicon = [
    "delve", "tapestry", "testament", "pivotal", "crucial", "vibrant", "intricate",
    "realm", "landscape", "showcase", "showcases", "showcasing", "underscore",
    "underscores", "underscoring", "leverage", "leverages", "leveraging",
    "robust", "seamless", "holistic", "myriad", "plethora", "nuanced",
    "comprehensive", "foster", "fosters", "fostering", "cultivate", "cultivating",
    "navigate", "navigating", "embark", "elevate", "streamline", "streamlined",
    "cutting-edge", "state-of-the-art", "game-changer", "paradigm",
    "furthermore", "moreover", "additionally", "notably", "importantly",
    "essentially", "fundamentally", "arguably", "indeed",
]
kLexiconRe = re.compile(r"\b(" + "|".join(re.escape(w) for w in kLexicon) + r")\b", re.I)

# American English. CaveWhere is a US project and the hand-written reference
# corpus contains no British forms. Inflections are listed explicitly rather
# than derived, so there are no surprises ("programme" must not catch
# "programmer"; a general -ise rule would catch "advise", "concise", "revise").
# Deliberately absent: "amongst" (appears twice in the author's own writing and
# is acceptable in American English) and "towards" (style, not spelling).
kBritish: dict[str, str] = {
    "behaviour": "behavior", "behaviours": "behaviors", "behavioural": "behavioral",
    "judgement": "judgment", "judgements": "judgments",
    "neighbour": "neighbor", "neighbours": "neighbors",
    "neighbouring": "neighboring", "neighbourhood": "neighborhood",
    "colour": "color", "colours": "colors", "coloured": "colored", "colourful": "colorful",
    "centre": "center", "centres": "centers", "centred": "centered",
    "licence": "license", "licences": "licenses",
    "catalogue": "catalog", "catalogues": "catalogs",
    "grey": "gray", "greys": "grays", "greyed": "grayed", "greyscale": "grayscale",
    "analyse": "analyze", "analysed": "analyzed", "analysing": "analyzing",
    "organise": "organize", "organised": "organized", "organisation": "organization",
    "organisations": "organizations",
    "defence": "defense", "metre": "meter", "metres": "meters",
    "millimetre": "millimeter", "millimetres": "millimeters",
    "centimetre": "centimeter", "centimetres": "centimeters",
    "kilometre": "kilometer", "kilometres": "kilometers",
    "favourite": "favorite", "favourites": "favorites",
    "travelled": "traveled", "travelling": "traveling",
    "cancelled": "canceled", "cancelling": "canceling",
    "whilst": "while", "learnt": "learned", "spelt": "spelled",
    "programme": "program", "programmes": "programs",
    "realise": "realize", "realised": "realized",
    "recognise": "recognize", "recognised": "recognized",
    "optimise": "optimize", "optimised": "optimized", "optimisation": "optimization",
    "customise": "customize", "customised": "customized",
    "visualise": "visualize", "visualised": "visualized", "visualisation": "visualization",
    "utilise": "utilize", "utilised": "utilized",
    "initialise": "initialize", "initialised": "initialized",
    "normalise": "normalize", "normalised": "normalized",
    "labelled": "labeled", "labelling": "labeling",
    "modelled": "modeled", "modelling": "modeling",
    "fibre": "fiber", "artefact": "artifact", "artefacts": "artifacts",
    "dialogue": "dialog", "dialogues": "dialogs",
    "practise": "practice", "enquire": "inquire", "enquiry": "inquiry",
    "storey": "story", "storeys": "stories", "aluminium": "aluminum",
    "sulphur": "sulfur", "draught": "draft", "draughts": "drafts",
    "mould": "mold", "moulds": "molds", "kerb": "curb", "tyre": "tire",
}
kBritishRe = re.compile(r"\b(" + "|".join(sorted(kBritish, key=len, reverse=True)) + r")\b", re.I)

kCurlyRe = re.compile(r"[\u201c\u201d\u2018\u2019]")
kTripleRe = re.compile(r"\b\w+,\s+\w+,\s+and\s+\w+\b")
kToBeRe = re.compile(r"\b(?:is|are|was|were|be|been|being|am|isn['\u2019]t|aren['\u2019]t|"
                     r"wasn['\u2019]t|weren['\u2019]t|it['\u2019]s|that['\u2019]s|there['\u2019]s)\b", re.I)
# The three shapes worth naming in the report, because each has a mechanical
# fix. Everything else the TOBE rate catches needs a human to judge.
kToBeShapes: list[tuple[re.Pattern, str]] = [
    (re.compile(r"\bthere\s+(?:is|are|was|were)\b", re.I),
     "existential 'there is/are' (delete it, the sentence rarely misses it)"),
    (re.compile(r"\b(?:is|are|was|were)\s+\w+(?:ed|built|written|known|given|"
                r"taken|drawn|shown|kept|left|held|made|put|set)\b", re.I),
     "passive (name the subject: 'CaveWhere writes', not 'is written')"),
    (re.compile(r"\b(?:is|are|was|were)\s+(?:a|an|the)\s+\w+(?:ion|ment|ance|ence|"
                r"groundwork|catch|direction|reason|result|cause)\b", re.I),
     "copula + nominalization (the verb is hiding in the noun)"),
]
kBoldBulletRe = re.compile(r"^\s*[-*+]\s+\*\*[^*]+\*\*\s*[:—-]")
kBulletRe = re.compile(r"^\s*[-*+]\s+\S")
kHeadingRe = re.compile(r"^(#{1,6})\s+(.*)$")
kSentenceSplitRe = re.compile(r"(?<=[.!?])\s+")

kTitleCaseSkip = {
    "a", "an", "and", "as", "at", "but", "by", "for", "from", "in", "into", "nor",
    "of", "on", "or", "the", "to", "up", "via", "with", "your", "you",
}


@dataclass
class Finding:
    path: str
    line: int
    rule: str
    message: str
    excerpt: str


@dataclass
class Metrics:
    path: str
    words: int = 0
    em_dashes: int = 0
    triples: int = 0
    bullets: int = 0
    bold_bullets: int = 0
    lex_hits: int = 0
    curly: int = 0
    to_be: int = 0
    to_be_shapes: dict[str, int] = field(default_factory=dict)
    sentence_lengths: list[int] = field(default_factory=list)
    long_paragraphs: int = 0

    def rate(self, n: int) -> float:
        return (n * 1000.0 / self.words) if self.words else 0.0

    @property
    def burstiness(self) -> float | None:
        if len(self.sentence_lengths) < kBurstMinSentences:
            return None
        return statistics.pstdev(self.sentence_lengths)


# --- extraction -------------------------------------------------------------

kFenceRe = re.compile(r"^\s*(```|~~~)")
kInlineCodeRe = re.compile(r"`[^`]*`")
kLinkRe = re.compile(r"!?\[([^\]]*)\]\([^)]*\)")
kHtmlTagRe = re.compile(r"<[^>]+>")
# QML user-facing strings. An earlier version keyed only off `text:`-style
# property names and so missed `toolTip:`, `return qsTr(...)` and every
# `qsTr(...).arg(...)` chain -- which is most of the app's dynamic messages.
# Match qsTr() wherever it appears, plus untranslated assignments to the
# properties users actually read.
kQmlStringRes = [
    re.compile(r'\bqsTr\(\s*"((?:[^"\\]|\\.){15,})"'),
    re.compile(r"\b(?:text|toolTip|toolTipText|description|summary|placeholderText"
               r"|title|label|message|hint)\s*:\s*\"((?:[^\"\\]|\\.){20,})\""),
]
kCppStringRes = [
    re.compile(r'\b(?:tr|QObject::tr|QStringLiteral)\(\s*"((?:[^"\\]|\\.){20,})"'),
]


def extract_markdown(text: str) -> list[tuple[int, str]]:
    """Return (1-based line number, prose) pairs with code and markup stripped."""
    out: list[tuple[int, str]] = []
    in_fence = False
    in_front_matter = False
    lines = text.splitlines()
    for i, raw in enumerate(lines, start=1):
        if i == 1 and raw.strip() == "---":
            in_front_matter = True
            continue
        if in_front_matter:
            if raw.strip() == "---":
                in_front_matter = False
            elif re.match(r"^\s*(summary|problem|title)\s*:", raw):
                out.append((i, re.sub(r"^\s*\w+\s*:\s*", "", raw)))
            continue
        if kFenceRe.match(raw):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        line = kInlineCodeRe.sub(" ", raw)
        line = kLinkRe.sub(r"\1", line)
        line = kHtmlTagRe.sub(" ", line)
        out.append((i, line))
    return out


def extract_strings(text: str, patterns: list[re.Pattern]) -> list[tuple[int, str]]:
    """Merge matches from several patterns, de-duplicating overlapping hits."""
    seen: dict[tuple[int, str], None] = {}
    for pattern in patterns:
        for m in pattern.finditer(text):
            line = text.count("\n", 0, m.start()) + 1
            body = kHtmlTagRe.sub(" ", m.group(1).replace("\\n", " ").replace('\\"', '"'))
            seen.setdefault((line, body), None)
    return sorted(seen)


def extract(path: str) -> list[tuple[int, str]] | None:
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            text = fh.read()
    except OSError:
        return None
    ext = os.path.splitext(path)[1].lower()
    if ext in (".md", ".markdown"):
        return extract_markdown(text)
    if ext == ".qml":
        return extract_strings(text, kQmlStringRes)
    if ext in (".cpp", ".cc", ".cxx", ".h", ".hpp", ".mm"):
        return extract_strings(text, kCppStringRes)
    return None


# --- analysis ---------------------------------------------------------------

def is_title_case_heading(title: str) -> bool:
    words = re.findall(r"[A-Za-z][A-Za-z'\-]*", title)
    if len(words) < 4:
        return False
    capitalized = sum(
        1 for w in words[1:]
        if w[0].isupper() and w.lower() not in kTitleCaseSkip and not w.isupper()
    )
    eligible = sum(1 for w in words[1:] if w.lower() not in kTitleCaseSkip and not w.isupper())
    return eligible >= 3 and capitalized == eligible


def analyze(path: str, prose: list[tuple[int, str]],
            is_doc: bool = True) -> tuple[list[Finding], Metrics]:
    """is_doc=False for extracted UI strings, where paragraph shape is an
    artifact of concatenation rather than something the author chose."""
    findings: list[Finding] = []
    metrics = Metrics(path=path)
    compiled = [(re.compile(rx, re.I | re.M), rid, msg) for rx, rid, msg in kHardRules]

    # Phrase rules run over the whole prose, not line by line. Markdown here is
    # hard-wrapped at ~80 columns, so "Under the hood" lands with "Under" ending
    # one line and "the hood" starting the next; a per-line scan silently misses
    # it and reports a clean file. Offsets are mapped back to line numbers.
    flat_parts: list[str] = []
    line_at: list[tuple[int, int]] = []   # (char offset in flat, line number)
    pos = 0
    for lineno, line in prose:
        line_at.append((pos, lineno))
        flat_parts.append(line)
        pos += len(line) + 1
    flat = "\n".join(flat_parts)
    # A wrapped phrase must match, so newlines read as spaces -- but ^ anchors
    # in heading rules still need real line starts, hence re.M on the original.
    flat_spaced = flat.replace("\n", " ")

    def line_of(offset: int) -> int:
        lo, hi = 0, len(line_at) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if line_at[mid][0] <= offset:
                lo = mid
            else:
                hi = mid - 1
        return line_at[lo][1]

    for rx, rid, msg in compiled:
        target = flat if rx.pattern.startswith("^") else flat_spaced
        for m in rx.finditer(target):
            excerpt = " ".join(
                target[max(0, m.start() - 20):m.end() + 20].split())
            findings.append(Finding(path, line_of(m.start()), rid, msg, excerpt))

    para: list[str] = []
    for lineno, line in prose:
        if not is_doc and "\u2014" in line:
            findings.append(Finding(
                path, lineno, "EMDASH",
                "Em dash in a UI string. Too short to be emphasis, so this is the "
                "appositive habit -- use a colon, a period, or parentheses.",
                line.strip()[:80]))

        for m in kBritishRe.finditer(line):
            found = m.group(1)
            fix = kBritish[found.lower()]
            if found[0].isupper():
                fix = fix.capitalize()
            findings.append(Finding(
                path, lineno, "BRIT",
                f"British spelling '{found}' -- American English is '{fix}'.",
                line[max(0, m.start() - 20):m.end() + 20].strip()))

        heading = kHeadingRe.match(line)
        if heading and is_title_case_heading(heading.group(2)):
            findings.append(Finding(
                path, lineno, "FMT",
                "Title Case heading -- CaveWhere uses sentence case for section headings.",
                heading.group(2).strip()))

        metrics.words += len(re.findall(r"\b[\w'-]+\b", line))
        metrics.em_dashes += line.count("—")
        metrics.triples += len(kTripleRe.findall(line))
        metrics.lex_hits += len(kLexiconRe.findall(line))
        metrics.curly += len(kCurlyRe.findall(line))
        metrics.to_be += len(kToBeRe.findall(line))
        for rx, label in kToBeShapes:
            n = len(rx.findall(line))
            if n:
                metrics.to_be_shapes[label] = metrics.to_be_shapes.get(label, 0) + n
        if kBulletRe.match(line):
            metrics.bullets += 1
            if kBoldBulletRe.match(line):
                metrics.bold_bullets += 1

        if line.strip():
            para.append(line.strip())
        else:
            if para:
                metrics.long_paragraphs += _flush_paragraph(" ".join(para), metrics)
            para = []
    if para:
        metrics.long_paragraphs += _flush_paragraph(" ".join(para), metrics)
    if not is_doc:
        metrics.long_paragraphs = 0

    return findings, metrics


def _flush_paragraph(text: str, metrics: Metrics) -> int:
    if text.lstrip().startswith(("#", "|", ">")):
        return 0
    for sentence in kSentenceSplitRe.split(text):
        n = len(re.findall(r"\b[\w'-]+\b", sentence))
        if n >= 3:
            metrics.sentence_lengths.append(n)
    return 1 if len(re.findall(r"\b[\w'-]+\b", text)) > kLongParagraphWords else 0


def metric_findings(m: Metrics) -> list[str]:
    notes: list[str] = []
    if m.words < 120:
        return notes
    if m.rate(m.em_dashes) > kEmDashRate:
        notes.append(f"EMDASH  {m.em_dashes} em dashes ({m.rate(m.em_dashes):.1f}/1k words). "
                     f"An em dash is a shout, so at most about one per page earns it. "
                     f"Keep the single most emphatic; the rest become commas, periods or parens.")
    if m.rate(m.to_be) > kToBeRate:
        note = (f"TOBE    {m.to_be} to-be verbs ({m.rate(m.to_be):.1f}/1k words). "
                f"Aim under {kToBeRate:.0f}; why-cavewhere.md reaches 8.9. Keep the "
                f"definitional ones ('a cave survey is a team effort') and cut these:")
        for label, n in sorted(m.to_be_shapes.items(), key=lambda kv: -kv[1]):
            note += f"\n                 {n:>3}x {label}"
        notes.append(note)
    if m.rate(m.triples) > kTripleRate:
        notes.append(f"TRIPLE  {m.triples} 'a, b, and c' triples ({m.rate(m.triples):.1f}/1k words) "
                     f"-- the rule of three is a reflex, not a finding. Break some into two or four.")
    if m.curly:
        notes.append(f"CURLY   {m.curly} curly quote(s) -- use straight quotes in source. "
                     f"Usually a paste from Word or a web page, not something anyone typed.")
    if m.rate(m.lex_hits) > kLexRate:
        notes.append(f"LEX     {m.lex_hits} AI-vocabulary words ({m.rate(m.lex_hits):.1f}/1k words).")
    if m.bullets >= 6 and m.bold_bullets / m.bullets > kBoldBulletRatio:
        notes.append(f"BULLET  {m.bold_bullets}/{m.bullets} bullets are '- **Header:** text' "
                     f"-- the shape is a template. Let some bullets just be sentences.")
    if len(m.sentence_lengths) >= kBurstMinSentences:
        mean = statistics.mean(m.sentence_lengths)
        if mean > kMeanSentence:
            notes.append(f"LONG    mean sentence {mean:.1f} words over {len(m.sentence_lengths)} sentences "
                         f"-- the hand-written reference runs 13.6. Break the long ones in two.")
        overlong = sum(1 for n in m.sentence_lengths if n > kLongSentence)
        if overlong:
            notes.append(f"LONG    {overlong} sentence(s) over {kLongSentence} words.")
    if m.long_paragraphs:
        notes.append(f"PARA    {m.long_paragraphs} paragraph(s) over {kLongParagraphWords} words.")
    return notes


# --- driver -----------------------------------------------------------------

kExts = {".md", ".markdown", ".qml", ".cpp", ".cc", ".cxx", ".h", ".hpp", ".mm"}


def walk(targets: list[str]) -> list[str]:
    out: list[str] = []
    for t in targets:
        if os.path.isdir(t):
            for root, dirs, files in os.walk(t):
                # _site is build output from scripts/build-manual-html.py; it
                # carries a copy of images/illustrations/README.md, which would
                # otherwise be scanned twice.
                dirs[:] = [d for d in dirs
                           if d not in (".git", "build", "node_modules", "_site")]
                for f in sorted(files):
                    if os.path.splitext(f)[1].lower() in kExts:
                        out.append(os.path.join(root, f))
        elif os.path.splitext(t)[1].lower() in kExts:
            out.append(t)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("targets", nargs="+", help="files or directories")
    ap.add_argument("--metrics-only", action="store_true", help="skip per-span findings")
    ap.add_argument("--baseline", action="store_true",
                    help="print corpus-wide percentiles instead of findings (for tuning)")
    args = ap.parse_args()

    paths = walk(args.targets)
    if not paths:
        print("slopcheck: no supported files found", file=sys.stderr)
        return 2

    all_findings: list[Finding] = []
    all_metrics: list[Metrics] = []
    for p in paths:
        prose = extract(p)
        if not prose:
            continue
        f, m = analyze(p, prose, is_doc=os.path.splitext(p)[1].lower() in (".md", ".markdown"))
        all_findings.extend(f)
        all_metrics.append(m)

    if args.baseline:
        return print_baseline(all_metrics)

    shown = 0
    for m in all_metrics:
        fs = [f for f in all_findings if f.path == m.path] if not args.metrics_only else []
        notes = metric_findings(m)
        if not fs and not notes:
            continue
        shown += 1
        print(f"\n{m.path}  ({m.words} words)")
        for f in sorted(fs, key=lambda x: x.line):
            print(f"  {f.line:>5}  [{f.rule}] {f.message}")
            print(f"         ...{f.excerpt}...")
        for n in notes:
            print(f"  file   {n}")

    total = len(all_findings)
    print(f"\n{'-' * 70}")
    print(f"{len(paths)} file(s) scanned, {shown} with something to look at, {total} span finding(s).")
    print("Mechanical tells only. Structural tells (over-explaining, single-track")
    print("procedures, unnamed specifics) need the reviewer -- see SKILL.md.")
    return 1 if (total or shown) else 0


def print_baseline(metrics: list[Metrics]) -> int:
    def pct(vals: list[float], q: float) -> float:
        if not vals:
            return 0.0
        vals = sorted(vals)
        k = (len(vals) - 1) * q
        lo, hi = math.floor(k), math.ceil(k)
        return vals[lo] if lo == hi else vals[lo] * (hi - k) + vals[hi] * (k - lo)

    usable = [m for m in metrics if m.words >= 120]
    print(f"corpus: {len(usable)} files, {sum(m.words for m in usable)} words\n")
    series = {
        "em dash /1k": [m.rate(m.em_dashes) for m in usable],
        "triples /1k": [m.rate(m.triples) for m in usable],
        "lexicon /1k": [m.rate(m.lex_hits) for m in usable],
        "burstiness ": [m.burstiness for m in usable if m.burstiness is not None],
        "mean sent  ": [statistics.mean(m.sentence_lengths) for m in usable if m.sentence_lengths],
        "p90 sent   ": [pct([float(x) for x in m.sentence_lengths], .90) for m in usable if m.sentence_lengths],
        "bold-bullet": [m.bold_bullets / m.bullets for m in usable if m.bullets >= 6],
    }
    print(f"{'metric':<14}{'p10':>8}{'p50':>8}{'p75':>8}{'p90':>8}{'max':>8}")
    for name, vals in series.items():
        if not vals:
            continue
        print(f"{name:<14}{pct(vals, .10):>8.2f}{pct(vals, .50):>8.2f}"
              f"{pct(vals, .75):>8.2f}{pct(vals, .90):>8.2f}{max(vals):>8.2f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

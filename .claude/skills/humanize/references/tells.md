# Catalog of AI-writing tells

Two sources, doing different jobs.

**Wikipedia, [Signs of AI writing](https://en.wikipedia.org/wiki/Wikipedia:Signs_of_AI_writing)** —
an editor-maintained list of surface patterns. Cheap to check, mostly
regex-able, and mostly already caught by `slopcheck.py`.

**[StoryScope](https://jenna-russell.github.io/assets/pdf/storyscope.pdf)** (Russell,
Rajendhran, Pham, Iyyer, Wieting — UMD / Google DeepMind, preprint) — a pipeline
that extracts 304 discourse-level narrative features from 61,608 stories written
to 10,272 shared prompts by one human author and five LLMs, then trains
classifiers on them. Its finding is that *narrative* features alone hit 93.2%
macro-F1 at human-vs-AI detection with every stylistic feature withheld —
retaining 97% of a model that also sees style. Those are the durable tells.

## Why the surface list is not enough

StoryScope tested this directly. They ran Chakrabarty et al.'s LAMP span-level
rewriter over 278 Gemini stories — it targets seven categories of AI artifact
(cliché, redundant exposition, purple prose among them) using 25 few-shot
examples from professional writers. Detection went from 95.5% to 93.9%
macro-F1. **A 1.6-point drop.** The paper's conclusion: "editing out clichéd
phrasing or purple prose does not alter the structural narrative choices…
that drive our classifier."

Separately, AI style is a moving target — the paper notes GPT 5.4 cut em-dash
usage sharply after it became a known tell, and fine-tuning drops detection
rates on creative writing from 97% to 3%. Word lists rot. Structure doesn't.

For CaveWhere specifically, the surface list is close to worthless: the
AI-vocabulary rate is **0.0 per 1k words in both** the hand-written blog and the
AI-drafted manual. See `voice.md`.

## Structural tells (StoryScope) — check these first

Percentages are AI vs human across the 61,608-story corpus.

| Finding | Paper's numbers | In CaveWhere docs |
|---|---|---|
| **Over-explains the theme** | Narrator states the story's theme 77% vs 52%; more explicit and moralizing (~20% higher on a 1–5 scale); "AI spells out meaning rather than trusting the reader to infer it" | The sentence after the useful one. "This ensures…", "By understanding these settings…". Summary sections. |
| **Tidy single-track causality** | 79% have no subplots vs 57%; tighter causal chains; protagonist-driven resolutions 69% vs 46% | A procedure with no branch, no prerequisite, no partial failure, no known bug. Real work is messy. |
| **Over-writes body and senses** | Conveys state via physical sensation/metaphor 81% vs 38%; humans use the explicit plain label 29% vs 8% | "Your work is safe with us" instead of "the save failed". "In the blink of an eye" instead of "about 45 s". |
| **Won't name real things** | Humans reference specific named texts/authors at 47% vs 24%; AI "avoids naming real brands, places, or works" | "your survey software" not "Compass". "the appropriate file" not "cave.cw". Missing versions, numbers, menu paths. |
| **Doesn't address the reader** | Humans break the fourth wall 67% vs 39% and address the reader directly 28% vs 7%; "AI writes as though no one is watching" | No "you", no aside, no admission that a step is annoying. |
| **Narrow repertoire** | Human stories rarer in feature space (0.71 vs 0.49 mean rarity percentile); humans span more locations, integrate subplots 42% vs 21% | Template lock: every tooltip the same frame, every bullet `- **Header:** text`, every list three long. |
| **Resolves into acceptance** | AI resolutions favor internal understanding or acceptance 47% vs 27%; humans comfortable with ambiguous endings | Ending on reassurance instead of an action. Refusing to write "we don't know why this happens yet". |

### Claude's own fingerprint

StoryScope profiles each model separately. Claude is the most distinctive of the
five, and the trait is **restraint**:

> "Its stories are defined by its restraint: event intensity escalates less than
> in any other source, and narrative voice is the most uniform. Claude takes a
> reverent/continuist approach to literary tradition, honoring and extending
> storytelling conventions rather than subverting or challenging them (62% of
> Claude stories vs. 39–56% across other sources). It favors epilogues and avoids
> dream sequences, producing careful, consistent stories that favor quiet endings
> over 'avalanche' endings."

Translated to documentation: everything comes out at the same mild, even pitch;
nothing is flagged as genuinely dangerous; the page ends with a tidy wrap-up
rather than stopping. Conventions get followed rather than questioned. Watch for
all four when reviewing your own drafts.

For contrast, the other fingerprints: GPT centers gossip and rumor (64% vs
44–55%) and subverts expectations more (41% vs 27–36%); Gemini produces the
tidiest endings, extended denouements, and the bleakest settings (88% bleak and
oppressive); DeepSeek front-loads crucial context; Kimi has no distinctive
choices at all, sitting at the generic center.

## Surface tells (Wikipedia) — rule ids in slopcheck.py

**`PAR` — negative parallelism.** The highest-signal single construction.
Three shapes: "not just X, but also Y"; "It's not X, it's Y"; "X rather than Y".

**`SIG` — editorializing significance.** "stands as", "serves as", "is a
testament to", "plays a crucial/pivotal/vital role", "marking a pivotal moment",
"a key turning point", "left an indelible mark", "reflects broader", "it's worth
noting that", "it is important to note".

**`LEX` — AI vocabulary** (rate-based; near-zero in this project). delve,
tapestry, testament, pivotal, crucial, vibrant, intricate, realm, landscape,
showcase, underscore, leverage, robust, holistic, myriad, plethora, nuanced,
comprehensive, foster, cultivate, navigate, embark, elevate, streamline,
cutting-edge, paradigm; and sentence-initial Additionally / Moreover /
Furthermore / Notably / Importantly / Essentially / Fundamentally.

**`ISAV` — copula avoidance.** Replacing "is" with serves as, stands as,
functions as, acts as, represents, boasts, features, maintains, offers.

**`TRI` — rule of three** (rate-based). Three adjectives, or three parallel
short phrases, reached for as a rhythm rather than because there are three
things. Hand-written reference: 0.00 per 1k.

**`VAGUE` — weasel attribution and unnamed specifics.** "Experts argue",
"Observers have cited", "Industry reports", "Some critics argue", "many users";
plus "the appropriate setting", "a variety of", "various options",
"significantly faster". Advisory — this rule misfires more than the others.

**`PURPLE` — cliché and marketing.** "under the hood", "at its core", "the
beauty of", "seamlessly", "effortlessly", "with ease", "in the blink of an eye",
"empower", "unlock the potential".

**`CHAT` — assistant register.** "Let's dive in", "we'll explore", "let's take a
look", "In today's world", "When it comes to". Also knowledge-cutoff
disclaimers and any "as an AI" self-reference.

**`CLOSE` — outline-shaped conclusions.** Headings named Conclusion, Summary,
Key Takeaways, Final Thoughts, Future Outlook, Challenges and Legacy. Phrases
"In conclusion", "Overall,", "Ultimately,", "By understanding these…". Wikipedia
notes the specific formula: acknowledge challenges, then speculate optimistically.

**`FMT` — formatting.** Title Case headings; every bullet shaped
`- **Header:** text`; boldface on every instance of a term; tables where prose
belongs; skipped heading levels; thematic breaks before headings.

**`EMDASH` — em-dash rate** (rate-based). The dominant measurable tell in this
project: 18.7 per 1k words in `docs/manual/` against 0.0 in the hand-written
blog. Highest-yield single fix available.

**`LONG` — sentence length** (rate-based). Manual mean 18.8 words, hand-written
mean 13.6; p90 32.8 against 19.5.

**`TOBE` — to-be verb rate** (rate-based). **Not an AI tell, and the only
threshold here that ignores the corpus.** Philip runs 29.6 / 26.7 / 24.7 per 1k
across the blog and the two papers, and `docs/manual/` sits at a median of 28.5 —
he is inside his own range and still reads tight. So this rule exists for
conciseness, at his request, not to separate human from machine. Threshold 15.0;
`why-cavewhere.md` was rewritten by hand to 8.9. The report breaks the count into
the three fixable shapes (existential, passive with a recoverable agent, copula +
nominalization) and deliberately says nothing about the rest, because
definitional copulas and rhetorical parallels are load-bearing. Do not raise the
threshold toward the corpus rate; see the comment on `kToBeRate`.

Where the manual stands, worst first: `survey-errors.md` 61.0,
`set-up-your-identity.md` 50.5, `caves-and-trips.md` 47.3, `export-surveys.md`
46.4, `declination.md` 45.5, `save-a-project.md` 45.5, `data-model.md` 45.4,
`coordinate-systems.md` 42.7. That is the work queue; 45 of 48 pages are over.

**`ARTIFACT` — generator leakage.** ChatGPT: `contentReference`, `oaicite`,
`turn0search0`. Gemini: `[cite: 1]`, `[span_1]`. Grok: `grok_render_citation_card_json`.
Perplexity: `ppl-ai-file-upload`. Also curly quotes where the codebase uses
straight ones, emoji as heading decoration, and Markdown syntax pasted where the
target format is something else.

## Deliberately not a rule

**Sentence-length variance / "burstiness".** The folk heuristic says humans vary
sentence length more. Measured here, the hand-written blog scores 6.0 and the
AI-drafted manual 10.3 — backwards. A burstiness rule would flag Philip's real
voice as machine-written. Left out on purpose; see `voice.md`.

Wikipedia's own list of weak indicators applies too: grammar and syntax alone
vary by model; a single AI-vocabulary word means nothing; promotional tone
without other markers is just marketing; and automated AI-detector scores are not
evidence.

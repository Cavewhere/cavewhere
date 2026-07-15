#!/usr/bin/env python3
"""Check the manual's internal links, anchors, and index sync.

Implements Verification 3 of plans/USER_MANUAL_PLAN.md. Every manual page links
to others with relative paths, and AUTHORING.md forbids linking to a page that
does not exist yet — this is what enforces both. It also holds the two index
files to their promise: a page missing from llms.txt is invisible to the AI
agents the manual is written for.

Checks, over every .md under docs/manual/ (excluding images/ and _site/):

  1. Every relative link and image target resolves to a file on disk.
  2. Every #anchor resolves to a real heading in the target page (or this page,
     for a bare #anchor), using GitHub's heading->slug rules.
  3. Every `related:` front-matter entry resolves (AUTHORING.md: "List only
     files that exist").
  4. Every page is listed in llms.txt.

Run from the repo root:  python3 scripts/check-manual-links.py
Exits non-zero and prints file:line for each problem.
"""

import os
import re
import sys

ROOT = "docs/manual"

# Pages that are indexes/meta rather than manual content, so llms.txt does not
# have to list them (it *is* one of them).
UNINDEXED_PAGES = ("index.md", "AUTHORING.md")

# Directories under ROOT holding assets rather than pages.
NON_PAGE_DIRS = ("images", "_site")

LINK_RE = re.compile(r"(?<!\!)\[[^\]]+\]\(([^)]+)\)")
IMG_RE = re.compile(r"!\[[^\]]*\]\(([^)]+)\)")
HEADING_RE = re.compile(r"#{1,6}\s+(.*)")
INLINE_CODE_RE = re.compile(r"`[^`]*`")


def find_pages(root):
    pages = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in NON_PAGE_DIRS]
        pages.extend(os.path.join(dirpath, f)
                     for f in filenames if f.endswith(".md"))
    return sorted(pages)


def iter_prose_lines(path):
    """Yield (lineno, line) outside fenced code blocks, with inline code stripped.

    Fences and inline code hold example markdown (AUTHORING.md documents the
    link syntax by showing it), which must not be checked as real links.
    """
    in_fence = False
    with open(path, encoding="utf-8") as handle:
        for lineno, line in enumerate(handle, 1):
            if line.strip().startswith("```"):
                in_fence = not in_fence
                continue
            if in_fence:
                continue
            yield lineno, INLINE_CODE_RE.sub("", line)


def anchors_of(path, _cache={}):
    """The set of anchor slugs a page exposes, by GitHub's heading rules."""
    if path in _cache:
        return _cache[path]
    anchors = set()
    for _, line in iter_prose_lines(path):
        match = HEADING_RE.match(line)
        if match:
            slug = match.group(1).strip().lower()
            slug = re.sub(r"[^\w\s-]", "", slug)
            anchors.add(re.sub(r"\s+", "-", slug))
    _cache[path] = anchors
    return anchors


def front_matter_related(path):
    """The `related:` entries from a page's YAML front matter, if any."""
    with open(path, encoding="utf-8") as handle:
        text = handle.read()
    if not text.startswith("---\n"):
        return []
    end = text.find("\n---", 4)
    if end == -1:
        return []
    match = re.search(r"^related:\s*\[(.*?)\]", text[4:end], re.MULTILINE | re.DOTALL)
    if not match:
        return []
    return [entry.strip().strip("'\"")
            for entry in match.group(1).split(",") if entry.strip()]


def check_target(page, lineno, target, errors):
    """Verify one link target: its file exists and its anchor is real."""
    if target.startswith(("http://", "https://", "mailto:")):
        return
    filepart, _, anchor = target.partition("#")
    if not filepart:
        if anchor and anchor not in anchors_of(page):
            errors.append(f"{page}:{lineno}: missing anchor #{anchor}")
        return
    resolved = os.path.normpath(os.path.join(os.path.dirname(page), filepart))
    if not os.path.exists(resolved):
        errors.append(f"{page}:{lineno}: missing file {target} -> {resolved}")
    elif anchor and resolved.endswith(".md") and anchor not in anchors_of(resolved):
        errors.append(f"{page}:{lineno}: missing anchor {target}")


def main():
    if not os.path.isdir(ROOT):
        print(f"error: {ROOT} not found — run from the repo root", file=sys.stderr)
        return 2

    pages = find_pages(ROOT)
    errors = []

    for page in pages:
        for lineno, line in iter_prose_lines(page):
            for match in list(LINK_RE.finditer(line)) + list(IMG_RE.finditer(line)):
                check_target(page, lineno, match.group(1).strip(), errors)
        for entry in front_matter_related(page):
            resolved = os.path.normpath(os.path.join(os.path.dirname(page), entry))
            if not os.path.exists(resolved):
                errors.append(f"{page}: related: missing file {entry} -> {resolved}")

    with open(os.path.join(ROOT, "llms.txt"), encoding="utf-8") as handle:
        llms = handle.read()
    for page in pages:
        rel = os.path.relpath(page, ROOT)
        if rel not in UNINDEXED_PAGES and rel not in llms:
            errors.append(f"llms.txt: page not listed: {rel}")

    if errors:
        print("\n".join(errors))
        print(f"\n{len(errors)} problem(s)")
        return 1
    print(f"OK — checked {len(pages)} pages, links, anchors, and llms.txt sync")
    return 0


if __name__ == "__main__":
    sys.exit(main())

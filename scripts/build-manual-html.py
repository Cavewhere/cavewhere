#!/usr/bin/env python3
"""Build a single self-contained HTML view of the CaveWhere manual.

The Markdown files under docs/manual/ are the source of truth. This script
converts them (via pandoc) into ONE standalone HTML file you can open directly
in a browser (double-click, or `open`): images are embedded as data URIs, all
CSS/JS is inline, and cross-file .md links become in-page anchors. Nav order and
titles come from docs/manual/llms.txt, so there is no second index to maintain.

Usage:
  scripts/build-manual-html.py [output.html]

Default output: docs/manual/_site/manual.html
Requires: pandoc on PATH.
"""

import base64
import os
import re
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(__file__), "..", "docs", "manual")
ROOT = os.path.normpath(ROOT)


def slug_key(relpath):
    """Stable per-file id namespace, e.g. concepts/why-cavewhere.md -> concepts-why-cavewhere."""
    base = re.sub(r"\.md$", "", relpath)
    return re.sub(r"[^a-z0-9]+", "-", base.lower()).strip("-")


# The chapter a page belongs to is its directory. These titles mirror the chapter
# headings in index.md so the sidebar reads as the same table of contents. A
# directory not listed here falls back to its name title-cased; a root-level file
# (AUTHORING.md, index.md) has no directory and lands in "Meta".
CHAPTER_TITLES = {
    "concepts": "Concepts",
    "getting-started": "Getting Started",
    "projects-and-files": "Projects & Files",
    "view-3d": "3D View",
    "survey-data": "Survey Data",
    "notes": "Notes",
    "scraps": "Scraps and Carpeting",
    "import-export": "Import & Export",
    "": "Meta",
}


def chapter_title(relpath):
    """The sidebar chapter a page is grouped under, derived from its directory."""
    directory = relpath.split("/", 1)[0] if "/" in relpath else ""
    return CHAPTER_TITLES.get(directory, directory.replace("-", " ").title() or "Meta")


def parse_llms():
    """Ordered [(section, title, relpath)] parsed from llms.txt."""
    entries = []
    section = "Manual"
    path = os.path.join(ROOT, "llms.txt")
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            header = re.match(r"^##\s+(.*)", line)
            if header:
                section = header.group(1).strip()
                continue
            item = re.match(r"^-\s+\[([^\]]+)\]\(([^)]+)\):", line)
            if item:
                entries.append((section, item.group(1).strip(), item.group(2).strip()))
    return entries


def strip_front_matter(text):
    return re.sub(r"^---\n.*?\n---\n", "", text, count=1, flags=re.DOTALL)


def pandoc_fragment(body, key):
    result = subprocess.run(
        ["pandoc", "--from", "gfm", "--to", "html", "--id-prefix", key + "--"],
        input=body, capture_output=True, text=True, check=True,
    )
    return result.stdout


def embed_images(html, page_dir):
    """Replace <img src="local.png"> with an inline data URI so the page is self-contained."""
    def repl(match):
        src = match.group(1)
        if src.startswith(("http:", "https:", "data:")):
            return match.group(0)
        img_path = os.path.normpath(os.path.join(ROOT, page_dir, src))
        if not os.path.exists(img_path):
            print(f"  warning: missing image {src} -> {img_path}", file=sys.stderr)
            return match.group(0)
        ext = os.path.splitext(img_path)[1].lstrip(".").lower()
        # "image/svg" is not a real type — browsers reject the data URI and show
        # a broken image, so SVG needs its +xml suffix spelled out.
        mime = {"jpg": "image/jpeg", "jpeg": "image/jpeg",
                "svg": "image/svg+xml"}.get(ext, f"image/{ext}")
        with open(img_path, "rb") as handle:
            data = base64.b64encode(handle.read()).decode("ascii")
        return f'src="data:{mime};base64,{data}"'

    return re.sub(r'src="([^"]+)"', repl, html)


def rewrite_links(html, relpath, key_by_relpath):
    """Turn cross-file .md links into in-page anchors (#pagekey or #pagekey--anchor)."""
    page_dir = os.path.dirname(relpath)

    def repl(match):
        target, anchor = match.group(1), match.group(2) or ""
        if target.startswith(("http:", "https:", "mailto:")):
            return match.group(0)
        resolved = os.path.normpath(os.path.join(page_dir, target))
        key = key_by_relpath.get(resolved)
        if not key:
            return match.group(0)  # link to a page not in this artifact; leave it
        frag = f"--{anchor[1:]}" if anchor else ""
        return f'href="#{key}{frag}"'

    return re.sub(r'href="([^"#]+\.md)(#[^"]*)?"', repl, html)


def build():
    entries = parse_llms()
    key_by_relpath = {relpath: slug_key(relpath) for _, _, relpath in entries}

    chapters = []  # [(chapter, [(title, key)])]
    pages_html = []
    for section, title, relpath in entries:
        key = key_by_relpath[relpath]
        with open(os.path.join(ROOT, relpath), encoding="utf-8") as handle:
            body = strip_front_matter(handle.read())
        frag = pandoc_fragment(body, key)
        frag = rewrite_links(frag, relpath, key_by_relpath)
        frag = embed_images(frag, os.path.dirname(relpath))
        pages_html.append(f'<section id="{key}" class="page">\n{frag}\n</section>')

        chapter = chapter_title(relpath)
        if not chapters or chapters[-1][0] != chapter:
            chapters.append((chapter, []))
        chapters[-1][1].append((title, key))

    nav_html = []
    for chapter, items in chapters:
        links = "\n".join(
            f'      <a href="#{key}">{title}</a>' for title, key in items
        )
        nav_html.append(
            f'    <div class="nav-group">\n'
            f'      <p class="nav-label">{chapter}</p>\n{links}\n    </div>'
        )
    nav = "\n".join(nav_html)
    main = "\n\n".join(pages_html)

    return PAGE_TEMPLATE.format(css=CSS, nav=nav, main=main, script=SCRIPT)


CSS = """
:root {
  --paper: #f3f5f7;
  --surface: #ffffff;
  --surface-2: #eef1f4;
  --ink: #171b20;
  --muted: #5c656e;
  --border: #dbe1e6;
  --accent: #17618f;
  --accent-soft: #dcebf5;
  --mark: #d6552b;
  --code-bg: #eef1f4;
  --shadow: 0 1px 2px rgba(20,30,40,.06), 0 8px 24px rgba(20,30,40,.06);
}
@media (prefers-color-scheme: dark) {
  :root {
    --paper: #0e1216; --surface: #161b21; --surface-2: #1b2129;
    --ink: #e7ecf1; --muted: #9aa4ae; --border: #2a323a;
    --accent: #7cc0ee; --accent-soft: #16303f; --mark: #ff8a5c;
    --code-bg: #1b2129;
    --shadow: 0 1px 2px rgba(0,0,0,.4), 0 10px 30px rgba(0,0,0,.35);
  }
}
:root[data-theme="light"] {
  --paper: #f3f5f7; --surface: #ffffff; --surface-2: #eef1f4;
  --ink: #171b20; --muted: #5c656e; --border: #dbe1e6;
  --accent: #17618f; --accent-soft: #dcebf5; --mark: #d6552b; --code-bg: #eef1f4;
  --shadow: 0 1px 2px rgba(20,30,40,.06), 0 8px 24px rgba(20,30,40,.06);
}
:root[data-theme="dark"] {
  --paper: #0e1216; --surface: #161b21; --surface-2: #1b2129;
  --ink: #e7ecf1; --muted: #9aa4ae; --border: #2a323a;
  --accent: #7cc0ee; --accent-soft: #16303f; --mark: #ff8a5c; --code-bg: #1b2129;
  --shadow: 0 1px 2px rgba(0,0,0,.4), 0 10px 30px rgba(0,0,0,.35);
}

* { box-sizing: border-box; }
body {
  margin: 0;
  background: var(--paper);
  color: var(--ink);
  font: 16px/1.65 -apple-system, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
  -webkit-font-smoothing: antialiased;
}
@media (prefers-reduced-motion: no-preference) { html { scroll-behavior: smooth; } }

.serif {
  font-family: "Iowan Old Style", "Palatino Linotype", Palatino, "Book Antiqua", Georgia, serif;
}

.layout { display: grid; grid-template-columns: 288px minmax(0, 1fr); }

/* Sidebar */
.sidebar {
  position: sticky; top: 0; align-self: start;
  height: 100vh; overflow-y: auto;
  background: var(--surface);
  border-right: 1px solid var(--border);
  padding: 28px 22px 40px;
}
.brand { display: block; margin-bottom: 26px; padding-bottom: 20px; border-bottom: 1px solid var(--border); }
.brand .mark-rule { width: 34px; height: 3px; background: var(--mark); border-radius: 2px; margin-bottom: 12px; }
.brand h1 {
  font-family: "Iowan Old Style", "Palatino Linotype", Palatino, Georgia, serif;
  font-size: 1.28rem; line-height: 1.2; margin: 0; letter-spacing: .01em; text-wrap: balance;
}
.brand p { margin: 8px 0 0; font-size: .82rem; color: var(--muted); line-height: 1.5; }

.nav-group { margin-bottom: 22px; }
.nav-label {
  margin: 0 0 8px; font-size: .68rem; font-weight: 700; letter-spacing: .12em;
  text-transform: uppercase; color: var(--muted);
}
.sidebar a {
  display: block; padding: 6px 10px; margin: 1px 0; border-radius: 7px;
  color: var(--ink); text-decoration: none; font-size: .92rem;
  border-left: 3px solid transparent;
}
.sidebar a:hover { background: var(--surface-2); }
.sidebar a.active {
  color: var(--accent); background: var(--accent-soft);
  border-left-color: var(--mark); font-weight: 600;
}
.sidebar a:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }

/* Reading column */
main { padding: 0; }
.page {
  max-width: 74ch; margin: 0 auto; padding: 54px 40px;
  border-bottom: 1px solid var(--border);
}
.page:last-child { border-bottom: 0; }

.page h1, .page h2, .page h3, .page h4 {
  font-family: "Iowan Old Style", "Palatino Linotype", Palatino, Georgia, serif;
  line-height: 1.22; text-wrap: balance; color: var(--ink);
}
.page h1 { font-size: 2.1rem; margin: 0 0 .5em; letter-spacing: .005em; }
.page h2 {
  font-size: 1.5rem; margin: 2em 0 .6em; padding-top: .5em;
}
.page h3 { font-size: 1.18rem; margin: 1.7em 0 .5em; }
.page h1 + p { font-size: 1.08rem; color: var(--muted); }
.page p, .page li { font-size: 1rem; }
.page a { color: var(--accent); text-decoration: underline; text-underline-offset: 2px; text-decoration-thickness: 1px; }
.page a:hover { text-decoration-thickness: 2px; }
.page strong { color: var(--ink); }
.page ul, .page ol { padding-left: 1.3em; }
.page li { margin: .3em 0; }
.page li::marker { color: var(--mark); }

figure, .page img { margin: 0; }
.page img {
  display: block; max-width: 100%; height: auto; margin: 1.6em auto .4em;
  border: 1px solid var(--border); border-radius: 8px; box-shadow: var(--shadow);
}
/* The italic caption that follows a block image. A caption is authored with no
   blank line under its image, so pandoc keeps the img and the emphasis in one
   <p>: `<p><img><em>caption</em></p>`. Style the em by its image sibling —
   NOT `p:has(> em:only-child)`, which (because :only-child ignores text nodes)
   skips real captions and instead hits any body paragraph whose one child
   element happens to be an <em>. */
.page p:has(> img) > em {
  display: block; font-size: .86rem; color: var(--muted);
  text-align: center; margin: 0 auto 1.6em; max-width: 60ch;
}

blockquote {
  margin: 1.4em 0; padding: .2em 1.1em; border-left: 3px solid var(--accent);
  background: var(--accent-soft); border-radius: 0 8px 8px 0; color: var(--ink);
}
blockquote p { margin: .6em 0; }

code {
  font-family: ui-monospace, "SF Mono", Menlo, Consolas, monospace; font-size: .88em;
  background: var(--code-bg); padding: .12em .4em; border-radius: 5px;
}
pre { background: var(--code-bg); border: 1px solid var(--border); border-radius: 10px; padding: 16px; overflow-x: auto; }
pre code { background: none; padding: 0; font-size: .84rem; line-height: 1.55; }

.table-wrap, .page > table { display: block; overflow-x: auto; }
.page table { border-collapse: collapse; width: 100%; margin: 1.4em 0; font-size: .92rem; }
.page th, .page td { border: 1px solid var(--border); padding: 8px 12px; text-align: left; vertical-align: top; }
.page th { background: var(--surface-2); font-weight: 600; }

hr { border: 0; border-top: 1px solid var(--border); margin: 2.2em 0; }

@media (max-width: 820px) {
  .layout { grid-template-columns: 1fr; }
  .sidebar { position: static; height: auto; border-right: 0; border-bottom: 1px solid var(--border); }
  .page { padding: 40px 22px; }
}
"""

SCRIPT = """
(function () {
  var links = Array.prototype.slice.call(document.querySelectorAll('.sidebar a'));
  var byId = {};
  links.forEach(function (a) { byId[a.getAttribute('href').slice(1)] = a; });
  var sections = Array.prototype.slice.call(document.querySelectorAll('.page'));
  var current = null;
  var obs = new IntersectionObserver(function (entries) {
    entries.forEach(function (e) {
      if (e.isIntersecting) {
        if (current) current.classList.remove('active');
        current = byId[e.target.id];
        if (current) current.classList.add('active');
      }
    });
  }, { rootMargin: '-10% 0px -80% 0px', threshold: 0 });
  sections.forEach(function (s) { obs.observe(s); });
})();
"""

PAGE_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>CaveWhere User Manual</title>
<style>{css}</style>
</head>
<body>
<div class="layout">
  <aside class="sidebar">
    <div class="brand">
      <div class="mark-rule"></div>
      <h1>CaveWhere User Manual</h1>
      <p>Turning underground sketches and instrument readings into an accurate, shareable 3D cave map.</p>
    </div>
    <nav>
{nav}
    </nav>
  </aside>
  <main>
{main}
  </main>
</div>
<script>{script}</script>
</body>
</html>
"""


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "_site", "manual.html")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    html = build()
    with open(out, "w", encoding="utf-8") as handle:
        handle.write(html)
    size_kb = os.path.getsize(out) / 1024
    print(f"Wrote {out} ({size_kb:.0f} KB)")


if __name__ == "__main__":
    main()

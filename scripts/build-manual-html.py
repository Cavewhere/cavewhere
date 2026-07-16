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
    "": "Meta",
}

# The import-export/ directory holds two chapters; split it by filename so import
# pages and export pages get their own headers.
CHAPTER_BY_FILE = {
    "import-export/import-surveys.md": "Import",
    "import-export/import-csv.md": "Import",
    "import-export/export-surveys.md": "Export",
    "import-export/export-a-map.md": "Export",
}


def chapter_title(relpath):
    """The sidebar chapter a page is grouped under, derived from its path."""
    if relpath in CHAPTER_BY_FILE:
        return CHAPTER_BY_FILE[relpath]
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


# The manual's first page opens with the CaveWhere app icon as a title mark.
LOGO_PAGE = "concepts/why-cavewhere.md"


def logo_html(css_class="page-logo"):
    """The CaveWhere app icon (cavewherelib/icons/svg/cave.svg) as a data-URI
    <img>, used both as the sidebar brand mark and the opening page's title mark."""
    path = os.path.join(ROOT, "assets", "cave.svg")
    with open(path, "rb") as handle:
        data = base64.b64encode(handle.read()).decode("ascii")
    return f'<img class="{css_class}" src="data:image/svg+xml;base64,{data}" alt="CaveWhere logo">'


def font_face_css():
    """Embed the Yanone Kaffeesatz heading font (the face cavewhere.com uses) as a
    data URI, so the manual renders with the brand font offline and with no
    external request — the same self-contained approach used for images."""
    path = os.path.join(ROOT, "assets", "YanoneKaffeesatz-latin.woff2")
    with open(path, "rb") as handle:
        data = base64.b64encode(handle.read()).decode("ascii")
    return (
        "@font-face {\n"
        "  font-family: 'Yanone Kaffeesatz';\n"
        "  font-style: normal;\n"
        "  font-weight: 400 700;\n"
        "  font-display: swap;\n"
        f"  src: url(data:font/woff2;base64,{data}) format('woff2');\n"
        "}\n"
    )


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
        if relpath == LOGO_PAGE:
            frag = logo_html() + "\n" + frag
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

    return PAGE_TEMPLATE.format(
        css=font_face_css() + CSS, brand_logo=logo_html("brand-logo"),
        nav=nav, main=main, script=SCRIPT,
    )


CSS = """
/* Palette drawn from CaveWhere's app (Theme.qml) and cavewhere.com: brand navy
   ink (#1a2533 family), CaveWhere link blue (#1d4d77 / dark #85c1f4), slate-gray
   muted tones, and the app's warm orange kept as a single accent spark. */
:root {
  --paper: #eceff3;
  --surface: #ffffff;
  --surface-2: #e6ebf0;
  --ink: #1f2b38;
  --heading: #16202b;
  --muted: #566472;
  --border: #d4d9df;
  --accent: #1d4d77;
  --accent-soft: #dbe6f0;
  --mark: #e0662f;
  --code-bg: #e6ebf0;
  --shadow: 0 1px 2px rgba(20,30,45,.06), 0 8px 24px rgba(20,30,45,.07);
  --font-head: "Yanone Kaffeesatz", "Oswald", "Segoe UI", system-ui, sans-serif;
  --font-body: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
}
@media (prefers-color-scheme: dark) {
  :root {
    --paper: #10161e; --surface: #182029; --surface-2: #1f2833;
    --ink: #d7dfe8; --heading: #eef3f8; --muted: #93a1b0; --border: #2c3742;
    --accent: #85c1f4; --accent-soft: #1c3346; --mark: #ff8a5c;
    --code-bg: #1f2833;
    --shadow: 0 1px 2px rgba(0,0,0,.45), 0 10px 30px rgba(0,0,0,.4);
  }
}
:root[data-theme="light"] {
  --paper: #eceff3; --surface: #ffffff; --surface-2: #e6ebf0;
  --ink: #1f2b38; --heading: #16202b; --muted: #566472; --border: #d4d9df;
  --accent: #1d4d77; --accent-soft: #dbe6f0; --mark: #e0662f; --code-bg: #e6ebf0;
  --shadow: 0 1px 2px rgba(20,30,45,.06), 0 8px 24px rgba(20,30,45,.07);
}
:root[data-theme="dark"] {
  --paper: #10161e; --surface: #182029; --surface-2: #1f2833;
  --ink: #d7dfe8; --heading: #eef3f8; --muted: #93a1b0; --border: #2c3742;
  --accent: #85c1f4; --accent-soft: #1c3346; --mark: #ff8a5c; --code-bg: #1f2833;
  --shadow: 0 1px 2px rgba(0,0,0,.45), 0 10px 30px rgba(0,0,0,.4);
}

* { box-sizing: border-box; }
body {
  margin: 0;
  background: var(--paper);
  color: var(--ink);
  font: 16px/1.65 var(--font-body);
  -webkit-font-smoothing: antialiased;
}
@media (prefers-reduced-motion: no-preference) { html { scroll-behavior: smooth; } }

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
.brand-head { display: flex; align-items: center; gap: 12px; }
.brand-logo {
  flex: 0 0 auto; width: 34px; height: 34px; padding: 6px;
  background: #ffffff; border: 1px solid var(--border); border-radius: 9px;
}
.brand h1 {
  font-family: var(--font-head); font-weight: 700;
  font-size: 1.42rem; line-height: 1.04; margin: 0; letter-spacing: .01em;
  text-wrap: balance; color: var(--heading);
}
.brand p { margin: 14px 0 0; font-size: .82rem; color: var(--muted); line-height: 1.5; }

.nav-group { margin-bottom: 22px; }
.nav-label {
  margin: 0 0 8px; font-family: var(--font-head); font-size: .86rem; font-weight: 600;
  letter-spacing: .05em; text-transform: uppercase; color: var(--muted);
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
  font-family: var(--font-head); font-weight: 700;
  line-height: 1.08; text-wrap: balance; color: var(--heading);
}
.page h1 { font-size: 2.6rem; margin: 0 0 .35em; letter-spacing: .01em; }
.page h2 {
  font-size: 1.9rem; font-weight: 600; margin: 1.8em 0 .5em; padding-top: .4em;
}
.page h3 { font-size: 1.45rem; font-weight: 600; margin: 1.6em 0 .45em; }
.page h1 + p { font-family: var(--font-body); font-size: 1.08rem; color: var(--muted); }
.page p, .page li { font-size: 1rem; }
.page a { color: var(--accent); text-decoration: underline; text-underline-offset: 2px; text-decoration-thickness: 1px; }
.page a:hover { text-decoration-thickness: 2px; }
.page strong { color: var(--heading); font-weight: 600; }
.page ul, .page ol { padding-left: 1.3em; }
.page li { margin: .3em 0; }
.page li::marker { color: var(--mark); }

figure, .page img { margin: 0; }
.page img {
  display: block; max-width: 100%; height: auto; margin: 1.6em auto .4em;
  border: 1px solid var(--border); border-radius: 8px; box-shadow: var(--shadow);
}
/* The CaveWhere app icon as a title mark above the first page's heading. Its
   formations are black, so it sits on a fixed white tile to stay legible on the
   dark navy theme too. */
.page img.page-logo {
  width: 180px; height: 180px; padding: 30px; margin: 0 auto 28px;
  background: #ffffff; border: 1px solid var(--border);
  border-radius: 36px; box-shadow: var(--shadow);
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
      <div class="brand-head">
        {brand_logo}
        <h1>CaveWhere User Manual</h1>
      </div>
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

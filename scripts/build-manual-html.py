#!/usr/bin/env python3
"""Build an HTML view of the CaveWhere manual from its Markdown sources.

The Markdown files under docs/manual/ are the source of truth. This script
converts them (via pandoc) into a browsable HTML site. Nav order and titles come
from docs/manual/llms.txt, so there is no second index to maintain.

Two output shapes:

  Multi-page (default) — one .html per source page, mirroring the source
  directory layout, sharing a single assets/manual.css and a copied images/
  tree. Each page pulls in only its own images, so a reader downloads a few
  hundred KB per page instead of the whole manual. This is what you upload to a
  static host; serve the directory (index.html is the landing page).

  Single-file (--single) — ONE standalone .html with every image inlined as a
  data URI and all CSS/JS inline. Big (tens of MB) but self-contained: double-
  click to open, or hand someone the one file for offline reading.

Usage:
  scripts/build-manual-html.py [output-dir]        # multi-page site
  scripts/build-manual-html.py --single [out.html] # one standalone file

Defaults: docs/manual/_site/ (multi-page), docs/manual/_site/manual.html
(--single). Requires: pandoc on PATH.
"""

import base64
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(__file__), "..", "docs", "manual")
ROOT = os.path.normpath(ROOT)


def slug_key(relpath):
    """Stable per-file id namespace, e.g. concepts/why-cavewhere.md -> concepts-why-cavewhere."""
    base = re.sub(r"\.md$", "", relpath)
    return re.sub(r"[^a-z0-9]+", "-", base.lower()).strip("-")


def out_relpath(relpath):
    """Output path of a source page, e.g. concepts/why-cavewhere.md -> concepts/why-cavewhere.html."""
    return re.sub(r"\.md$", ".html", relpath)


def rel_href(from_relpath, to_out_relpath):
    """Relative href from the page being rendered to another output file."""
    from_dir = os.path.dirname(out_relpath(from_relpath))
    return os.path.relpath(to_out_relpath, from_dir or ".")


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


def group_chapters(entries):
    """Group the ordered entries into [(chapter, [(title, relpath)])] runs, so the
    sidebar shows the same chapter headers in both output shapes."""
    chapters = []
    for _, title, relpath in entries:
        chapter = chapter_title(relpath)
        if not chapters or chapters[-1][0] != chapter:
            chapters.append((chapter, []))
        chapters[-1][1].append((title, relpath))
    return chapters


def strip_front_matter(text):
    return re.sub(r"^---\n.*?\n---\n", "", text, count=1, flags=re.DOTALL)


def pandoc_fragment(body, key, id_prefix=True):
    """Render a Markdown body to an HTML fragment. In the single-file build every
    page shares one document, so heading ids are namespaced with the page key to
    stay unique; in the multi-page build each page is its own document, so the
    plain GFM heading slugs are kept (they match the manual's authored #anchors)."""
    command = ["pandoc", "--from", "gfm", "--to", "html"]
    if id_prefix:
        command += ["--id-prefix", key + "--"]
    result = subprocess.run(
        command, input=body, capture_output=True, text=True, check=True,
    )
    return result.stdout


def embed_images(html, page_dir):
    """Replace <img src="local.png"> with an inline data URI (single-file build)."""
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


def warn_missing_images(html, page_dir):
    """Multi-page keeps relative <img src> as-is (the images/ tree is copied next
    to the pages), so nothing is rewritten — but still flag a broken local ref."""
    for src in re.findall(r'src="([^"]+)"', html):
        if src.startswith(("http:", "https:", "data:")):
            continue
        img_path = os.path.normpath(os.path.join(ROOT, page_dir, src))
        if not os.path.exists(img_path):
            print(f"  warning: missing image {src} -> {img_path}", file=sys.stderr)


def rewrite_links_single(html, relpath, key_by_relpath):
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


def rewrite_links_multi(html, relpath, key_by_relpath):
    """Turn cross-file .md links into links to the sibling .html page. The output
    mirrors the source layout, so the relative link stays valid — only the .md
    extension changes; the #anchor (a GFM heading slug) is preserved."""
    page_dir = os.path.dirname(relpath)

    def repl(match):
        target, anchor = match.group(1), match.group(2) or ""
        if target.startswith(("http:", "https:", "mailto:")):
            return match.group(0)
        resolved = os.path.normpath(os.path.join(page_dir, target))
        if resolved not in key_by_relpath:
            return match.group(0)  # link to a page not in this manual; leave it
        return f'href="{out_relpath(target)}{anchor}"'

    return re.sub(r'href="([^"#]+\.md)(#[^"]*)?"', repl, html)


def cavewhere_version():
    """The CaveWhere version the manual documents, as `git describe HEAD` — the
    same string the app shows (e.g. 2026.4.3-424-ged1d7c4a). Returns None when
    git or a tag is unavailable, so the sidebar simply omits the line."""
    try:
        result = subprocess.run(
            ["git", "describe", "HEAD"],
            cwd=ROOT, capture_output=True, text=True, check=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return None
    return result.stdout.strip() or None


# The manual's first page opens with the CaveWhere app icon as a title mark.
LOGO_PAGE = "concepts/why-cavewhere.md"

# The multi-page site's landing page and the target the sidebar brand links home to.
INDEX_PAGE = "index.md"


# The CaveWhere app icon (cavewherelib/icons/svg/cave.svg), used as the sidebar
# brand mark and the opening page's title mark.
LOGO_SVG = "assets/cave.svg"


def logo_html(css_class="page-logo"):
    """The app icon as a data-URI <img> — for the single-file build, which inlines
    every asset. It is a detailed ~65 KB SVG, so the multi-page build references
    the copied file instead (see logo_img) rather than repeating it on every page."""
    path = os.path.join(ROOT, LOGO_SVG)
    with open(path, "rb") as handle:
        data = base64.b64encode(handle.read()).decode("ascii")
    return f'<img class="{css_class}" src="data:image/svg+xml;base64,{data}" alt="CaveWhere logo">'


def logo_img(current_relpath, css_class="page-logo"):
    """The app icon as an <img> referencing the shared assets/cave.svg, resolved
    relative to the page being rendered so it caches once across the site."""
    href = rel_href(current_relpath, LOGO_SVG)
    return f'<img class="{css_class}" src="{href}" alt="CaveWhere logo">'


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


def version_html(version):
    return f'      <p class="brand-version">CaveWhere {version}</p>\n' if version else ""


def build_single_html():
    """Render the whole manual as one standalone HTML string."""
    entries = parse_llms()
    key_by_relpath = {relpath: slug_key(relpath) for _, _, relpath in entries}

    pages_html = []
    for _, _, relpath in entries:
        key = key_by_relpath[relpath]
        with open(os.path.join(ROOT, relpath), encoding="utf-8") as handle:
            body = strip_front_matter(handle.read())
        frag = pandoc_fragment(body, key)
        frag = rewrite_links_single(frag, relpath, key_by_relpath)
        frag = embed_images(frag, os.path.dirname(relpath))
        if relpath == LOGO_PAGE:
            frag = logo_html() + "\n" + frag
        pages_html.append(f'<section id="{key}" class="page">\n{frag}\n</section>')

    nav_html = []
    for chapter, items in group_chapters(entries):
        links = "\n".join(
            f'      <a href="#{key_by_relpath[relpath]}">{title}</a>'
            for title, relpath in items
        )
        nav_html.append(
            f'    <div class="nav-group">\n'
            f'      <p class="nav-label">{chapter}</p>\n{links}\n    </div>'
        )

    return SINGLE_PAGE_TEMPLATE.format(
        css=font_face_css() + CSS, brand_logo=logo_html("brand-logo"),
        brand_version=version_html(cavewhere_version()),
        nav="\n".join(nav_html), main="\n\n".join(pages_html), script=SCRIPT,
    )


def render_nav_multi(chapters, current_relpath):
    """Sidebar nav for one page: links resolve relative to that page, and the
    current page's link is marked active (no scroll-spy needed)."""
    nav_html = []
    for chapter, items in chapters:
        links = []
        for title, relpath in items:
            href = rel_href(current_relpath, out_relpath(relpath))
            active = ' class="active"' if relpath == current_relpath else ""
            links.append(f'      <a{active} href="{href}">{title}</a>')
        nav_html.append(
            f'    <div class="nav-group">\n'
            f'      <p class="nav-label">{chapter}</p>\n' + "\n".join(links) + "\n    </div>"
        )
    return "\n".join(nav_html)


def render_page_nav(entries, index, current_relpath):
    """The prev/next footer links, in llms.txt reading order."""
    def link(entry, css_class, label):
        _, title, relpath = entry
        href = rel_href(current_relpath, out_relpath(relpath))
        return (f'    <a class="{css_class}" href="{href}">'
                f'<span>{label}</span>{title}</a>')

    parts = []
    if index > 0:
        parts.append(link(entries[index - 1], "page-nav-prev", "Previous"))
    if index < len(entries) - 1:
        parts.append(link(entries[index + 1], "page-nav-next", "Next"))
    if not parts:
        return ""
    return '  <nav class="page-nav">\n' + "\n".join(parts) + "\n  </nav>\n"


def render_page(entries, index, chapters, key_by_relpath, version):
    _, title, relpath = entries[index]
    key = key_by_relpath[relpath]
    with open(os.path.join(ROOT, relpath), encoding="utf-8") as handle:
        body = strip_front_matter(handle.read())
    frag = pandoc_fragment(body, key, id_prefix=False)
    frag = rewrite_links_multi(frag, relpath, key_by_relpath)
    warn_missing_images(frag, os.path.dirname(relpath))
    if relpath == LOGO_PAGE:
        frag = logo_img(relpath, "page-logo") + "\n" + frag

    return MULTI_PAGE_TEMPLATE.format(
        title=title,
        css_href=rel_href(relpath, "assets/manual.css"),
        index_href=rel_href(relpath, out_relpath(INDEX_PAGE)),
        brand_logo=logo_img(relpath, "brand-logo"),
        brand_version=version_html(version),
        nav=render_nav_multi(chapters, relpath),
        key=key, main=frag,
        page_nav=render_page_nav(entries, index, relpath),
    )


def build_multipage(out_dir):
    """Write the manual as a multi-page site under out_dir."""
    entries = parse_llms()
    key_by_relpath = {relpath: slug_key(relpath) for _, _, relpath in entries}
    chapters = group_chapters(entries)
    version = cavewhere_version()

    os.makedirs(os.path.join(out_dir, "assets"), exist_ok=True)
    with open(os.path.join(out_dir, "assets", "manual.css"), "w", encoding="utf-8") as handle:
        handle.write(font_face_css() + CSS)
    shutil.copyfile(os.path.join(ROOT, LOGO_SVG), os.path.join(out_dir, LOGO_SVG))
    # Publish llms.txt at the site root: it is the machine-readable index (the
    # landing page links to it for AI agents) and follows the llms.txt convention.
    shutil.copyfile(os.path.join(ROOT, "llms.txt"), os.path.join(out_dir, "llms.txt"))

    # Copy the shared image tree once; pages reference it relatively (../images/...).
    src_images = os.path.join(ROOT, "images")
    if os.path.isdir(src_images):
        shutil.copytree(src_images, os.path.join(out_dir, "images"), dirs_exist_ok=True)

    for index, (_, _, relpath) in enumerate(entries):
        html = render_page(entries, index, chapters, key_by_relpath, version)
        dest = os.path.join(out_dir, out_relpath(relpath))
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "w", encoding="utf-8") as handle:
            handle.write(html)

    print(f"Wrote {len(entries)} pages + shared assets/images to {out_dir}")


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
.brand-head { display: flex; align-items: center; gap: 12px; text-decoration: none; color: inherit; }
a.brand-head:hover h1 { color: var(--accent); }
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
.brand-version {
  margin: 12px 0 0; font-size: .72rem; color: var(--muted); letter-spacing: .02em;
  font-family: ui-monospace, "SF Mono", Menlo, Consolas, monospace; word-break: break-all;
}

.nav-group { margin-bottom: 22px; }
.nav-label {
  margin: 0 0 8px; font-family: var(--font-head); font-size: .86rem; font-weight: 600;
  letter-spacing: .05em; text-transform: uppercase; color: var(--muted);
}
.sidebar nav a {
  display: block; padding: 6px 10px; margin: 1px 0; border-radius: 7px;
  color: var(--ink); text-decoration: none; font-size: .92rem;
  border-left: 3px solid transparent;
}
.sidebar nav a:hover { background: var(--surface-2); }
.sidebar nav a.active {
  color: var(--accent); background: var(--accent-soft);
  border-left-color: var(--mark); font-weight: 600;
}
.sidebar nav a:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }

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

/* Prev/next footer (multi-page only). */
.page-nav {
  max-width: 74ch; margin: 0 auto; padding: 8px 40px 64px;
  display: flex; gap: 16px; justify-content: space-between;
}
.page-nav a {
  display: flex; flex-direction: column; gap: 2px; max-width: 46%;
  padding: 12px 16px; border: 1px solid var(--border); border-radius: 10px;
  background: var(--surface); color: var(--accent);
  text-decoration: none; font-weight: 600; font-size: .98rem;
}
.page-nav a:hover { border-color: var(--accent); }
.page-nav a:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }
.page-nav a span {
  font-size: .74rem; font-weight: 600; text-transform: uppercase;
  letter-spacing: .05em; color: var(--muted);
}
.page-nav-prev { align-items: flex-start; }
.page-nav-next { align-items: flex-end; text-align: right; margin-left: auto; }

@media (max-width: 820px) {
  .layout { grid-template-columns: 1fr; }
  .sidebar { position: static; height: auto; border-right: 0; border-bottom: 1px solid var(--border); }
  .page { padding: 40px 22px; }
  .page-nav { flex-direction: column; padding: 8px 22px 48px; }
  .page-nav a { max-width: none; }
  .page-nav-next { align-items: flex-start; text-align: left; margin-left: 0; }
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

SINGLE_PAGE_TEMPLATE = """<!DOCTYPE html>
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
{brand_version}    </div>
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

MULTI_PAGE_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title} — CaveWhere User Manual</title>
<link rel="stylesheet" href="{css_href}">
</head>
<body>
<div class="layout">
  <aside class="sidebar">
    <div class="brand">
      <a class="brand-head" href="{index_href}">
        {brand_logo}
        <h1>CaveWhere User Manual</h1>
      </a>
      <p>Turning underground sketches and instrument readings into an accurate, shareable 3D cave map.</p>
{brand_version}    </div>
    <nav>
{nav}
    </nav>
  </aside>
  <main>
    <article id="{key}" class="page">
{main}
    </article>
{page_nav}  </main>
</div>
</body>
</html>
"""


def main():
    args = sys.argv[1:]
    single = False
    if args and args[0] == "--single":
        single = True
        args = args[1:]

    if single:
        out = args[0] if args else os.path.join(ROOT, "_site", "manual.html")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w", encoding="utf-8") as handle:
            handle.write(build_single_html())
        size_kb = os.path.getsize(out) / 1024
        print(f"Wrote {out} ({size_kb:.0f} KB)")
    else:
        out = args[0] if args else os.path.join(ROOT, "_site")
        build_multipage(out)


if __name__ == "__main__":
    main()

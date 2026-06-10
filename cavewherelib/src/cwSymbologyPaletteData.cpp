/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSymbologyPaletteData.h"

//Qt includes
#include <QHash>
#include <QSet>
#include <QStringList>

//Std includes
#include <functional>

namespace {

// A node in the glyph ↔ brush dependency graph. The kind keeps a glyph and a
// brush that happen to share a name from colliding on the DFS stack.
struct DependencyNode {
    enum Kind : int { Glyph, Brush };
    Kind kind;
    QString name;
    bool operator==(const DependencyNode &o) const = default;
};

size_t qHash(const DependencyNode &node, size_t seed) noexcept
{
    return qHashMulti(seed, static_cast<int>(node.kind), node.name);
}

} // namespace

std::optional<cwLineBrush> cwSymbologyPaletteData::brush(QStringView name) const
{
    for (const auto &b : lineBrushes) {
        if (b.name == name) {
            return b;
        }
    }
    return std::nullopt;
}

std::optional<cwSymbologyGlyph> cwSymbologyPaletteData::glyph(QStringView name) const
{
    for (const auto &g : glyphs) {
        if (g.name == name) {
            return g;
        }
    }
    return std::nullopt;
}

cwPaletteSnapshot cwSymbologyPaletteData::snapshot() const
{
    QHash<QString, cwLineBrush> brushesByName;
    brushesByName.reserve(lineBrushes.size());
    for (const auto &b : lineBrushes) {
        brushesByName.insert(b.name, b);
    }

    QHash<QString, cwSymbologyGlyph> glyphsByName;
    glyphsByName.reserve(glyphs.size());
    for (const auto &g : glyphs) {
        glyphsByName.insert(g.name, g);
    }

    return cwPaletteSnapshot(std::move(brushesByName), std::move(glyphsByName));
}

QString cwSymbologyPaletteData::findDuplicateBrushName(const QVector<cwLineBrush> &brushes)
{
    QSet<QString> seen;
    seen.reserve(brushes.size());
    for (const auto &b : brushes) {
        if (seen.contains(b.name)) {
            return b.name;
        }
        seen.insert(b.name);
    }
    return QString();
}

QString cwSymbologyPaletteData::findDuplicateGlyphName(const QVector<cwSymbologyGlyph> &glyphs)
{
    QSet<QString> seen;
    seen.reserve(glyphs.size());
    for (const auto &g : glyphs) {
        if (seen.contains(g.name)) {
            return g.name;
        }
        seen.insert(g.name);
    }
    return QString();
}

QString cwSymbologyPaletteData::findGlyphDependencyCycle() const
{
    // A glyph edges to the brush of each of its strokes; a brush edges to the
    // glyph of each stamp (non-OffsetCurve) decoration layer. Both lookups go
    // through hashes so the whole DFS is O(nodes + edges), not O(nodes × glyphs).
    QHash<QString, cwLineBrush> brushByName;
    brushByName.reserve(lineBrushes.size());
    for (const auto &b : lineBrushes) {
        brushByName.insert(b.name, b);
    }
    QHash<QString, cwSymbologyGlyph> glyphByName;
    glyphByName.reserve(glyphs.size());
    for (const auto &g : glyphs) {
        glyphByName.insert(g.name, g);
    }

    const auto successors = [&](const DependencyNode &node) -> QVector<DependencyNode> {
        QVector<DependencyNode> out;
        if (node.kind == DependencyNode::Glyph) {
            const auto it = glyphByName.constFind(node.name);
            if (it != glyphByName.constEnd()) {
                for (const auto &stroke : it->strokes) {
                    if (brushByName.contains(stroke.brushName)) {
                        out.append({DependencyNode::Brush, stroke.brushName});
                    }
                }
            }
        } else {
            const auto it = brushByName.constFind(node.name);
            if (it != brushByName.constEnd()) {
                for (const auto &layer : it->decorations) {
                    if (!layer.glyphName.isEmpty()) {
                        out.append({DependencyNode::Glyph, layer.glyphName});
                    }
                }
            }
        }
        return out;
    };

    QSet<DependencyNode> explored;   // nodes whose subtree is proven acyclic
    QSet<DependencyNode> onPath;     // nodes on the active DFS stack
    QList<DependencyNode> pathNodes; // the active DFS stack, in order

    std::function<QString(const DependencyNode &)> dfs =
        [&](const DependencyNode &node) -> QString {
        pathNodes.append(node);
        onPath.insert(node);
        for (const DependencyNode &next : successors(node)) {
            if (onPath.contains(next)) {
                QStringList names;
                names.reserve(pathNodes.size() + 1);
                for (const DependencyNode &n : pathNodes) {
                    names.append(n.name);
                }
                names.append(next.name);
                return names.join(QStringLiteral(" → "));
            }
            if (!explored.contains(next)) {
                const QString cycle = dfs(next);
                if (!cycle.isEmpty()) {
                    return cycle;
                }
            }
        }
        onPath.remove(node);
        pathNodes.removeLast();
        explored.insert(node);
        return QString();
    };

    for (const auto &g : glyphs) {
        const DependencyNode node{DependencyNode::Glyph, g.name};
        if (!explored.contains(node)) {
            const QString cycle = dfs(node);
            if (!cycle.isEmpty()) {
                return cycle;
            }
        }
    }
    return QString();
}

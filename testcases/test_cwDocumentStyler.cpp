//Our includes
#include "cwDocumentStyler.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QColor>
#include <QCoreApplication>
#include <QImage>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickTextDocument>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFragment>
#include <QUrl>

//Std includes
#include <functional>

namespace {
    const int kBaseSize = 16;
    const QColor kMuted(200, 100, 50);
    const QColor kLink(10, 20, 200);
    const QString kHeadingFamily = QStringLiteral("TestHeadingFamily");

    QTextBlock blockAtHeadingLevel(QTextDocument* doc, int level)
    {
        for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
            if (block.blockFormat().headingLevel() == level) {
                return block;
            }
        }
        return QTextBlock();
    }

    QTextFragment firstTextFragment(const QTextBlock& block)
    {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (fragment.isValid() && !fragment.charFormat().isImageFormat()) {
                return fragment;
            }
        }
        return QTextFragment();
    }

    QTextFragment findFragment(QTextDocument* doc, const std::function<bool(const QTextFragment&)>& match)
    {
        for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
            for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
                const QTextFragment fragment = it.fragment();
                if (fragment.isValid() && match(fragment)) {
                    return fragment;
                }
            }
        }
        return QTextFragment();
    }
}

TEST_CASE("cwDocumentStyler restyles a rendered Markdown document", "[cwDocumentStyler]")
{
    //A live QQuickTextDocument only comes from a Quick text item, so drive a real
    //TextEdit rendering MarkdownText and post-process its document.
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick\nTextEdit { textFormat: TextEdit.MarkdownText }", QUrl());
    REQUIRE(component.status() == QQmlComponent::Ready);
    QObject* edit = component.create();
    REQUIRE(edit != nullptr);

    //A known-size image on disk (a non-"qrc:" path the styler reads verbatim).
    QImage image(400, 200, QImage::Format_RGB32);
    image.fill(Qt::blue);
    QTemporaryDir imageDir;
    REQUIRE(imageDir.isValid());
    const QString imagePath = imageDir.filePath(QStringLiteral("styler-image.png"));
    REQUIRE(image.save(imagePath, "PNG"));

    const QString markdown =
        QStringLiteral("# Title\n\n"
                       "Lead paragraph after the heading.\n\n"
                       "## Section\n\n"
                       "Body with a [link](https://example.com) inside.\n\n"
                       "![alt](%1)\n").arg(imagePath);

    edit->setProperty("text", markdown);
    QCoreApplication::processEvents();

    QQuickTextDocument* quickDocument = qvariant_cast<QQuickTextDocument*>(edit->property("textDocument"));
    REQUIRE(quickDocument != nullptr);
    QTextDocument* doc = quickDocument->textDocument();
    REQUIRE(doc != nullptr);
    REQUIRE(doc->blockCount() >= 5); //h1, lead, h2, body, image — the parse is ready

    cwDocumentStyler styler;
    styler.setBaseFontSize(kBaseSize);
    styler.setMutedColor(kMuted);
    styler.setLinkColor(kLink);
    styler.setHeadingFontFamily(kHeadingFamily);
    styler.setContentWidth(100.0);
    styler.setDocument(quickDocument); //applies

    SECTION("headings are sized from the base and use the heading font family")
    {
        const QTextFragment h1 = firstTextFragment(blockAtHeadingLevel(doc, 1));
        const QTextFragment h2 = firstTextFragment(blockAtHeadingLevel(doc, 2));
        REQUIRE(h1.isValid());
        REQUIRE(h2.isValid());

        CHECK(h1.charFormat().font().pixelSize() == qRound(2.6 * kBaseSize)); //42
        CHECK(h2.charFormat().font().pixelSize() == qRound(1.9 * kBaseSize)); //30
        CHECK(h1.charFormat().font().family() == kHeadingFamily);
    }

    SECTION("the first paragraph after an h1 is the muted, slightly larger lead")
    {
        //The block right after the h1 title.
        QTextBlock lead = blockAtHeadingLevel(doc, 1).next();
        while (lead.isValid() && lead.text().trimmed().isEmpty()) {
            lead = lead.next();
        }
        const QTextFragment fragment = firstTextFragment(lead);
        REQUIRE(fragment.isValid());

        CHECK(fragment.charFormat().font().pixelSize() == qRound(1.08 * kBaseSize)); //17
        CHECK(fragment.charFormat().foreground().color() == kMuted);
    }

    SECTION("links are recolored to the theme link color")
    {
        const QTextFragment anchor = findFragment(doc, [](const QTextFragment& f) {
            return f.charFormat().isAnchor();
        });
        REQUIRE(anchor.isValid());
        CHECK(anchor.charFormat().foreground().color() == kLink);
    }

    SECTION("body paragraphs get proportional line spacing")
    {
        //The body paragraph under the h2 (not the lead, not a heading, no image).
        QTextBlock body = blockAtHeadingLevel(doc, 2).next();
        while (body.isValid() && body.text().trimmed().isEmpty()) {
            body = body.next();
        }
        REQUIRE(body.isValid());
        CHECK(body.blockFormat().lineHeightType() == QTextBlockFormat::ProportionalHeight);
        CHECK(body.blockFormat().lineHeight() == 130);
    }

    SECTION("a wide image is clamped to the content width, only shrinking")
    {
        const auto isImage = [](const QTextFragment& f) { return f.charFormat().isImageFormat(); };

        QTextFragment imageFragment = findFragment(doc, isImage);
        REQUIRE(imageFragment.isValid());
        //400px natural clamped to 100px content width, aspect preserved.
        CHECK(imageFragment.charFormat().toImageFormat().width() == 100.0);
        CHECK(imageFragment.charFormat().toImageFormat().height() == 50.0);

        //Widening past the natural size restores it (never upscales).
        styler.setContentWidth(1000.0);
        imageFragment = findFragment(doc, isImage);
        REQUIRE(imageFragment.isValid());
        CHECK(imageFragment.charFormat().toImageFormat().width() == 400.0);
        CHECK(imageFragment.charFormat().toImageFormat().height() == 200.0);
    }
}

#ifndef CPPHIGHLIGHTER_H
#define CPPHIGHLIGHTER_H

#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

class CppHighlighter : public QSyntaxHighlighter
{
public:
    CppHighlighter(QTextDocument *parent = nullptr)
        : QSyntaxHighlighter(parent)
    {
        HighlightingRule rule;

        commentFormat.setForeground(QColor(0x6A, 0x99, 0x55));

        // // ... コメント
        rule.pattern = QRegularExpression("//[^\n]*");
        rule.format = commentFormat;
        highlightingRules.append(rule);

        // /* ... */ コメント（複数行）
        multiLineCommentFormat.setForeground(QColor(0x6A, 0x99, 0x55));
        commentStartExpression = QRegularExpression("/\\*");
        commentEndExpression   = QRegularExpression("\\*/");
    }

protected:
    void highlightBlock(const QString &text) override
    {
        for (const HighlightingRule &rule : highlightingRules) {
            QRegularExpressionMatchIterator i = rule.pattern.globalMatch(text);
            while (i.hasNext()) {
                auto match = i.next();
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
            }
        }

        setCurrentBlockState(0);

        // 複数行コメント
        int start = 0;
        if (previousBlockState() != 1)
            start = text.indexOf(commentStartExpression);

        while (start >= 0) {
            QRegularExpressionMatch endMatch = commentEndExpression.match(text, start);
            int end = endMatch.capturedStart();

            int length;
            if (end == -1) {
                setCurrentBlockState(1);
                length = text.length() - start;
            } else {
                length = end - start + endMatch.capturedLength();
            }

            setFormat(start, length, multiLineCommentFormat);
            start = text.indexOf(commentStartExpression, start + length);
        }
    }

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> highlightingRules;

    QTextCharFormat commentFormat;
    QTextCharFormat multiLineCommentFormat;
    QRegularExpression commentStartExpression;
    QRegularExpression commentEndExpression;
};


#endif // CPPHIGHLIGHTER_H

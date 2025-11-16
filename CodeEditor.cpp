#include "CodeEditor.h"
#include "LineNumberArea.h"
#include "SafeCompleter.h"

#include <QPainter>
#include <QTextBlock>
#include <QKeyEvent>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QRegularExpression>
#include <QTimer>

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent) {
    lineNumberArea = new LineNumberArea(this);

    connect(this, &CodeEditor::blockCountChanged,
            this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest,
            this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged,
            this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    // SafeCompleter を使う
    completer = new SafeCompleter(this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setWidget(this);

    // words モデルを設定
    QStringList words = {
        "int", "long", "double", "float", "char", "string",
        "vector", "map", "set", "unordered_map", "unordered_set",
        "queue", "priority_queue", "stack", "pair",
        "for", "while", "if", "else", "return",
        "include", "namespace", "std",
        "cout", "cin", "endl",
        "push_back", "size", "begin", "end"
    };

    auto *model = new QStringListModel(words, completer);
    completer->setModel(model);
    (void)completer->popup();

    connect(
        completer, QOverload<const QString &>::of(&QCompleter::activated),
        this, &CodeEditor::insertCompletion
        );
}

int CodeEditor::lineNumberAreaWidth() {
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        digits++;
    }
    int space = 4 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy) {
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event) {
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), QColor(30, 30, 30));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(QColor(180, 180, 180));
            painter.drawText(0, top, lineNumberArea->width(), fontMetrics().height(),
                             Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        blockNumber++;
    }
}

void CodeEditor::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> extraSelections;

    QTextEdit::ExtraSelection selection;
    QColor lineColor = QColor(Qt::yellow).lighter(160);

    selection.format.setBackground(lineColor);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    extraSelections.append(selection);

    setExtraSelections(extraSelections);
}

void CodeEditor::keyPressEvent(QKeyEvent *e)
{
    if (completer && completer->popup()->isVisible()) {
        switch (e->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            e->ignore();
            return;
        default:
            break;
        }
    }

    QPlainTextEdit::keyPressEvent(e);

    if (!completer)
        return;

    QString text = e->text();
    if (text.isEmpty()) {
        completer->popup()->hide();
        return;
    }

    const QChar c = text.at(0);
    if (!c.isLetterOrNumber() && c != '_') {
        completer->popup()->hide();
        return;
    }

    QTextCursor tc = textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    QString prefix = tc.selectedText();

    if (prefix.isEmpty()) {
        completer->popup()->hide();
        return;
    }

    completer->setCompletionPrefix(prefix);
    completer->popup()->setCurrentIndex(completer->completionModel()->index(0, 0));

    // --- UI更新後に安全にcomplete呼び出し ---
    QTimer::singleShot(0, this, [this]() {
        if (!completer || !completer->popup())
            return;

        QRect cr = cursorRect();
        cr.setWidth(completer->popup()->sizeHintForColumn(0)
                    + completer->popup()->verticalScrollBar()->sizeHint().width());
        completer->complete(cr);
    });
}

void CodeEditor::insertCompletion(const QString &completion)
{
    if (!completer || completer->widget() != this)
        return;

    QTextCursor tc = textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    tc.insertText(completion);
    setTextCursor(tc);

    // popupを閉じる（2重発火防止）
    completer->popup()->hide();
}

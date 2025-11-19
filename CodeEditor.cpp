#include "CodeEditor.h"
#include "LineNumberArea.h"
#include "SafeCompleter.h"
#include "CppHighlighter.h"

#include <QPainter>
#include <QTextBlock>
#include <QKeyEvent>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QRegularExpression>
#include <QTimer>
#include <QStringView>

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
    highlighter = new CppHighlighter(this->document());

    completer = new SafeCompleter(this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setWidget(this);

    // words モデルを設定
    QStringList words = {
        "#include", "<iostream>", "<vector>", "<algorithm>",
        "int", "long", "double", "float", "char", "string",
        "vector", "map", "set", "unordered_map", "unordered_set",
        "queue", "priority_queue", "stack", "pair",
        "for", "while", "if", "else", "return",
        "namespace", "std", "cout", "cin", "endl",
        "push_back", "size", "begin", "end", "using"
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
    if (e->modifiers() & Qt::ControlModifier && e->key() == Qt::Key_L) {
        toggleComment();
        return;
    }

    // --- ① 補完候補が表示中の特殊キー処理 ---
    if (completer && completer->popup()->isVisible()) {
        switch (e->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            e->ignore();
            return; // completerが処理する
        default:
            break;
        }
    }

    // 2. 括弧・クォートの自動補完
    const QString openers = "([{\'\"";
    const QString closers = ")]}\'\"";

    // 入力された1文字を取得
    const QString text = e->text();
    if (text.length() == 1) {
        const QChar c = text[0];
        int idx = openers.indexOf(c);
        if (idx != -1) {
            // 括弧系の入力時、自動で閉じ文字を挿入
            QChar closing = closers[idx];
            QPlainTextEdit::keyPressEvent(e); // 入力文字を表示
            insertPlainText(QString(closing));
            moveCursor(QTextCursor::Left); // カーソルを括弧の間に戻す
            return;
        }
    }

    // --- ② 通常のキー入力を処理 ---
    QPlainTextEdit::keyPressEvent(e);
    if (!completer)
        return;

    if (text.isEmpty()) {
        completer->popup()->hide();
        return;
    }

    // --- ③ 入力文字が補完対象かチェック ---
    const QChar c = text.at(0);
    const QString allowedSymbols = "_<>#.:/";
    if (!c.isLetterOrNumber() && !allowedSymbols.contains(c)) {
        completer->popup()->hide();
        return;
    }

    // --- ④ prefix（入力中の単語）を正規表現で抽出 ---
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 64);
    QString before = cursor.selectedText();

    // 許可する構成文字
    QRegularExpression re("([A-Za-z0-9_<>#.:/]+)$");
    QRegularExpressionMatch match = re.match(before);
    QString prefix = match.hasMatch() ? match.captured(1) : "";

    // prefixを正規化（クラッシュ防止のため余分な記号を除去）
    prefix = prefix.remove(QRegularExpression("[^A-Za-z0-9_<>#.:/]"));

    if (prefix.isEmpty()) {
        completer->popup()->hide();
        return;
    }

    // --- ⑤ 補完候補リストを更新 ---
    completer->setCompletionPrefix(prefix);
    completer->popup()->setCurrentIndex(completer->completionModel()->index(0, 0));

    // --- ⑥ UIが安定してから補完を表示 ---
    QTimer::singleShot(0, this, [this]() {
        if (!completer || !completer->popup())
            return;

        QRect cr = cursorRect();
        cr.setWidth(
            completer->popup()->sizeHintForColumn(0) +
            completer->popup()->verticalScrollBar()->sizeHint().width());
        completer->complete(cr);
    });
}

void CodeEditor::insertCompletion(const QString &completion)
{
    if (!completer || completer->widget() != this)
        return;

    QTextCursor tc = textCursor();

    QTextBlock block = tc.block();
    QString lineText = block.text().left(tc.positionInBlock());

    QRegularExpression re("([A-Za-z0-9_<>#.:/]+)$");
    QRegularExpressionMatch match = re.match(lineText);
    QString prefix = match.hasMatch() ? match.captured(1) : "";

    if (!prefix.isEmpty()) {
        tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, prefix.length());
        tc.removeSelectedText();
    }

    tc.insertText(completion);
    setTextCursor(tc);

    completer->popup()->hide();
}

// ヘルパ: ドキュメントの任意位置から文字列を取得
static inline QString textAt(QTextDocument* doc, int pos, int len) {
    QTextCursor c(doc);
    c.setPosition(pos);
    c.setPosition(pos + len, QTextCursor::KeepAnchor);
    return c.selectedText();
}

void CodeEditor::toggleComment() {
    QTextCursor cur = textCursor();
    cur.beginEditBlock();

    if (!cur.hasSelection()) {
        // 単一行は // のトグル
        cur.movePosition(QTextCursor::StartOfLine);
        cur.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
        QString line = cur.selectedText();
        QStringView view(line);
        cur.movePosition(QTextCursor::StartOfLine);

        int idx = line.indexOf("//");
        if (idx >= 0) {
            // 解除（// と直後の空白1つを削除）
            cur.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, idx);
            cur.deleteChar(); // '/'
            cur.deleteChar(); // '/'
            if (view.mid(idx + 2).startsWith(u' ')) cur.deleteChar();
        } else {
            // 追加
            cur.insertText("// ");
        }
        cur.endEditBlock();
        return;
    }

    // === ここから範囲選択: ブロックコメント /* ... */ をトグル ===
    // 「先頭行の先頭」に開始トークンを、「最終行の末尾」に終了トークンを置くルール
    int selStart = cur.selectionStart();
    int selEnd   = cur.selectionEnd();

    QTextCursor head(document());
    head.setPosition(selStart);
    head.movePosition(QTextCursor::StartOfBlock);
    const int blockStartPos = head.position();

    QTextCursor tail(document());
    // selectionEnd が行頭に居る場合は直前の行を末尾にする
    int adjustEndPos = std::max(selStart, selEnd - 1);
    tail.setPosition(adjustEndPos);
    tail.movePosition(QTextCursor::EndOfBlock);
    const int blockEndPos = tail.position();

    // すでに /* ... */ で囲まれているか判定（行頭「/* 」or「/*」, 行末「 */」or「*/」）
    bool hasStart2 = (textAt(document(), blockStartPos, 2) == "/*");
    bool hasStart3 = (textAt(document(), blockStartPos, 3) == "/* ");
    bool hasEnd2   = (blockEndPos >= 2 && textAt(document(), blockEndPos - 2, 2) == "*/");
    bool hasEnd3   = (blockEndPos >= 3 && textAt(document(), blockEndPos - 3, 3) == " */");

    bool hasBlockPair = ( (hasStart2 || hasStart3) && (hasEnd2 || hasEnd3) );

    if (hasBlockPair) {
        // 解除は「末尾を先に」「次に先頭」を削除（先頭を消すと末尾位置がズレるため）
        int endLen   = hasEnd3 ? 3 : 2;
        int startLen = hasStart3 ? 3 : 2;

        // 末尾の */ / " */" を削除
        QTextCursor delTail(document());
        delTail.setPosition(blockEndPos - endLen);
        delTail.setPosition(blockEndPos, QTextCursor::KeepAnchor);
        delTail.removeSelectedText();

        // 先頭の /* / "/* " を削除
        QTextCursor delHead(document());
        delHead.setPosition(blockStartPos);
        delHead.setPosition(blockStartPos + startLen, QTextCursor::KeepAnchor);
        delHead.removeSelectedText();
    } else {
        // 追加：先頭行頭に "/* "、末尾に " */"
        QTextCursor insHead(document());
        insHead.setPosition(blockStartPos);
        insHead.insertText("/* ");

        QTextCursor insTail(document());
        // 先頭で3文字増えたぶんだけ末尾位置をずらす
        insTail.setPosition(blockEndPos + 3);
        insTail.insertText(" */");
    }

    cur.endEditBlock();
}

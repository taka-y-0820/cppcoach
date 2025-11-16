#include "LineNumberArea.h"
#include "CodeEditor.h"
#include <QPaintEvent>

void LineNumberArea::paintEvent(QPaintEvent *event) {
    codeEditor->lineNumberAreaPaintEvent(event);
}

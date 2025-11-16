#ifndef LINENUMBERAREA_H
#define LINENUMBERAREA_H

#include "CodeEditor.h"

class CodeEditor;

class LineNumberArea : public QWidget {
public:
    LineNumberArea(CodeEditor *editor)
        : QWidget(editor), codeEditor(editor) {}

    QSize sizeHint() const override {
        return QSize(codeEditor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    CodeEditor *codeEditor;
};



#endif // LINENUMBERAREA_H

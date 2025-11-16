#ifndef SAFECOMPLETER_H
#define SAFECOMPLETER_H

#include <QCompleter>

class SafeCompleter : public QCompleter
{
    Q_OBJECT
public:
    SafeCompleter(QObject *parent = nullptr)
        : QCompleter(parent) {}

protected:
    // QPlainTextEdit と競合する focusOut を無効化
    bool eventFilter(QObject *obj, QEvent *e) override;
};

#endif

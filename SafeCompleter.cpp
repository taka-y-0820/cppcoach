#include "SafeCompleter.h"
#include <QEvent>

bool SafeCompleter::eventFilter(QObject *obj, QEvent *e)
{
    if (e->type() == QEvent::FocusOut) {
        // popup が消える時の競合を無視
        return false;
    }
    return QCompleter::eventFilter(obj, e);
}

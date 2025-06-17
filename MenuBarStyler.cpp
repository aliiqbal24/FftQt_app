#include "MenuBarStyler.h"
#include <QMenu>

void MenuBarStyler::applyDarkTheme(QMenuBar *menuBar)
{
    if (!menuBar)
        return;

    QColor lightGray(183, 182, 191);
    QColor darkGray(13, 13, 13);

    menuBar->setStyleSheet(QString(
        "QMenuBar { background-color: rgb(%1,%2,%3); color: rgb(%4,%5,%6); }"
        "QMenuBar::item:selected { background-color: rgb(40,40,40); }"
        ).arg(darkGray.red()).arg(darkGray.green()).arg(darkGray.blue())
         .arg(lightGray.red()).arg(lightGray.green()).arg(lightGray.blue()));

    // Apply the same colors to drop-down menus
    const QString menuStyle = QString(
        "QMenu { background-color: rgb(%1,%2,%3); color: rgb(%4,%5,%6); }"
        "QMenu::item:selected { background-color: rgb(40,40,40); }"
        ).arg(darkGray.red()).arg(darkGray.green()).arg(darkGray.blue())
         .arg(lightGray.red()).arg(lightGray.green()).arg(lightGray.blue());

    for (QMenu *menu : menuBar->findChildren<QMenu*>()) {
        menu->setStyleSheet(menuStyle);
    }
}

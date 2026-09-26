"""Invoke native Nuke buttons in an unmapped floating properties window."""
from contextlib import contextmanager
import nuke


@contextmanager
def native_button(node, label):
    from PySide6.QtCore import QObject, QEvent, QEventLoop, Qt
    from PySide6.QtWidgets import QApplication, QWidget, QAbstractButton
    app=QApplication.instance()
    existing=set(QApplication.topLevelWidgets())
    protected=[]

    class PreventMapping(QObject):
        def eventFilter(self, obj, event):
            if event.type() in (QEvent.Show, QEvent.ShowToParent) and isinstance(obj,QWidget):
                window=obj.window()
                if window not in existing and window not in protected:
                    # Set before the native window is mapped; visible to Qt's
                    # event delivery, never visible in the user's Properties.
                    window.setAttribute(Qt.WA_DontShowOnScreen,True)
                    window.setAttribute(Qt.WA_ShowWithoutActivating,True)
                    protected.append(window)
            return False

    guard=PreventMapping(app)
    app.installEventFilter(guard)
    try:
        try:
            # Floating avoids replacing/scrolling any existing Properties panel.
            nuke.show(node,forceFloat=True)
            QApplication.processEvents(QEventLoop.ExcludeUserInputEvents)
        finally:
            app.removeEventFilter(guard)
        buttons=[w for w in QApplication.allWidgets() if isinstance(w,QAbstractButton)
                 and w.window() in protected and w.text().replace('&','')==label and w.isEnabled()]
        if len(buttons)!=1:
            raise RuntimeError('Could not prepare hidden native button: '+label)
        yield buttons[0]
    finally:
        node.hideControlPanel()
        for window in protected:
            try:window.close()
            except RuntimeError:pass
        guard.deleteLater()

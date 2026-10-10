"""Qt Quick front end for the Windows Bloodborne launcher."""
from pathlib import Path
import sys

from PySide6.QtCore import QUrl
from PySide6.QtGui import QIcon
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtQuickControls2 import QQuickStyle
from PySide6.QtWidgets import QApplication

from bbport_launcher_backend import LauncherBackend


def run_ui():
    QQuickStyle.setStyle('Basic')
    app = QApplication(sys.argv)
    app.setApplicationName('Bloodborne')
    app.setWindowIcon(QIcon(str(Path(__file__).resolve().parent / 'assets' / 'hunter-icon.ico')))
    backend = LauncherBackend()
    backend.connect_workers()
    engine = QQmlApplicationEngine()
    engine.rootContext().setContextProperty('launcher', backend)
    path = Path(__file__).resolve().parent / 'qml' / 'Main.qml'
    if getattr(sys, 'frozen', False):
        import bbport_launcher_win as core
        path = core.PORT_DIR / 'launcher' / 'qml' / 'Main.qml'
    engine.load(QUrl.fromLocalFile(str(path)))
    if not engine.rootObjects():
        raise RuntimeError(f'Could not load Qt Quick launcher: {path}')
    backend.closeRequested.connect(engine.rootObjects()[0].close)
    return app.exec()

if __name__ == "__main__":
    sys.exit(run_ui())

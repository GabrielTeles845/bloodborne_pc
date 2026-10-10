"""UI-independent settings, launch and file operations for the Windows launcher.

The Qt Quick view talks to this object through a small set of slots. The game still
uses the same settings.json, bbport.ini, mods.json and run.py as the original UI.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import threading
import time
import urllib.request
import webbrowser
import zipfile

from PySide6.QtCore import QObject, Property, Signal, Slot
from PySide6.QtGui import QGuiApplication
from PySide6.QtWidgets import QFileDialog, QMessageBox

import bbport_launcher_win as core
from bbport_pkg_setup import PackageSetupError, install_game
from bbport_readiness import check_readiness, valid_trophy_key


def choice_rows(options):
    return [{'value': str(value), 'text': label[0]} for value, label in options]


def row(key, label, kind='text', options=None, hint=''):
    return {'key': key, 'label': label, 'kind': kind, 'options': choice_rows(options or []), 'hint': hint}


FIELDS = {
    'Graphics': [
        row('upscaler', 'Upscaler', 'choice', core.UPSCALERS),
        row('preset', 'Upscaling preset', 'choice', core.PRESETS),
        row('output_res', 'Output resolution', 'choice', core.OUTPUTS),
        row('live_resolution', 'Live resolution changes', 'choice', core.LIVE),
        row('sharpen', 'Sharpening (RCAS)', 'toggle'),
        row('sharpness', 'Sharpness (0.00–1.00)'),
        row('object_motion', 'Object motion vectors', 'toggle'),
        row('model_lod', 'Model detail', 'choice', core.LODS),
        row('show_fps', 'Show FPS counter', 'toggle'),
        *[row(key, title[0], 'toggle') for key, title, _ in core.EFFECTS],
    ],
    'Display': [
        row('fps_mode', 'Frame rate', 'choice', core.FPS_MODES),
        row('frame_cap', 'Unlocked mode limit', 'choice', core.FRAME_CAPS),
        row('frames_ahead', 'Frames ahead', 'choice', core.FRAMES_AHEAD),
        row('fullscreen', 'Fullscreen', 'toggle'),
        row('present_mode', 'Presentation mode', 'choice', core.PRESENT_MODES),
        row('hdr', 'HDR output', 'toggle'),
    ],
    'Game': [
        row('game_dir', 'Game folder (contains eboot.bin)', 'folder'),
        row('user_dir', 'Saves folder', 'folder'),
        row('language', 'Game language', 'choice', core.LANGUAGES),
        row('player_name', 'Hunter name'),
        *[row(key, title[0], 'toggle') for key, title, _ in core.EXTRAS[:1]],
    ],
    'Controller': [
        row('debug_camera', 'Free camera', 'toggle'),
        row('debug_menu', 'Debug menu', 'toggle'),
    ],
    'Cheats': [
        *[row(key, title[0], 'toggle') for key, title, _ in core.CHEATS],
        *[row(key, title[0], 'toggle') for key, title, _ in core.TWEAKS],
    ],
    'Online': [
        row('online', 'Play online', 'toggle'),
        row('online_community', 'Community game server'),
        row('online_server', 'shadNet server'),
        row('online_webapi', 'shadNet WebAPI'),
        row('online_npid', 'Online account ID'),
        row('online_password', 'Online password', 'password'),
        row('online_upnp', 'UPnP', 'toggle'),
    ],
    'Mods': [
        row('mods_enabled', 'Enable mods', 'toggle'),
        row('mods_dir', 'Mods folder', 'folder'),
        row('patches_dir', 'External patches folder', 'folder'),
    ],
    'Advanced': [
        row('ui_language', 'Launcher language', 'choice', [('', ('System default',))] + core.UI_LANGUAGES),
        row('close_on_play', 'Close launcher after Play', 'toggle'),
        row('check_updates', 'Check for updates', 'toggle'),
        row('draw_pipe', 'Draw pipeline', 'choice', core.DRAW_PIPE),
        row('readbacks', 'GPU readbacks', 'choice', core.READBACKS),
        row('frame_stats', 'Frame statistics', 'toggle'),
        row('gpu_profile', 'GPU profiling', 'toggle'),
        row('vk_validation', 'Vulkan validation', 'toggle'),
        row('extra_env', 'Extra environment variables'),
    ],
}


class LauncherBackend(QObject):
    changed = Signal()
    logChanged = Signal()
    noticeChanged = Signal()
    runningChanged = Signal()
    closeRequested = Signal()
    updateAvailable = Signal(str, str, str)
    workerNotice = Signal(str)
    gpuDetected = Signal(str)
    connectionResult = Signal(str)
    setupRunningChanged = Signal()
    setupCompleted = Signal(str)
    setupFailed = Signal(str)

    def __init__(self):
        super().__init__()
        self.app = {**core.APP_DEFAULTS, **core.load_json(core.CONFIG_FILE, {})}
        self.ini, self.ini_lines = core.load_ini()
        self.process = None
        self.job = None
        self._log = ''
        self._notice = 'Ready to hunt.'
        self._mods = []
        self._patches = []
        self._update = None
        self._gpu_status = 'Checking graphics card…'
        self._setup_running = False
        self.workerNotice.connect(self._set_notice)
        self.updateAvailable.connect(self._offer_update)
        self.gpuDetected.connect(self._set_gpu_status)
        self.connectionResult.connect(self._show_connection_result)
        self.setupCompleted.connect(self._complete_game_setup)
        self.setupFailed.connect(self._fail_game_setup)
        self.refresh_addons()
        threading.Thread(target=self._detect_gpu, daemon=True).start()
        if self.app.get('check_updates'):
            self.checkForUpdates(False)

    @Property(str, notify=noticeChanged)
    def notice(self):
        return self._notice

    @Property(str, constant=True)
    def version(self):
        return core.VERSION

    @Property(str, notify=logChanged)
    def log(self):
        return self._log

    @Property(bool, notify=runningChanged)
    def running(self):
        return self.process is not None

    @Property(bool, notify=setupRunningChanged)
    def setupRunning(self):
        return self._setup_running

    @Property(str, notify=changed)
    def gameStatus(self):
        info = core.game_info(self.app.get('game_dir'))
        return f'Game ready · version {info[1]}' if info else 'Choose your game folder to begin'

    @Property(str, notify=changed)
    def setupSummary(self):
        return (f"{self.ini.get('output_res', '1920x1080').replace('x', ' × ')}  ·  "
                f"{self.app.get('fps_mode', 'uncap').upper()} FPS  ·  "
                f"{self.ini.get('upscaler', 'dlss').upper()}")

    @Property(str, notify=changed)
    def gameFolder(self):
        return str(self.app.get('game_dir', ''))

    @Property(str, notify=changed)
    def saveStatus(self):
        return self.readiness[2]['detail']

    @Property('QVariantList', notify=changed)
    def readiness(self):
        return check_readiness(self.app.get('game_dir'),
                               self.app.get('user_dir') or core.DATA_DIR / 'user',
                               core.game_info)

    @Slot()
    def refreshReadiness(self):
        self.changed.emit()

    @Property(str, notify=changed)
    def gpuStatus(self):
        return self._gpu_status

    @Slot(str)
    def _set_gpu_status(self, status):
        self._gpu_status = status
        self.changed.emit()

    def _detect_gpu(self):
        exe = core.PORT_DIR / 'bin' / 'bb-gpu-capabilities.exe'
        if not exe.is_file():
            exe = core.PORT_DIR / 'out' / 'bb-gpu-capabilities.exe'
        if not exe.is_file():
            self.gpuDetected.emit('Graphics card check unavailable')
            return
        env = dict(os.environ)
        clang = next((p / 'msys64' / 'clang64' / 'bin' for p in core.PORT_DIR.parents
                      if (p / 'msys64' / 'clang64' / 'bin').is_dir()), None)
        if clang:
            env['PATH'] = str(clang) + os.pathsep + env.get('PATH', '')
        try:
            result = subprocess.run([str(exe), '--live-resolution'], capture_output=True, text=True,
                                    timeout=30, env=env, creationflags=core.NO_WINDOW)
            names = [line[5:].split(':')[0] for line in result.stderr.splitlines() if line.startswith('GPU: ')]
            self.gpuDetected.emit(names[0] if names else 'Vulkan graphics card not detected')
        except (OSError, subprocess.TimeoutExpired):
            self.gpuDetected.emit('Graphics card check unavailable')

    @Property('QVariantList', notify=changed)
    def mods(self):
        return self._mods

    @Property('QVariantList', notify=changed)
    def patches(self):
        return self._patches

    @Property(str, notify=changed)
    def promptChoice(self):
        prompts = [m for m in self._mods if 'xbox' in m['name'].lower() and 'prompt' in m['name'].lower()]
        return 'Xbox' if any(m['enabled'] for m in prompts) else 'PlayStation'

    @Property(str, notify=changed)
    def fsrStatus(self):
        missing = core.fsr4_missing()
        return 'FSR 4 assets ready' if not missing else f'{len(missing)} FSR 4 assets missing'

    @Slot(str, result='QVariantList')
    def fields(self, page):
        return FIELDS.get(page, [])

    @Slot(str, result='QVariant')
    def value(self, key):
        return self.ini.get(key, '') if key in core.INI_DEFAULTS else self.app.get(key, '')

    def _write(self):
        core.CONFIG_DIR.mkdir(parents=True, exist_ok=True)
        core.CONFIG_FILE.write_text(json.dumps(self.app, indent=2, ensure_ascii=False), encoding='utf-8')
        core.save_ini({k: self.ini[k] for k in core.INI_DEFAULTS}, self.ini_lines)
        self.ini, self.ini_lines = core.load_ini()
        self.changed.emit()

    @Slot()
    def installFromArchive(self):
        if self._setup_running or self.running:
            self._set_notice('Close the game or wait for the current setup to finish.')
            return
        source, _ = QFileDialog.getOpenFileName(
            None, 'Choose cusa-03173.zip or CUSA03173.pkg', str(Path.home() / 'Downloads'),
            'Game archive or base PKG (*.zip *.pkg)')
        if not source:
            return
        update = None
        if Path(source).suffix.casefold() == '.pkg':
            update, _ = QFileDialog.getOpenFileName(
                None, 'Choose CUSA03173 Update v1.09.pkg', str(Path(source).parent), 'Update PKG (*.pkg)')
            if not update:
                return
        parent = QFileDialog.getExistingDirectory(
            None, 'Choose where to create the CUSA03173 game folder', str(Path.home()))
        if not parent:
            return
        target = Path(parent) / 'CUSA03173'
        if target.exists():
            QMessageBox.warning(None, 'Game folder already exists',
                                f'{target} already exists. Choose another location so no game files are replaced.')
            return
        extractor = core.PORT_DIR / 'tools' / 'pkg-extractor' / 'pkg_extractor.exe'
        if not extractor.is_file() and not core.FROZEN:
            extractor = core.PORT_DIR / 'out' / 'vendor' / 'pkg-extractor' / 'pkg_extractor.exe'
        self._setup_running = True
        self.setupRunningChanged.emit()
        self._set_notice('Preparing your game files. Keep the launcher open…')

        def work():
            try:
                installed = install_game(source, parent, extractor, update, self.workerNotice.emit)
                self.setupCompleted.emit(str(installed))
            except PackageSetupError as error:
                self.setupFailed.emit(str(error))
            except Exception as error:
                self.setupFailed.emit(f'Unexpected installer error: {error}')
        threading.Thread(target=work, daemon=True).start()

    @Slot(str)
    def _complete_game_setup(self, folder):
        self.app['game_dir'] = folder
        self._write()
        self._setup_running = False
        self.setupRunningChanged.emit()
        info = core.game_info(folder)
        if info and info[1] == '01.09':
            self._set_notice('Game installed and version 1.09 verified. Press PLAY.')
        else:
            self._set_notice('Game installed. Check that the launcher reports version 1.09 before playing.')

    @Slot(str)
    def _fail_game_setup(self, message):
        self._setup_running = False
        self.setupRunningChanged.emit()
        self._set_notice(message)
        QMessageBox.warning(None, 'Game installation could not finish', message)

    @Slot(str, 'QVariant')
    def setValue(self, key, value):
        if key not in core.APP_DEFAULTS and key not in core.INI_DEFAULTS:
            return
        if key in core.INI_DEFAULTS:
            if key in core.INI_FLAGS:
                value = '1' if value in (True, 1, '1', 'true') else '0'
            elif key == 'sharpness':
                try:
                    value = f'{max(0.0, min(1.0, float(value))):.2f}'
                except (ValueError, TypeError):
                    value = self.ini.get(key, '0.50')
            self.ini[key] = str(value)
        else:
            self.app[key] = value
        self._write()

    @Slot(str, result=bool)
    def checked(self, key):
        value = self.value(key)
        return value is True or value == '1'

    @Slot()
    def importTrophyKeys(self):
        source, _ = QFileDialog.getOpenFileName(
            None, 'Choose your trophy keys.json', '',
            'JSON files (*.json)'
        )
        if not source:
            return

        try:
            data = json.loads(Path(source).read_text(encoding='utf-8-sig'))
            trophy_key = data['TrophyKeySet']['ReleaseTrophyKey']
            if not valid_trophy_key(trophy_key):
                raise ValueError('ReleaseTrophyKey must be 32 hexadecimal characters')
        except (OSError, ValueError, KeyError, TypeError) as error:
            QMessageBox.warning(None, 'Invalid trophy keys',
                                f'This is not a valid trophy keys.json: {error}')
            return

        user_dir = Path(self.app.get('user_dir') or core.DATA_DIR / 'user')
        destination = user_dir / 'keys.json'

        if destination.exists():
            if Path(source).resolve() == destination.resolve():
                self._set_notice('Trophy keys are already installed.')
                return
            answer = QMessageBox.question(
                None, 'Replace trophy keys?',
                'This folder already contains keys.json. Replace it?',
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No
            )
            if answer != QMessageBox.StandardButton.Yes:
                return

        try:
            user_dir.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
        except OSError as error:
            QMessageBox.warning(None, 'Import failed', str(error))
            return

        self._set_notice('Trophy keys installed successfully.')
        self.changed.emit()
    @Slot(str)
    def browse(self, key):
        if key not in ('game_dir', 'user_dir', 'mods_dir', 'patches_dir'):
            return
        folder = QFileDialog.getExistingDirectory(None, f'Choose {key.replace("_", " ")}', str(self.value(key) or core.DATA_DIR))
        if folder:
            self.setValue(key, folder)
            if key in ('mods_dir', 'patches_dir'):
                self.refresh_addons()

    @Slot(str)
    def openFolder(self, key):
        path = self.value(key) if key else core.PORT_DIR
        if not path:
            path = core.DATA_DIR / {'user_dir': 'user', 'mods_dir': 'mods', 'patches_dir': 'patches'}.get(key, '')
        path = Path(path)
        if not path.exists():
            path.mkdir(parents=True, exist_ok=True)
        os.startfile(str(path))

    def _set_notice(self, message):
        self._notice = message
        self.noticeChanged.emit()

    def _append_log(self, value):
        self._log = (self._log + value)[-350000:]
        self.logChanged.emit()

    @Slot()
    def play(self):
        if self._setup_running:
            self._set_notice('Wait for the game files to finish installing.')
            return
        if self.process:
            return
        if not core.game_info(self.app.get('game_dir')):
            self._set_notice('Choose the game folder containing eboot.bin first.')
            return
        if self.app.get('online') and '@' in str(self.app.get('online_npid', '')):
            self._set_notice('Online ID must be your account name, not an email address.')
            return
        self._write()
        self._log = ''
        self.logChanged.emit()
        try:
            self.process = subprocess.Popen(core.run_command(), cwd=core.PORT_DIR,
                                            env=core.game_environment(self.app), stdin=subprocess.DEVNULL,
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                            creationflags=core.NO_WINDOW)
        except OSError as error:
            self._set_notice(f'Could not start: {error}')
            self.process = None
            return
        self.job = core.GameJob(self.process)
        self.runningChanged.emit()
        self._set_notice('Preparing the game…')
        threading.Thread(target=self._read_output, args=(self.process,), daemon=True).start()
        if self.app.get('close_on_play'):
            from PySide6.QtCore import QTimer
            QTimer.singleShot(5000, self.closeRequested.emit)

    def _read_output(self, process):
        log_dir = Path(self.app.get('user_dir') or core.DATA_DIR / 'user')
        log_dir.mkdir(parents=True, exist_ok=True)
        with open(log_dir / 'last_run.log', 'w', encoding='utf-8', buffering=1) as log:
            for raw in iter(process.stdout.readline, b''):
                line = raw.decode('utf-8', errors='replace')
                log.write(line)
                self._outputLine.emit(line)
        self._gameExited.emit(process.wait())

    _outputLine = Signal(str)
    _gameExited = Signal(int)

    @Slot(str)
    def _receive_line(self, line):
        self._append_log(line)
        if 'Entering original x86-64 code' in line:
            self._set_notice('The game is running.')
        elif 'restarting through run.py' in line:
            self._set_notice('Restarting with the new settings…')

    @Slot(int)
    def _receive_exit(self, code):
        self._append_log(f'\n— The game exited (code {code}) —\n')
        self.process = None
        if self.job:
            self.job.close()
            self.job = None
        self.runningChanged.emit()
        self._set_notice('Ready to hunt.')

    def connect_workers(self):
        self._outputLine.connect(self._receive_line)
        self._gameExited.connect(self._receive_exit)

    @Slot()
    def stop(self):
        if self.job:
            self.job.terminate()

    @Slot()
    def refresh_addons(self):
        from mods import discover
        from patches import external_patches
        folder = Path(self.app.get('mods_dir') or core.DATA_DIR / 'mods')
        profile = core.load_json(core.DATA_DIR / 'mods.json', {})
        disabled = set(profile.get('disabled', []))
        names = discover(folder)
        order = [n for n in profile.get('order', []) if n in names]
        order += [n for n in sorted(names) if n not in order]
        self._mods = [{'name': n, 'enabled': n not in disabled} for n in order]
        patch_profile = core.load_json(core.DATA_DIR / 'patches.json', {})
        patch_folder = Path(self.app.get('patches_dir') or core.DATA_DIR / 'patches')
        found = external_patches(patch_folder, core.PATCH_VERSION)
        enabled = set(patch_profile.get('enabled', []))
        denied = set(patch_profile.get('disabled', []))
        self._patches = [{'name': name,
                          'enabled': name in enabled or (name not in denied and
                                       meta.get('isEnabled', 'false').lower() == 'true')}
                         for name, _path, meta in found]
        self.changed.emit()

    @Slot(str, bool)
    def setMod(self, name, enabled):
        profile_file = core.DATA_DIR / 'mods.json'
        profile_file.parent.mkdir(parents=True, exist_ok=True)
        profile = core.load_json(profile_file, {})
        names = [m['name'] for m in self._mods]
        if name not in names:
            return
        disabled = set(profile.get('disabled', []))
        (disabled.discard if enabled else disabled.add)(name)
        profile.update({'order': names, 'disabled': sorted(disabled)})
        profile_file.write_text(json.dumps(profile, indent=2), encoding='utf-8')
        self.refresh_addons()
        self._set_notice('Mod change saved. Restart the game to apply it.')

    @Slot(str, int)
    def moveMod(self, name, step):
        names = [m['name'] for m in self._mods]
        if name not in names:
            return
        index = names.index(name)
        other = index + step
        if not 0 <= other < len(names):
            return
        names[index], names[other] = names[other], names[index]
        profile_file = core.DATA_DIR / 'mods.json'
        profile_file.parent.mkdir(parents=True, exist_ok=True)
        profile = core.load_json(profile_file, {})
        profile['order'] = names
        profile_file.write_text(json.dumps(profile, indent=2), encoding='utf-8')
        self.refresh_addons()

    @Slot(str, bool)
    def setPatch(self, name, enabled):
        profile_file = core.DATA_DIR / 'patches.json'
        profile_file.parent.mkdir(parents=True, exist_ok=True)
        profile = core.load_json(profile_file, {})
        enabled_names = set(profile.get('enabled', []))
        disabled_names = set(profile.get('disabled', []))
        (enabled_names.add if enabled else enabled_names.discard)(name)
        (disabled_names.discard if enabled else disabled_names.add)(name)
        profile.update({'enabled': sorted(enabled_names), 'disabled': sorted(disabled_names)})
        profile_file.write_text(json.dumps(profile, indent=2), encoding='utf-8')
        self.refresh_addons()

    @Slot(str)
    def setPrompt(self, choice):
        prompt_mods = [m['name'] for m in self._mods if 'xbox' in m['name'].lower() and 'prompt' in m['name'].lower()]
        if choice == 'Xbox' and not prompt_mods:
            self._set_notice('Install the optional Xbox prompts mod in the Mods folder first.')
            return
        profile_file = core.DATA_DIR / 'mods.json'
        profile_file.parent.mkdir(parents=True, exist_ok=True)
        profile = core.load_json(profile_file, {})
        disabled = set(profile.get('disabled', []))
        for name in prompt_mods:
            disabled.add(name)
        if choice == 'Xbox':
            disabled.discard(prompt_mods[0])
        profile['disabled'] = sorted(disabled)
        profile_file.write_text(json.dumps(profile, indent=2), encoding='utf-8')
        self.refresh_addons()
        self._set_notice(f'{choice} button prompts selected. Restart the game to apply them.')

    @Slot()
    def downloadFsr4(self):
        missing = core.fsr4_missing()
        if not missing:
            self._set_notice('FSR 4 assets are already installed.')
            return
        self._set_notice(f'Downloading {len(missing)} FSR 4 files…')
        def work():
            try:
                folder = core.PORT_DIR / 'fsr4_shaders'
                folder.mkdir(parents=True, exist_ok=True)
                for name in missing:
                    part = folder / (name + '.part')
                    with urllib.request.urlopen(f'{core.FSR4_BASE}/{name}', timeout=60) as reply:
                        part.write_bytes(reply.read())
                    part.replace(folder / name)
                self.workerNotice.emit('FSR 4 assets installed.')
            except OSError as error:
                self.workerNotice.emit(f'FSR 4 download failed: {error}')
            self.changed.emit()
        threading.Thread(target=work, daemon=True).start()

    @Slot(bool)
    def checkForUpdates(self, manual=True):
        def work():
            try:
                version, url, page = core.latest_release()
                if core.release_is_newer(version):
                    self.updateAvailable.emit(version, url or '', page)
                elif manual:
                    self.workerNotice.emit(f'You have the latest version ({core.VERSION}).')
            except (OSError, ValueError, KeyError) as error:
                if manual:
                    self.workerNotice.emit(f'Could not check for updates: {error}')
        threading.Thread(target=work, daemon=True).start()

    @Slot(str, str, str)
    def _offer_update(self, version, url, page):
        self._update = (version, url, page)
        self._set_notice(f'Version {version} is available. Open Advanced to update.')

    @Property(bool, notify=noticeChanged)
    def hasUpdate(self):
        return self._update is not None

    @Slot()
    def installUpdate(self):
        if not self._update:
            self.checkForUpdates(True)
            return
        version, url, page = self._update
        if self.running:
            self._set_notice('Close the game before updating.')
            return
        if not core.FROZEN or not url:
            webbrowser.open(page)
            return
        if QMessageBox.question(None, 'Bloodborne',
                                f'Install version {version} now? Saves, settings and mods are kept.') != QMessageBox.Yes:
            return
        def work():
            try:
                shutil.rmtree(core.UPDATE_DIR, ignore_errors=True)
                core.UPDATE_DIR.mkdir(parents=True, exist_ok=True)
                archive = core.UPDATE_DIR / 'update.zip'
                request = urllib.request.Request(url, headers={'User-Agent': 'bbport-launcher'})
                with urllib.request.urlopen(request, timeout=60) as reply, open(archive, 'wb') as out:
                    while chunk := reply.read(1 << 20):
                        out.write(chunk)
                with zipfile.ZipFile(archive) as package:
                    package.extractall(core.UPDATE_DIR / 'new')
                new = next((p.parent for p in (core.UPDATE_DIR / 'new').rglob('Bloodborne.exe')), None)
                if not new:
                    raise OSError('Bloodborne.exe is missing from the download')
                subprocess.Popen([str(new / 'Bloodborne.exe'), '--install-update', str(core.PORT_DIR), str(os.getpid())],
                                 cwd=new, stdin=subprocess.DEVNULL, creationflags=core.NO_WINDOW)
                self.closeRequested.emit()
            except (OSError, zipfile.BadZipFile) as error:
                self.workerNotice.emit(f'Update failed: {error}')
        self._set_notice(f'Downloading version {version}…')
        threading.Thread(target=work, daemon=True).start()

    @Slot()
    def createShortcut(self):
        if core.FROZEN:
            target, arguments, icon = sys.executable, '', f'{sys.executable},0'
        else:
            pythonw = Path(sys.executable).with_name('pythonw.exe')
            target = str(pythonw if pythonw.exists() else sys.executable)
            arguments = str(Path(core.__file__).resolve())
            icon = str(core.PORT_DIR / 'launcher' / 'bloodborne.ico')
        def ps_string(value):
            return "'" + str(value).replace("'", "''") + "'"
        script = ('$s=(New-Object -ComObject WScript.Shell).CreateShortcut([Environment]::GetFolderPath("Desktop")'
                  '+"\\Bloodborne.lnk");'
                  f'$s.TargetPath={ps_string(target)};$s.WorkingDirectory={ps_string(core.PORT_DIR)};'
                  f'$s.IconLocation={ps_string(icon)};'
                  + (f'$s.Arguments={ps_string(chr(34) + arguments + chr(34))};' if arguments else '') + '$s.Save()')
        result = subprocess.run(['powershell', '-NoProfile', '-Command', script], capture_output=True,
                                creationflags=core.NO_WINDOW)
        self._set_notice('Desktop shortcut created.' if result.returncode == 0 else
                         result.stderr.decode(errors='replace')[:300])

    @Slot()
    def addToSteam(self):
        import bbport_steam
        steam = bbport_steam.steam_folder()
        configs = bbport_steam.user_configs(steam) if steam else []
        if not configs:
            self._set_notice('Steam was not found on this PC or no account has signed in.')
            return
        if core.FROZEN:
            exe, options = core.PORT_DIR / 'Play Bloodborne.exe', ''
        else:
            pythonw = Path(sys.executable).with_name('pythonw.exe')
            exe = pythonw if pythonw.exists() else Path(sys.executable)
            options = f'"{Path(core.__file__).resolve()}" --play'
        self._write()
        restart = False
        if bbport_steam.steam_running():
            if QMessageBox.question(None, 'Bloodborne',
                                    'Steam must close while the game is added. Close Steam now?') != QMessageBox.Yes:
                return
            subprocess.Popen([str(steam / 'steam.exe'), '-shutdown'], creationflags=core.NO_WINDOW)
            for _ in range(60):
                time.sleep(0.5)
                if not bbport_steam.steam_running():
                    break
            else:
                self._set_notice('Steam did not close. Close it yourself and try again.')
                return
            time.sleep(1)
            restart = True
        try:
            for config in configs:
                bbport_steam.add_shortcut(config / 'shortcuts.vdf', 'Bloodborne', str(exe), str(core.PORT_DIR),
                                          str(exe), options)
        except (OSError, ValueError) as error:
            self._set_notice(f'Could not add the game to Steam: {error}')
            return
        if restart:
            os.startfile('steam://open/games')
        self._set_notice('Bloodborne was added to your Steam library.')

    @Slot()
    def testConnection(self):
        self._set_notice('Testing online servers…')
        def work():
            server = str(self.app['online_server']).strip() or core.APP_DEFAULTS['online_server']
            result = core.server_check(server, core.webapi_setting(self.app),
                                       core.community_address(self.app['online_community']))
            self.connectionResult.emit(result)
        threading.Thread(target=work, daemon=True).start()

    @Slot(str)
    def _show_connection_result(self, result):
        QMessageBox.information(None, 'Online connection', result)
        self._set_notice('Online connection test finished.')

    @Slot(str)
    def action(self, name):
        if name == 'openGame':
            self.openFolder('game_dir')
        elif name == 'openSaves':
            self.openFolder('user_dir')
        elif name == 'openMods':
            self.openFolder('mods_dir')
        elif name == 'openPort':
            self.openFolder('')
        elif name == 'openConfig':
            self._write()
            os.startfile(str(core.ini_path()))
        elif name == 'mod19':
            webbrowser.open('https://www.nexusmods.com/bloodborne/mods/19')
        elif name == 'releases':
            webbrowser.open(core.RELEASES_PAGE)
        elif name == 'clearCache':
            if self.running:
                self._set_notice('Close the game before clearing its cache.')
            else:
                shutil.rmtree(Path(self.app.get('user_dir') or core.DATA_DIR / 'user') / 'cache', ignore_errors=True)
                self._set_notice('Shader cache cleared. The next launch will rebuild it.')
        elif name == 'copyLog':
            QGuiApplication.clipboard().setText(self._log)
            self._set_notice('Log copied to clipboard.')
        elif name == 'clearLog':
            self._log = ''
            self.logChanged.emit()


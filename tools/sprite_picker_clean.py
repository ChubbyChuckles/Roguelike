"""Clean sprite rectangle picker (PyQt6) replacement for corrupted sprite_picker.py.

Run:  python tools/sprite_picker_clean.py

Features:
  - 16x16 tile grid, drag multi‑tile rectangle (left-drag; right click clears).
  - Metadata: Type (TREE / PLANT), Name, Number, Rarity, Canopy (only if TREE).
  - Prefix rules: plain prefix prepends; (substring) removes first occurrence.
  - Output auto‑copied:
                TREE : TYPE, NAME+NUMBER, path, c1, r1, c2, r2, rarity, canopy
                PLANT: TYPE, NAME+NUMBER, path, c1, r1, c2, r2, rarity
  - Zoom slider (25–800%) + +/-/0 keys.
  - Persistent JSON settings (.sprite_picker_settings.json) storing: last_dir,
                last_zoom, prefix, last_type, last_name, last_number, last_rarity, last_canopy
  - Recent files list (max 10) in .sprite_picker_recent.txt.
"""

from __future__ import annotations

import json, os, sys
from dataclasses import dataclass
from typing import Optional, Callable

from PyQt6.QtCore import Qt, QRect, QPoint
from PyQt6.QtGui import (
    QAction,
    QCloseEvent,
    QGuiApplication,
    QKeyEvent,
    QMouseEvent,
    QPaintEvent,
    QPainter,
    QPen,
    QPixmap,
    QColor,
    QBrush,
)
from PyQt6.QtWidgets import (
    QApplication,
    QComboBox,
    QFileDialog,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QScrollArea,
    QSlider,
    QSizePolicy,
    QSplitter,
    QStatusBar,
    QToolBar,
    QVBoxLayout,
    QWidget,
)

TILE_SIZE = 16


def find_project_root(start: str) -> str:
    cur = os.path.abspath(start)
    for _ in range(7):
        if os.path.isdir(os.path.join(cur, "assets")) and os.path.exists(
            os.path.join(cur, "CMakeLists.txt")
        ):
            return cur
        nxt = os.path.dirname(cur)
        if nxt == cur:
            break
        cur = nxt
    return os.path.abspath(start)


@dataclass
class SheetInfo:
    path: str
    pixmap: QPixmap

    @property
    def width(self):
        return self.pixmap.width()

    @property
    def height(self):
        return self.pixmap.height()

    @property
    def cols(self):
        return self.width // TILE_SIZE

    @property
    def rows(self):
        return self.height // TILE_SIZE


class SheetView(QWidget):
    def __init__(self):
        super().__init__()
        self.sheet: Optional[SheetInfo] = None
        self.sel_c1 = self.sel_r1 = self.sel_c2 = self.sel_r2 = None
        self.drag_origin: Optional[tuple[int, int]] = None
        self.hover_col = self.hover_row = None
        self.scale = 1.0
        self.on_select: Optional[Callable[[int, int, int, int], None]] = None
        self.setMouseTracking(True)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)
        self.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

    # data ---------------------------------------------------------------
    def load_sheet(self, path: str) -> bool:
        pm = QPixmap(path)
        if pm.isNull():
            return False
        self.sheet = SheetInfo(path, pm)
        self.sel_c1 = self.sel_r1 = self.sel_c2 = self.sel_r2 = None
        self.hover_col = self.hover_row = None
        self.drag_origin = None
        self._sync_size()
        self.update()
        return True

    def set_scale(self, scale: float):
        scale = max(0.25, min(scale, 8.0))
        if abs(scale - self.scale) > 1e-6:
            self.scale = scale
            self._sync_size()
            self.update()

    def _sync_size(self):
        if not self.sheet:
            self.setMinimumSize(200, 200)
            return
        w = int(self.sheet.width * self.scale)
        h = int(self.sheet.height * self.scale)
        self.setMinimumSize(w, h)
        self.resize(w, h)

    def _cell(self, pt: QPoint):
        if not self.sheet:
            return None
        c = int(pt.x() / (TILE_SIZE * self.scale))
        r = int(pt.y() / (TILE_SIZE * self.scale))
        if c < 0 or r < 0 or c >= self.sheet.cols or r >= self.sheet.rows:
            return None
        return c, r

    def _norm(self):
        if None in (self.sel_c1, self.sel_r1, self.sel_c2, self.sel_r2):
            return None
        c1 = min(self.sel_c1, self.sel_c2)
        c2 = max(self.sel_c1, self.sel_c2)
        r1 = min(self.sel_r1, self.sel_r2)
        r2 = max(self.sel_r1, self.sel_r2)
        return c1, r1, c2, r2

    def _emit(self):
        rect = self._norm()
        if rect and self.on_select:
            self.on_select(*rect)

    # events -------------------------------------------------------------
    def mousePressEvent(self, e: QMouseEvent):  # noqa: N802
        if e.button() == Qt.MouseButton.LeftButton and self.sheet:
            hit = self._cell(e.position().toPoint())
            if hit:
                c, r = hit
                self.sel_c1 = self.sel_c2 = c
                self.sel_r1 = self.sel_r2 = r
                self.drag_origin = (c, r)
                self.update()
                self._emit()
        elif e.button() == Qt.MouseButton.RightButton:
            self.sel_c1 = self.sel_r1 = self.sel_c2 = self.sel_r2 = None
            self.drag_origin = None
            self.update()
            self._emit()
        super().mousePressEvent(e)

    def mouseMoveEvent(self, e: QMouseEvent):  # noqa: N802
        if not self.sheet:
            return
        hit = self._cell(e.position().toPoint())
        self.hover_col, self.hover_row = hit if hit else (None, None)
        if self.drag_origin and hit and (hit[0], hit[1]) != (self.sel_c2, self.sel_r2):
            self.sel_c2, self.sel_r2 = hit
            self.update()
            self._emit()
        self.update()
        super().mouseMoveEvent(e)

    def mouseReleaseEvent(self, e: QMouseEvent):  # noqa: N802
        if e.button() == Qt.MouseButton.LeftButton and self.drag_origin:
            self.drag_origin = None
            self._emit()
        super().mouseReleaseEvent(e)

    def keyPressEvent(self, e: QKeyEvent):  # noqa: N802
        k = e.key()
        if k in (Qt.Key.Key_Plus, Qt.Key.Key_Equal):
            self.set_scale(self.scale * 1.25)
        elif k == Qt.Key.Key_Minus:
            self.set_scale(self.scale / 1.25)
        elif k == Qt.Key.Key_0:
            self.set_scale(1.0)
        elif k in (Qt.Key.Key_Left, Qt.Key.Key_Right, Qt.Key.Key_Up, Qt.Key.Key_Down):
            rect = self._norm()
            if rect and self.sheet:
                c1, r1, c2, r2 = rect
                w = c2 - c1
                h = r2 - r1
                dc = -1 if k == Qt.Key.Key_Left else 1 if k == Qt.Key.Key_Right else 0
                dr = -1 if k == Qt.Key.Key_Up else 1 if k == Qt.Key.Key_Down else 0
                nc1 = max(0, min(self.sheet.cols - 1, c1 + dc))
                nr1 = max(0, min(self.sheet.rows - 1, r1 + dr))
                nc2 = nc1 + w
                nr2 = nr1 + h
                if nc2 >= self.sheet.cols:
                    nc2 = self.sheet.cols - 1
                    nc1 = nc2 - w
                if nr2 >= self.sheet.rows:
                    nr2 = self.sheet.rows - 1
                    nr1 = nr2 - h
                self.sel_c1, self.sel_r1, self.sel_c2, self.sel_r2 = nc1, nr1, nc2, nr2
                self.update()
                self._emit()
                return
        super().keyPressEvent(e)

    def paintEvent(self, e: QPaintEvent):  # noqa: N802
        p = QPainter(self)
        p.fillRect(self.rect(), Qt.GlobalColor.black)
        if not self.sheet:
            p.end()
            return
        scaled = self.sheet.pixmap.scaled(
            int(self.sheet.width * self.scale),
            int(self.sheet.height * self.scale),
            Qt.AspectRatioMode.IgnoreAspectRatio,
            Qt.TransformationMode.FastTransformation,
        )
        p.drawPixmap(0, 0, scaled)
        pen = QPen(Qt.GlobalColor.darkGray)
        pen.setWidth(1)
        p.setPen(pen)
        for c in range(self.sheet.cols + 1):
            x = int(c * TILE_SIZE * self.scale)
            p.drawLine(x, 0, x, scaled.height())
        for r in range(self.sheet.rows + 1):
            y = int(r * TILE_SIZE * self.scale)
            p.drawLine(0, y, scaled.width(), y)
        if self.hover_col is not None and self.hover_row is not None:
            p.fillRect(
                QRect(
                    int(self.hover_col * TILE_SIZE * self.scale),
                    int(self.hover_row * TILE_SIZE * self.scale),
                    int(TILE_SIZE * self.scale),
                    int(TILE_SIZE * self.scale),
                ),
                Qt.GlobalColor.darkYellow,
            )
        rect = self._norm()
        if rect:
            c1, r1, c2, r2 = rect
            x = int(c1 * TILE_SIZE * self.scale)
            y = int(r1 * TILE_SIZE * self.scale)
            w = int((c2 - c1 + 1) * TILE_SIZE * self.scale)
            h = int((r2 - r1 + 1) * TILE_SIZE * self.scale)
            # Semi-transparent cyan overlay instead of opaque fill
            p.fillRect(QRect(x, y, w, h), QBrush(QColor(0, 255, 255, 70)))
            pen_sel = QPen(QColor(0, 255, 255))
            pen_sel.setWidth(2)
            p.setPen(pen_sel)
            p.drawRect(QRect(x, y, w, h))
        p.end()


class MainWindow(QMainWindow):
    def __init__(self, project_root: str, assets_root: str):
        super().__init__()
        self.setWindowTitle("Sprite Rectangle Picker (Clean)")
        # Always on top
        self.setWindowFlag(Qt.WindowType.WindowStaysOnTopHint, True)
        self.project_root = project_root
        self.assets_root = assets_root
        self._settings_path = os.path.join(
            os.path.dirname(__file__), ".sprite_picker_settings.json"
        )
        self._settings = {}
        self._last_dir = ""
        self._load_settings()
        self._build_ui()
        self._apply_styles()
        self._load_recent()
        self._update_meta_visibility()

    # UI -----------------------------------------------------------------
    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        tb = QToolBar()
        self.addToolBar(tb)
        a_open = QAction("Open", self)
        a_open.triggered.connect(self.browse)  # type: ignore
        a_in = QAction("Zoom +", self)
        a_in.triggered.connect(lambda: self._zoom_rel(1.25))  # type: ignore
        a_out = QAction("Zoom -", self)
        a_out.triggered.connect(lambda: self._zoom_rel(1 / 1.25))  # type: ignore
        a_reset = QAction("1:1", self)
        a_reset.triggered.connect(lambda: self.view.set_scale(1.0))  # type: ignore
        for a in (a_open, a_in, a_out, a_reset):
            tb.addAction(a)
        split = QSplitter()
        root.addWidget(split, 1)
        self.view = SheetView()
        self.view.on_select = self.cell_selected
        scr = QScrollArea()
        scr.setWidget(self.view)
        scr.setWidgetResizable(False)
        split.addWidget(scr)
        side = QWidget()
        side_layout = QVBoxLayout(side)
        # path
        prow = QHBoxLayout()
        prow.addWidget(QLabel("Path:"))
        self.path_edit = QLineEdit()
        prow.addWidget(self.path_edit, 1)
        bb = QPushButton("Browse")
        bb.clicked.connect(self.browse)  # type: ignore
        prow.addWidget(bb)
        side_layout.addLayout(prow)
        # recent
        rrow = QHBoxLayout()
        rrow.addWidget(QLabel("Recent:"))
        self.recent_combo = QComboBox()
        rrow.addWidget(self.recent_combo, 1)
        self.recent_combo.currentIndexChanged.connect(self._recent_chosen)  # type: ignore
        side_layout.addLayout(rrow)
        # prefix
        pxrow = QHBoxLayout()
        pxrow.addWidget(QLabel("Prefix:"))
        self.prefix_edit = QLineEdit()
        self.prefix_edit.setText(self._settings.get("prefix", ""))
        self.prefix_edit.textChanged.connect(self._prefix_changed)  # type: ignore
        pxrow.addWidget(self.prefix_edit, 1)
        side_layout.addLayout(pxrow)
        # metadata
        meta = QVBoxLayout()
        tr = QHBoxLayout()
        tr.addWidget(QLabel("Type:"))
        self.type_combo = QComboBox()
        self.type_combo.addItems(["TREE", "PLANT"])
        lt = self._settings.get("last_type", "TREE")
        if lt in ("TREE", "PLANT"):
            self.type_combo.setCurrentText(lt)
        tr.addWidget(self.type_combo)
        meta.addLayout(tr)
        nr = QHBoxLayout()
        nr.addWidget(QLabel("Name:"))
        self.name_edit = QLineEdit()
        self.name_edit.setText(self._settings.get("last_name", ""))
        nr.addWidget(self.name_edit, 1)
        meta.addLayout(nr)
        numr = QHBoxLayout()
        numr.addWidget(QLabel("Number:"))
        self.number_edit = QLineEdit()
        self.number_edit.setFixedWidth(70)
        self.number_edit.setText(self._settings.get("last_number", "1"))
        numr.addWidget(self.number_edit)
        meta.addLayout(numr)
        rr = QHBoxLayout()
        rr.addWidget(QLabel("Rarity:"))
        self.rarity_edit = QLineEdit()
        self.rarity_edit.setFixedWidth(70)
        self.rarity_edit.setText(self._settings.get("last_rarity", "1"))
        rr.addWidget(self.rarity_edit)
        meta.addLayout(rr)
        cr = QHBoxLayout()
        self.canopy_label = QLabel("Canopy:")
        cr.addWidget(self.canopy_label)
        self.canopy_edit = QLineEdit()
        self.canopy_edit.setFixedWidth(70)
        self.canopy_edit.setText(self._settings.get("last_canopy", "2"))
        cr.addWidget(self.canopy_edit)
        meta.addLayout(cr)
        side_layout.addLayout(meta)
        # output
        out = QHBoxLayout()
        out.addWidget(QLabel("Selection:"))
        self.output_edit = QLineEdit()
        self.output_edit.setReadOnly(True)
        b_copy = QPushButton("Copy Output")
        b_copy.clicked.connect(self.copy_manual)  # type: ignore
        out.addWidget(self.output_edit, 1)
        out.addWidget(b_copy)
        side_layout.addLayout(out)
        # info / zoom / preview
        self.info_label = QLabel("Image: - x - | cols: - rows: -")
        side_layout.addWidget(self.info_label)
        zrow = QHBoxLayout()
        zrow.addWidget(QLabel("Zoom"))
        self.zoom_slider = QSlider(Qt.Orientation.Horizontal)
        self.zoom_slider.setRange(25, 800)
        z = int(self._settings.get("last_zoom", 100))
        z = z if 25 <= z <= 800 else 100
        self.zoom_slider.setValue(z)
        self.zoom_slider.valueChanged.connect(self._zoom_slider_changed)  # type: ignore
        zrow.addWidget(self.zoom_slider, 1)
        side_layout.addLayout(zrow)
        self.preview_label = QLabel("No selection")
        self.preview_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.preview_label.setStyleSheet(
            "border:1px solid #444; background:#222; min-height:150px;"
        )
        side_layout.addWidget(self.preview_label, 1)
        side_layout.addStretch(1)
        split.addWidget(side)
        split.setSizes([700, 260])  # type: ignore
        self.status = QStatusBar()
        self.setStatusBar(self.status)
        # signals
        for w in (
            self.type_combo,
            self.name_edit,
            self.number_edit,
            self.rarity_edit,
            self.canopy_edit,
        ):
            if hasattr(w, "textChanged"):
                w.textChanged.connect(self._meta_changed)  # type: ignore
        self.type_combo.currentIndexChanged.connect(self._meta_changed)  # type: ignore

    # settings -----------------------------------------------------------
    def _load_settings(self):
        try:
            if os.path.isfile(self._settings_path):
                with open(self._settings_path, "r", encoding="utf-8") as f:
                    data = json.load(f)
                if isinstance(data, dict):
                    self._settings.update(data)
                self._last_dir = self._settings.get("last_dir", "")
        except Exception:
            pass

    def _save_settings(self):
        try:
            with open(self._settings_path, "w", encoding="utf-8") as f:
                json.dump(self._settings, f, indent=2)
        except Exception:
            pass

    # recent -------------------------------------------------------------
    def _recent_store_path(self, path: str):
        if not path:
            return
        paths = getattr(self, "_recent_paths", [])
        if path in paths:
            paths.remove(path)
        paths.insert(0, path)
        del paths[10:]
        self._recent_paths = paths
        self._refresh_recent_combo()
        try:
            rec = os.path.join(os.path.dirname(__file__), ".sprite_picker_recent.txt")
            with open(rec, "w", encoding="utf-8") as f:
                for p in paths:
                    f.write(p + "\n")
        except Exception:
            pass

    def _load_recent(self):
        self._recent_paths = []
        rec = os.path.join(os.path.dirname(__file__), ".sprite_picker_recent.txt")
        if os.path.isfile(rec):
            try:
                with open(rec, "r", encoding="utf-8") as f:
                    for line in f:
                        p = line.strip()
                        if p:
                            self._recent_paths.append(p)
            except Exception:
                pass
        self._refresh_recent_combo()

    def _refresh_recent_combo(self):
        self.recent_combo.blockSignals(True)
        self.recent_combo.clear()
        for p in getattr(self, "_recent_paths", []):
            rel = (
                os.path.relpath(p, self.project_root).replace("\\", "/")
                if p.startswith(self.project_root)
                else p
            )
            self.recent_combo.addItem(rel, p)
        self.recent_combo.blockSignals(False)

    def _recent_chosen(self):
        i = self.recent_combo.currentIndex()
        p = self.recent_combo.itemData(i)
        if p:
            self.path_edit.setText(p)
            self.load_path_from_edit()

    # reactions ----------------------------------------------------------
    def _zoom_slider_changed(self, val: int):
        self.view.set_scale(val / 100.0)
        self.status.showMessage(f"Zoom: {val}%", 1000)
        self._settings["last_zoom"] = val
        self._save_settings()

    def _zoom_rel(self, factor: float):
        self.view.set_scale(self.view.scale * factor)
        v = int(self.view.scale * 100)
        self.zoom_slider.blockSignals(True)
        self.zoom_slider.setValue(v)
        self.zoom_slider.blockSignals(False)
        self._settings["last_zoom"] = v
        self._save_settings()

    def _prefix_changed(self, text: str):
        self._settings["prefix"] = text
        self._save_settings()
        rect = self.view._norm()  # type: ignore
        if rect and self.view.sheet:
            self.cell_selected(*rect)

    def _meta_changed(self, *_):
        self._settings["last_type"] = self.type_combo.currentText()
        self._settings["last_name"] = self.name_edit.text()
        self._settings["last_number"] = self.number_edit.text()
        self._settings["last_rarity"] = self.rarity_edit.text()
        self._settings["last_canopy"] = self.canopy_edit.text()
        self._save_settings()
        self._update_meta_visibility()
        rect = self.view._norm()  # type: ignore
        if rect and self.view.sheet:
            self.cell_selected(*rect)

    def _update_meta_visibility(self):
        is_tree = self.type_combo.currentText() == "TREE"
        self.canopy_label.setVisible(is_tree)
        self.canopy_edit.setVisible(is_tree)

    # selection formatting -----------------------------------------------
    def cell_selected(self, c1: int, r1: int, c2: int, r2: int):
        if not self.view.sheet:
            return
        rel = os.path.relpath(self.view.sheet.path, self.project_root).replace(
            "\\", "/"
        )
        prefix = str(self._settings.get("prefix", ""))
        if prefix.startswith("(") and prefix.endswith(")"):
            removal = prefix[1:-1]
            if removal and removal in rel:
                rel = rel.replace(removal, "", 1)
            while "//" in rel:
                rel = rel.replace("//", "/")
            rel = rel.lstrip("/")
            full_path = rel
        else:
            full_path = prefix + rel if prefix and not rel.startswith(prefix) else rel
        t = self.type_combo.currentText()
        base = self.name_edit.text().strip()
        num = self.number_edit.text().strip()
        name = f"{base}{num}" if num else base
        rarity = self.rarity_edit.text().strip() or "1"
        canopy = self.canopy_edit.text().strip() or "2"
        out = (
            f"{t}, {name}, {full_path}, {c1}, {r1}, {c2}, {r2}, {rarity}, {canopy}"
            if t == "TREE"
            else f"{t}, {name}, {full_path}, {c1}, {r1}, {c2}, {r2}, {rarity}"
        )
        self.output_edit.setText(out)
        cb = QGuiApplication.clipboard()
        if cb:
            cb.setText(out)
        self.status.showMessage("Copied", 1200)
        self._update_preview(c1, r1, c2, r2)

    def _update_preview(self, c1=None, r1=None, c2=None, r2=None):
        sheet = self.view.sheet
        if not sheet:
            self.preview_label.setText("No selection")
            self.preview_label.setPixmap(QPixmap())
            return
        if None in (c1, r1, c2, r2):
            rect = self.view._norm()  # type: ignore
            if not rect:
                self.preview_label.setText("No selection")
                self.preview_label.setPixmap(QPixmap())
                return
            c1, r1, c2, r2 = rect
        x = c1 * TILE_SIZE
        y = r1 * TILE_SIZE
        w = (c2 - c1 + 1) * TILE_SIZE
        h = (r2 - r1 + 1) * TILE_SIZE
        sub = sheet.pixmap.copy(x, y, w, h)
        scaled = sub.scaled(
            160,
            160,
            Qt.AspectRatioMode.KeepAspectRatio,
            Qt.TransformationMode.FastTransformation,
        )
        self.preview_label.setPixmap(scaled)
        self.preview_label.setMinimumHeight(170)

    # file ops -----------------------------------------------------------
    def browse(self):
        start = (
            self._last_dir
            if os.path.isdir(self._last_dir)
            else (
                self.assets_root
                if os.path.isdir(self.assets_root)
                else self.project_root
            )
        )
        path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Sprite Sheet",
            start,
            "Images (*.png *.jpg *.bmp *.gif *.webp);;All Files (*)",
        )
        if path:
            self.path_edit.setText(path)
            self.load_path_from_edit()

    def load_path_from_edit(self):
        path = self.path_edit.text().strip()
        if not path:
            return
        if not os.path.isabs(path):
            cand1 = os.path.join(self.project_root, path)
            cand2 = os.path.join(self.assets_root, path)
            if os.path.exists(cand1):
                path = cand1
            elif os.path.exists(cand2):
                path = cand2
        if not os.path.isfile(path):
            QMessageBox.warning(self, "Load Failed", f"File not found: {path}")
            return
        if not self.view.load_sheet(path):
            QMessageBox.warning(self, "Load Failed", f"Failed to load image: {path}")
            return
        sheet = self.view.sheet
        assert sheet
        self.info_label.setText(
            f"Image: {sheet.width} x {sheet.height} | cols: {sheet.cols} rows: {sheet.rows}"
        )
        rel = os.path.relpath(sheet.path, self.project_root).replace("\\", "/")
        self.status.showMessage(f"Loaded {rel}", 2500)
        self._recent_store_path(sheet.path)
        self._update_preview()
        self._last_dir = os.path.dirname(sheet.path)
        self._settings["last_dir"] = self._last_dir
        self._save_settings()

    def copy_manual(self):
        # Ensure latest selection string first
        rect = self.view._norm()  # type: ignore
        if rect and self.view.sheet:
            self.cell_selected(*rect)
        txt = self.output_edit.text()
        if not txt:
            return
        cb = QGuiApplication.clipboard()
        if cb:
            cb.setText(txt)
        self.status.showMessage("Copied to clipboard", 1500)
        self._update_preview()
        # Auto-increment number field AFTER copying so clipboard has old value
        try:
            cur = int(self.number_edit.text())
            self.number_edit.setText(str(cur + 1))
        except ValueError:
            # Reset to 2 if not parseable
            self.number_edit.setText("2")
        self._settings["last_number"] = self.number_edit.text()
        self._save_settings()

    # style / close ------------------------------------------------------
    def _apply_styles(self):
        self.setStyleSheet(
            """QMainWindow{background:#202225;} QLabel{color:#d0d0d0;} QLineEdit{background:#2b2f33;color:#e0e0e0;border:1px solid #3a3f44;padding:4px;} QLineEdit:read-only{background:#24282b;} QPushButton{background:#3a3f44;color:#e0e0e0;border:1px solid #4a5055;padding:4px 8px;} QPushButton:hover{background:#445059;} QPushButton:pressed{background:#2f363b;} QToolBar{background:#2a2d31;spacing:6px;border:0;} QStatusBar{background:#2a2d31;color:#bbb;} QComboBox{background:#2b2f33;color:#dcdcdc;padding:2px 6px;} QScrollArea{background:#1e1f22;} QSlider::groove:horizontal{background:#2d3135;height:6px;border-radius:3px;} QSlider::handle:horizontal{background:#3daee9;width:14px;margin:-4px 0;border-radius:7px;} QSlider::sub-page:horizontal{background:#3daee9;}"""
        )

    def closeEvent(self, e: QCloseEvent):  # noqa: N802
        self._save_settings()
        super().closeEvent(e)


def main():  # pragma: no cover
    app = QApplication(sys.argv)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = find_project_root(script_dir)
    assets_root = os.path.join(project_root, "assets")
    win = MainWindow(project_root, assets_root)
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":  # pragma: no cover
    main()

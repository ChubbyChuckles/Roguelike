#!/usr/bin/env python3
"""
Sprite Sheet Cell Picker (PyQt6)

Features:
- Browse for a sprite sheet (file dialog rooted at project assets directory)
- Display the image with a 16x16 px grid overlay
- Click a cell to:
    * Highlight the selected cell
    * Generate text: "<relative/path/to/sheet>, <col>, <row>"
    * Place that text into the textbox for easy copy
    * Copy the text to the system clipboard automatically
- Shows image dimensions and inferred columns/rows.

Run with Python 3.12.3
Dependencies: PyQt6 (pip install PyQt6)

You can place this file anywhere inside the project; it auto-detects the project root by searching for the 'assets' folder upwards from the script location.
"""
from __future__ import annotations
import os
import sys
from dataclasses import dataclass
from typing import Optional, Callable, Tuple

from PyQt6.QtCore import Qt, QEvent
from PyQt6.QtGui import (
    QAction,
    QGuiApplication,
    QPainter,
    QPen,
    QPixmap,
    QColor,
    QMouseEvent,
    QKeyEvent,
    QPaintEvent,
)
from PyQt6.QtWidgets import (
    QApplication,
    QWidget,
    QVBoxLayout,
    QHBoxLayout,
    QPushButton,
    QFileDialog,
    QLabel,
    QLineEdit,
    QMessageBox,
    QSizePolicy,
    QScrollArea,
    QStatusBar,
    QMainWindow,
    QToolBar,
    QSlider,
    QSplitter,
    QComboBox,
)

TILE_SIZE = 16


def find_project_root(start: str) -> str:
    """Walk upwards until 'assets' directory is found, else return start."""
    cur = os.path.abspath(start)
    while True:
        if os.path.isdir(os.path.join(cur, "assets")):
            return cur
        parent = os.path.dirname(cur)
        if parent == cur:
            return start  # fallback
        cur = parent


@dataclass
class SpriteSheetInfo:
    path: str
    pixmap: QPixmap

    @property
    def width(self) -> int:
        return self.pixmap.width()

    @property
    def height(self) -> int:
        return self.pixmap.height()

    @property
    def cols(self) -> int:
        return self.width // TILE_SIZE

    @property
    def rows(self) -> int:
        return self.height // TILE_SIZE


class SpriteSheetView(QWidget):
    """Widget that draws the sprite sheet with 16x16 grid and manages selection.

    Enhancements:
    - Zoom support
    - Keyboard navigation (single-cell)
    - Drag rectangle selection (cell to cell)
    - Callback `on_select(col1,row1,col2,row2)` always provides normalized rectangle
    - Hover highlight & selection overlay
    """

    def __init__(self, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self.setMouseTracking(True)
        # Core image
        self.sheet: Optional[SpriteSheetInfo] = None
        # Anchor cell (kept for potential future single cell features)
        self.selected_col: Optional[int] = None
        self.selected_row: Optional[int] = None
        # Rectangle selection corners
        self.sel_c1: Optional[int] = None
        self.sel_r1: Optional[int] = None
        self.sel_c2: Optional[int] = None
        self.sel_r2: Optional[int] = None
        # Drag state
        self.dragging: bool = False
        self.drag_start_col: Optional[int] = None
        self.drag_start_row: Optional[int] = None
        # Hover cell
        self.hover_col: Optional[int] = None
        self.hover_row: Optional[int] = None
        # Zoom
        self.scale: float = 1.0
        # Selection callback: (c1,r1,c2,r2)
        self.on_select: Optional[Callable[[int, int, int, int], None]] = None
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)
        self.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

    # --- public API ---
    def load_sheet(self, path: str) -> bool:
        pm = QPixmap(path)
        if pm.isNull():
            return False
        self.sheet = SpriteSheetInfo(path=path, pixmap=pm)
        self.selected_col = self.selected_row = None
        self.hover_col = self.hover_row = None
        self._sync_size()
        self.update()
        return True

    def set_scale(self, scale: float) -> None:
        scale = max(0.25, min(scale, 8.0))
        if abs(scale - self.scale) > 1e-6:
            self.scale = scale
            self._sync_size()
            self.update()

    def zoom_in(self) -> None:
        self.set_scale(self.scale * 1.25)

    def zoom_out(self) -> None:
        self.set_scale(self.scale / 1.25)

    def reset_zoom(self) -> None:
        self.set_scale(1.0)

    # --- helpers ---
    def _sync_size(self) -> None:
        if self.sheet:
            self.setFixedSize(
                int(self.sheet.width * self.scale), int(self.sheet.height * self.scale)
            )

    def _cell_at_pos(self, x: float, y: float) -> Optional[Tuple[int, int]]:
        if not self.sheet:
            return None
        if self.scale != 0:
            x /= self.scale
            y /= self.scale
        col = int(x // TILE_SIZE)
        row = int(y // TILE_SIZE)
        if 0 <= col < self.sheet.cols and 0 <= row < self.sheet.rows:
            return col, row
        return None

    def _normalize_rect(self) -> Optional[Tuple[int, int, int, int]]:
        if (
            self.sel_c1 is None
            or self.sel_c2 is None
            or self.sel_r1 is None
            or self.sel_r2 is None
        ):
            return None
        c1 = min(self.sel_c1, self.sel_c2)
        c2 = max(self.sel_c1, self.sel_c2)
        r1 = min(self.sel_r1, self.sel_r2)
        r2 = max(self.sel_r1, self.sel_r2)
        return c1, r1, c2, r2

    def _emit_selection(self) -> None:
        rect = self._normalize_rect()
        if self.on_select and rect:
            c1, r1, c2, r2 = rect
            self.on_select(c1, r1, c2, r2)

    # --- events ---
    def mousePressEvent(self, a0: Optional[QMouseEvent]) -> None:  # noqa: N802
        if not a0:
            return
        if a0.button() == Qt.MouseButton.LeftButton:
            hit = self._cell_at_pos(a0.position().x(), a0.position().y())
            if hit:
                c, r = hit
                self.selected_col, self.selected_row = c, r
                self.drag_start_col, self.drag_start_row = c, r
                # Initialize rectangle to single cell
                self.sel_c1 = c
                self.sel_r1 = r
                self.sel_c2 = c
                self.sel_r2 = r
                self.dragging = True
                self.setFocus()
                self.update()
        elif a0.button() == Qt.MouseButton.RightButton:
            # Clear selection
            self.selected_col = self.selected_row = None
            self.sel_c1 = self.sel_c2 = self.sel_r1 = self.sel_r2 = None
            self.dragging = False
            self.update()

    def mouseMoveEvent(self, a0: Optional[QMouseEvent]) -> None:  # noqa: N802
        if not a0:
            return
        hit = self._cell_at_pos(a0.position().x(), a0.position().y())
        changed = False
        if hit:
            col, row = hit
            if col != self.hover_col or row != self.hover_row:
                self.hover_col, self.hover_row = col, row
                changed = True
            if (
                self.dragging
                and self.drag_start_col is not None
                and self.drag_start_row is not None
            ):
                # Update drag rectangle end
                if (col, row) != (self.sel_c2, self.sel_r2):
                    self.sel_c2, self.sel_r2 = col, row
                    changed = True
        else:
            if self.hover_col is not None:
                self.hover_col = self.hover_row = None
                changed = True
        if changed:
            self.update()

    def mouseReleaseEvent(self, a0: Optional[QMouseEvent]) -> None:  # noqa: N802
        if not a0:
            return
        if a0.button() == Qt.MouseButton.LeftButton and self.dragging:
            self.dragging = False
            # Emit final rectangle
            self._emit_selection()

    def leaveEvent(self, a0: Optional[QEvent]) -> None:  # noqa: N802
        self.hover_col = None
        self.hover_row = None
        self.update()
        if a0:
            super().leaveEvent(a0)

    def keyPressEvent(self, a0: Optional[QKeyEvent]) -> None:  # noqa: N802
        if not self.sheet:
            return
        if not a0:
            return
        # Require full rectangle to manipulate
        if None in (self.sel_c1, self.sel_c2, self.sel_r1, self.sel_r2):
            return
        dx = dy = 0
        if a0.key() == Qt.Key.Key_Left:
            dx = -1
        elif a0.key() == Qt.Key.Key_Right:
            dx = 1
        elif a0.key() == Qt.Key.Key_Up:
            dy = -1
        elif a0.key() == Qt.Key.Key_Down:
            dy = 1
        if dx or dy:
            # All non-None asserted above
            assert (
                self.sel_c1 is not None
                and self.sel_c2 is not None
                and self.sel_r1 is not None
                and self.sel_r2 is not None
            )
            width = self.sel_c2 - self.sel_c1
            height = self.sel_r2 - self.sel_r1
            c1 = max(0, min(self.sheet.cols - 1 - width, self.sel_c1 + dx))
            r1 = max(0, min(self.sheet.rows - 1 - height, self.sel_r1 + dy))
            c2 = c1 + width
            r2 = r1 + height
            if (c1, r1, c2, r2) != (self.sel_c1, self.sel_r1, self.sel_c2, self.sel_r2):
                self.sel_c1, self.sel_r1, self.sel_c2, self.sel_r2 = c1, r1, c2, r2
                self.update()
                self._emit_selection()

    def paintEvent(self, a0: Optional[QPaintEvent]) -> None:  # noqa: N802
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing, False)
        painter.fillRect(self.rect(), QColor(30, 32, 34))
        if self.sheet:
            if self.scale != 1.0:
                painter.save()
                painter.scale(self.scale, self.scale)
            painter.drawPixmap(0, 0, self.sheet.pixmap)
            grid_color = QColor(80, 180, 200, 110)
            pen = QPen(grid_color)
            pen.setWidth(1)
            painter.setPen(pen)
            for x in range(0, self.sheet.width + 1, TILE_SIZE):
                painter.drawLine(int(x), 0, int(x), self.sheet.height)
            for y in range(0, self.sheet.height + 1, TILE_SIZE):
                painter.drawLine(0, int(y), self.sheet.width, int(y))
            # Hover highlight
            if self.hover_col is not None and self.hover_row is not None:
                painter.fillRect(
                    self.hover_col * TILE_SIZE,
                    self.hover_row * TILE_SIZE,
                    TILE_SIZE,
                    TILE_SIZE,
                    QColor(255, 255, 0, 70),
                )
            # Selection highlight (rectangle)
            rect = self._normalize_rect()
            if rect:
                c1, r1, c2, r2 = rect
                x = c1 * TILE_SIZE
                y = r1 * TILE_SIZE
                w = (c2 - c1 + 1) * TILE_SIZE
                h = (r2 - r1 + 1) * TILE_SIZE
                painter.fillRect(x, y, w, h, QColor(0, 150, 255, 60))
                sel_pen = QPen(QColor(0, 180, 255, 180))
                sel_pen.setWidth(2)
                painter.setPen(sel_pen)
                painter.drawRect(x, y, w, h)
            if self.scale != 1.0:
                painter.restore()
        else:
            painter.setPen(QPen(QColor(160, 160, 160)))
            painter.drawText(
                self.rect(), Qt.AlignmentFlag.AlignCenter, "No sprite sheet loaded"
            )
        painter.end()


class MainWindow(QMainWindow):
    def __init__(self, project_root: str, assets_root: str):
        super().__init__()
        self.project_root = project_root
        self.assets_root = assets_root
        self.setWindowTitle("Sprite Sheet Picker (16x16)")
        self.view = SpriteSheetView()
        self.view.on_select = self.cell_selected

        # Root widget layout uses splitter for modern side panel
        central = QWidget()
        root_layout = QVBoxLayout(central)

        # --- Toolbar ---
        toolbar = QToolBar()
        toolbar.setMovable(False)
        self.addToolBar(toolbar)

        open_act = QAction("Open", self)
        open_act.setShortcut("Ctrl+O")
        open_act.triggered.connect(self.browse)  # type: ignore
        toolbar.addAction(open_act)  # type: ignore

        zoom_in_act = QAction("Zoom +", self)
        zoom_in_act.setShortcut("Ctrl++")
        zoom_in_act.triggered.connect(self.view.zoom_in)  # type: ignore
        toolbar.addAction(zoom_in_act)  # type: ignore

        zoom_out_act = QAction("Zoom -", self)
        zoom_out_act.setShortcut("Ctrl+-")
        zoom_out_act.triggered.connect(self.view.zoom_out)  # type: ignore
        toolbar.addAction(zoom_out_act)  # type: ignore

        reset_zoom_act = QAction("1:1", self)
        reset_zoom_act.setShortcut("Ctrl+0")
        reset_zoom_act.triggered.connect(self.view.reset_zoom)  # type: ignore
        toolbar.addAction(reset_zoom_act)  # type: ignore

        # --- Path controls row ---
        path_row = QHBoxLayout()
        self.recent_combo = QComboBox()
        self.recent_combo.setEditable(False)
        self.recent_combo.currentIndexChanged.connect(self._recent_chosen)  # type: ignore
        self.path_edit = QLineEdit()
        self.path_edit.setPlaceholderText("Sprite sheet path (relative or absolute)")
        browse_btn = QPushButton("Browse…")
        browse_btn.clicked.connect(self.browse)  # type: ignore
        load_btn = QPushButton("Load")
        load_btn.clicked.connect(self.load_path_from_edit)  # type: ignore
        path_row.addWidget(QLabel("Recent:"))
        path_row.addWidget(self.recent_combo)
        path_row.addWidget(self.path_edit, 1)
        path_row.addWidget(browse_btn)
        path_row.addWidget(load_btn)
        root_layout.addLayout(path_row)

        splitter = QSplitter()
        splitter.setOrientation(Qt.Orientation.Horizontal)

        # Left: scroll image
        scroll = QScrollArea()
        scroll.setWidgetResizable(False)
        scroll.setWidget(self.view)
        splitter.addWidget(scroll)

        # Right: side panel with output & preview
        side = QWidget()
        side_layout = QVBoxLayout(side)
        # Output
        self.output_edit = QLineEdit()
        self.output_edit.setReadOnly(True)
        copy_btn = QPushButton("Copy Output")
        copy_btn.clicked.connect(self.copy_manual)  # type: ignore
        out_row = QHBoxLayout()
        out_row.addWidget(QLabel("Selection:"))
        out_row.addWidget(self.output_edit, 1)
        out_row.addWidget(copy_btn)
        side_layout.addLayout(out_row)

        # Info label
        self.info_label = QLabel("Image: - x - | cols: - rows: -")
        font = self.info_label.font()
        font.setPointSizeF(font.pointSizeF() * 0.9)
        self.info_label.setFont(font)
        side_layout.addWidget(self.info_label)

        # Zoom slider
        zoom_row = QHBoxLayout()
        zoom_row.addWidget(QLabel("Zoom"))
        self.zoom_slider = QSlider(Qt.Orientation.Horizontal)
        self.zoom_slider.setRange(25, 800)  # percent
        self.zoom_slider.setValue(100)
        self.zoom_slider.valueChanged.connect(self._zoom_slider_changed)  # type: ignore
        zoom_row.addWidget(self.zoom_slider, 1)
        side_layout.addLayout(zoom_row)

        # Selected tile preview
        self.preview_label = QLabel("No selection")
        self.preview_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.preview_label.setStyleSheet(
            "border:1px solid #444; background:#222; min-height:150px;"
        )
        side_layout.addWidget(self.preview_label, 1)

        side_layout.addStretch(1)
        splitter.addWidget(side)
        splitter.setSizes([700, 250])  # type: ignore

        root_layout.addWidget(splitter, 1)

        self.status = QStatusBar()
        self.setStatusBar(self.status)

        self.setCentralWidget(central)
        self.resize(1100, 750)

        # Styling (dark theme)
        self._apply_styles()

        # Recent list load
        self._load_recent()

    # --- Styling ---
    def _apply_styles(self):
        self.setStyleSheet(
            """
            QMainWindow { background:#202225; }
            QLabel { color:#d0d0d0; }
            QLineEdit { background:#2b2f33; color:#e0e0e0; border:1px solid #3a3f44; padding:4px; }
            QLineEdit:read-only { background:#24282b; }
            QPushButton { background:#3a3f44; color:#e0e0e0; border:1px solid #4a5055; padding:4px 8px; }
            QPushButton:hover { background:#445059; }
            QPushButton:pressed { background:#2f363b; }
            QToolBar { background:#2a2d31; spacing:6px; border:0px; }
            QStatusBar { background:#2a2d31; color:#bbb; }
            QComboBox { background:#2b2f33; color:#dcdcdc; padding:2px 6px; }
            QScrollArea { background:#1e1f22; }
            QSlider::groove:horizontal { background:#2d3135; height:6px; border-radius:3px; }
            QSlider::handle:horizontal { background:#3daee9; width:14px; margin:-4px 0; border-radius:7px; }
            QSlider::sub-page:horizontal { background:#3daee9; }
            """
        )

    # --- Recent handling ---
    def _recent_store_path(self, path: str):
        if not path:
            return
        paths = self._recent_paths
        if path in paths:
            paths.remove(path)
        paths.insert(0, path)
        del paths[10:]
        self._recent_paths = paths
        self._refresh_recent_combo()
        # Persist to a small text file in tools
        try:
            rec_file = os.path.join(
                os.path.dirname(__file__), ".sprite_picker_recent.txt"
            )
            with open(rec_file, "w", encoding="utf-8") as f:
                for p in paths:
                    f.write(p + "\n")
        except Exception:
            pass

    def _load_recent(self):
        self._recent_paths: list[str] = []
        rec_file = os.path.join(os.path.dirname(__file__), ".sprite_picker_recent.txt")
        if os.path.isfile(rec_file):
            try:
                with open(rec_file, "r", encoding="utf-8") as f:
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
        for p in self._recent_paths:
            rel = (
                os.path.relpath(p, self.project_root).replace("\\", "/")
                if p.startswith(self.project_root)
                else p
            )
            self.recent_combo.addItem(rel, p)
        self.recent_combo.blockSignals(False)

    def _recent_chosen(self):
        idx = self.recent_combo.currentIndex()
        if idx >= 0:
            p = self.recent_combo.itemData(idx)
            if p:
                self.path_edit.setText(p)
                self.load_path_from_edit()

    # --- Interaction logic ---
    def _zoom_slider_changed(self, val: int):
        self.view.set_scale(val / 100.0)
        self.status.showMessage(f"Zoom: {val}%", 1000)

    # API called by SpriteSheetView (rectangle selection)
    def cell_selected(self, c1: int, r1: int, c2: int, r2: int):
        if not self.view.sheet:
            return
        rel_path = os.path.relpath(self.view.sheet.path, self.project_root).replace(
            "\\", "/"
        )
        text = f"{rel_path}, {c1}, {r1}, {c2}, {r2}"
        self.output_edit.setText(text)
        cb = QGuiApplication.clipboard()
        if cb:
            cb.setText(text)
        self.status.showMessage(f"Copied: {text}", 2500)
        self._update_preview(c1, r1, c2, r2)

    def _update_preview(
        self,
        c1: Optional[int] = None,
        r1: Optional[int] = None,
        c2: Optional[int] = None,
        r2: Optional[int] = None,
    ):
        sheet = self.view.sheet
        if not sheet:
            self.preview_label.setText("No selection")
            self.preview_label.setPixmap(QPixmap())
            return
        # If explicit rectangle not provided try from view
        if c1 is None or r1 is None or c2 is None or r2 is None:
            rect = self.view._normalize_rect()  # type: ignore
            if not rect:
                self.preview_label.setText("No selection")
                self.preview_label.setPixmap(QPixmap())
                return
            c1, r1, c2, r2 = rect
        # compute pixel region
        x = c1 * TILE_SIZE
        y = r1 * TILE_SIZE
        w = (c2 - c1 + 1) * TILE_SIZE
        h = (r2 - r1 + 1) * TILE_SIZE
        sub = sheet.pixmap.copy(x, y, w, h)
        # Use FastTransformation to preserve pixel-art crispness without triggering type check issues
        scaled = sub.scaled(
            160,
            160,
            Qt.AspectRatioMode.KeepAspectRatio,
            Qt.TransformationMode.FastTransformation,
        )
        self.preview_label.setPixmap(scaled)
        self.preview_label.setMinimumHeight(170)

    def browse(self):
        start_dir = (
            self.assets_root if os.path.isdir(self.assets_root) else self.project_root
        )
        path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Sprite Sheet",
            start_dir,
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
        assert sheet is not None
        self.info_label.setText(
            f"Image: {sheet.width} x {sheet.height} | cols: {sheet.cols} rows: {sheet.rows}"
        )
        rel_path = os.path.relpath(sheet.path, self.project_root).replace("\\", "/")
        self.status.showMessage(f"Loaded {rel_path}", 2500)
        self._recent_store_path(sheet.path)
        self._update_preview()

    def copy_manual(self):
        text = self.output_edit.text()
        if text:
            cb = QGuiApplication.clipboard()
            if cb:
                cb.setText(text)
            self.status.showMessage("Copied to clipboard", 1500)
            self._update_preview()

    # (Removed duplicate old cell_selected implementation for single cell)

    # (Removed legacy duplicate browse/load_path_from_edit/copy_manual definitions)


def main():
    # High DPI attributes not required/available in this PyQt6 build; rely on default behavior
    app = QApplication(sys.argv)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = find_project_root(script_dir)
    assets_root = os.path.join(project_root, "assets")
    win = MainWindow(project_root, assets_root)
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()

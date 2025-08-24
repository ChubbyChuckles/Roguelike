#!/usr/bin/env python3
"""
C Project Refactor Tool (PyQt6)

A GUI application for refactoring large C/C++ repositories by simulating a new
folder structure, previewing changes, and applying them safely with:
- File moves preserved via `git mv` (fallback to shutil.move)
- Automatic updates to local #include paths in .c/.cpp/.h/.hpp files
- CMakeLists.txt and *.cmake path updates in add_executable/add_library/target_sources
- Test sources updated similarly (includes and hardcoded string paths)
- Multiple CMakeLists.txt handled (recurses from project root)
- Options: dry-run, backup of modified text files, and logging to file
- Error handling: conflict checks, confirmation prompts, rollback on failure

Notes:
- Uses a virtual target tree to model the new structure (drag & drop inside the
  target tree or from source to target). The source tree mirrors the filesystem and
  is read-only.
- Include updating logic prefers quoted includes. Angle includes are only
  updated if the target can be resolved to a file within the project root. System
  headers are left intact.
- CMake update uses conservative regex replacements for moved file paths.
- Build verification can be optionally run after apply via a configurable command.

This script aims for robust defaults; for complex projects with custom build macros,
manual review is still recommended. The Preview step provides a detailed plan.

Author: GitHub Copilot
License: MIT
"""
from __future__ import annotations

import os
import re
import sys
import time
import shutil
import traceback
import subprocess
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Set, Tuple, cast

from PyQt6.QtCore import (
    QModelIndex,
    QMimeData,
    QObject,
    Qt,
    QUrl,
)
from PyQt6.QtGui import QStandardItem, QStandardItemModel
from PyQt6.QtWidgets import (
    QApplication,
    QCheckBox,
    QDialog,
    QDialogButtonBox,
    QFileIconProvider,
    QFileDialog,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QSplitter,
    QStatusBar,
    QTreeView,
    QVBoxLayout,
    QWidget,
)

# ------------------------- Configuration & Constants -------------------------

PROJECT_IGNORES = {
    ".git",
    "build",
    ".venv",
    "venv",
    "__pycache__",
    ".idea",
    ".vscode",
    "cmake-build-debug",
    ".githooks",
    ".github",
    "Testing",  # CMake test directory
}

C_SOURCE_EXTS = {".c", ".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh", ".hxx"}
CMAKE_FILE_NAMES = {"CMakeLists.txt"}
CMAKE_EXTS = {".cmake"}

DEFAULT_LOG_FILE = "refactor_tool.log"
DEFAULT_BACKUP_DIR = ".refactor_backups"

# Reusable OS icon provider for folder/file icons
ICON_PROVIDER = QFileIconProvider()

# ---------------------------- Utility / Core Types ---------------------------


def relpath(path: str, root: str) -> str:
    """Return forward-slash normalized path relative to root."""
    rp = os.path.relpath(path, root)
    return rp.replace("\\", "/")


def norm_join(*parts: str) -> str:
    """Join and normalize to forward slashes."""
    return os.path.normpath(os.path.join(*parts)).replace("\\", "/")


def ensure_dir(path: str) -> None:
    os.makedirs(path, exist_ok=True)


def is_text_file_ext(path: str) -> bool:
    _, ext = os.path.splitext(path)
    ext = ext.lower()
    return (
        ext in C_SOURCE_EXTS
        or ext in CMAKE_EXTS
        or os.path.basename(path) in CMAKE_FILE_NAMES
    )


def read_text(path: str) -> str:
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        return f.read()


def write_text(path: str, data: str) -> None:
    with open(path, "w", encoding="utf-8") as f:
        f.write(data)


@dataclass
class MovePlan:
    """Mapping of file moves from old -> new relative paths (project-root relative)."""

    moves: Dict[str, str] = field(default_factory=lambda: cast(Dict[str, str], {}))

    def add(self, old_rel: str, new_rel: str) -> None:
        if old_rel != new_rel:
            self.moves[old_rel] = new_rel

    def invert(self) -> Dict[str, str]:
        return {v: k for k, v in self.moves.items()}

    def conflicts(self) -> List[Tuple[str, str]]:
        rev: Dict[str, str] = {}
        dup: List[Tuple[str, str]] = []
        for old, new in self.moves.items():
            if new in rev:
                dup.append((rev[new], old))
            rev[new] = old
        return dup

    def to_list(self) -> List[Tuple[str, str]]:
        return sorted(self.moves.items())


@dataclass
class UpdatePlan:
    """Planned text edits for a file."""

    file_rel: str
    changes: List[Tuple[int, str, str]] = field(
        default_factory=lambda: cast(List[Tuple[int, str, str]], [])
    )
    # (line_number, old_line, new_line)

    def add_change(self, line_no: int, old: str, new: str) -> None:
        self.changes.append((line_no, old, new))

    def has_changes(self) -> bool:
        return len(self.changes) > 0


@dataclass
class RefactorPlan:
    move_plan: MovePlan
    text_updates: List[UpdatePlan] = field(
        default_factory=lambda: cast(List[UpdatePlan], [])
    )

    def is_empty(self) -> bool:
        return not self.move_plan.moves and not any(
            up.has_changes() for up in self.text_updates
        )


# ------------------------------- Logging Helper ------------------------------


class Logger:
    def __init__(self, log_path: str):
        self.log_path = log_path
        try:
            with open(self.log_path, "a", encoding="utf-8") as f:
                f.write(f"\n=== Refactor Tool Session {time.ctime()} ===\n")
        except Exception:
            pass

    def log(self, msg: str) -> None:
        print(msg)
        try:
            with open(self.log_path, "a", encoding="utf-8") as f:
                f.write(msg + "\n")
        except Exception:
            pass


# --------------------------- Virtual Target Tree Model -----------------------


class TargetTreeModel(QStandardItemModel):
    """A drag-and-drop tree model representing the desired post-refactor structure.

    Each item stores:
    - name (display)
    - data roles:
      * role 'is_dir': bool
      * role 'rel_path': current relative path from project root (computed on export)
    We store only names and hierarchy; final rel_path is computed when exporting.
    """

    ROLE_IS_DIR = Qt.ItemDataRole.UserRole + 1
    ROLE_SRC_ABS = Qt.ItemDataRole.UserRole + 2  # original absolute path (if from FS)

    def __init__(self, parent: Optional[QObject] = None):
        super().__init__(parent)

        self.setHorizontalHeaderLabels(["Target Structure"])  # type: ignore[arg-type]

    @staticmethod
    def make_item(
        name: str, is_dir: bool, src_abs: Optional[str] = None
    ) -> QStandardItem:
        item = QStandardItem(name)
        # Add a native icon for better visuals
        icon = (
            ICON_PROVIDER.icon(QFileIconProvider.IconType.Folder)
            if is_dir
            else ICON_PROVIDER.icon(QFileIconProvider.IconType.File)
        )
        item.setIcon(icon)
        flags = (
            Qt.ItemFlag.ItemIsSelectable
            | Qt.ItemFlag.ItemIsEnabled
            | Qt.ItemFlag.ItemIsDragEnabled
        )
        if is_dir:
            flags |= Qt.ItemFlag.ItemIsDropEnabled
        else:
            # Allow dropping files into files? No -> only dirs accept drops
            pass
        item.setFlags(flags)
        item.setData(is_dir, TargetTreeModel.ROLE_IS_DIR)
        if src_abs:
            item.setData(src_abs, TargetTreeModel.ROLE_SRC_ABS)
        return item

    def is_dir(self, index: QModelIndex) -> bool:
        if not index.isValid():
            return False
        return bool(index.data(TargetTreeModel.ROLE_IS_DIR))

    def mimeTypes(self) -> List[str]:
        # Accept filesystem URLs from left tree
        return ["text/uri-list", "application/x-qstandarditemmodeldatalist"]

    def supportedDropActions(self) -> Qt.DropAction:
        return Qt.DropAction.MoveAction | Qt.DropAction.CopyAction

    def canDropMimeData(
        self,
        data: Optional[QMimeData],
        action: Qt.DropAction,
        row: int,
        column: int,
        parent: QModelIndex,
    ) -> bool:
        if not parent.isValid():
            return False
        return self.is_dir(parent)

    def dropMimeData(
        self,
        data: Optional[QMimeData],
        action: Qt.DropAction,
        row: int,
        column: int,
        parent: QModelIndex,
    ) -> bool:
        if not self.canDropMimeData(data, action, row, column, parent):
            return False
        parent_item = self.itemFromIndex(parent)
        if parent_item is None:
            return False
        if data is not None and data.hasUrls():
            # Drop from source FS tree: add items recursively
            for url in data.urls():
                path = url.toLocalFile()
                if not path:
                    continue
                # Prefer moving existing mirrored item (by original absolute path)
                if not self._move_or_add_path(parent_item, path):
                    # Fallback: add recursively
                    self._add_path_recursive(parent_item, path)
            # Emit dataChanged to notify views that the model structure changed
            self.layoutChanged.emit()  # type: ignore[attr-defined]
            return True
        # Internal move is handled by default implementation; let base class handle
        result = super().dropMimeData(data, action, row, column, parent)
        if result:
            # Emit signal to notify that the model changed
            self.layoutChanged.emit()  # type: ignore[attr-defined]
        return result

    def _add_path_recursive(self, parent_item: QStandardItem, abs_path: str) -> None:
        name = os.path.basename(abs_path)
        is_directory = os.path.isdir(abs_path)
        new_item = TargetTreeModel.make_item(name, is_directory, src_abs=abs_path)
        parent_item.appendRow(new_item)  # type: ignore[arg-type]
        if is_directory:
            try:
                for entry in sorted(os.listdir(abs_path)):
                    if entry in PROJECT_IGNORES:
                        continue
                    sub = os.path.join(abs_path, entry)
                    self._add_path_recursive(new_item, sub)
            except Exception:
                pass

    def _find_item_by_src_abs(
        self, start: QStandardItem, src_abs: str
    ) -> Optional[QStandardItem]:
        # DFS search for an item with matching ROLE_SRC_ABS
        for i in range(start.rowCount()):
            child = start.child(i)
            if child is None:
                continue
            val = child.data(TargetTreeModel.ROLE_SRC_ABS)
            if isinstance(val, str) and os.path.normpath(val) == os.path.normpath(
                src_abs
            ):
                return child
            if bool(child.data(TargetTreeModel.ROLE_IS_DIR)):
                found = self._find_item_by_src_abs(child, src_abs)
                if found is not None:
                    return found
        return None

    def _move_or_add_path(self, new_parent: QStandardItem, abs_path: str) -> bool:
        # Try to find an existing item mirroring this path and move it under new_parent
        root = self.invisibleRootItem()
        if root is None:
            return False
        existing = self._find_item_by_src_abs(root, abs_path)
        if existing is None:
            return False
        # Detach from old parent
        old_parent = existing.parent() or root
        # Find row of existing in old_parent
        row = None
        for i in range(old_parent.rowCount()):
            if old_parent.child(i) is existing:
                row = i
                break
        if row is None:
            return False
        taken = old_parent.takeRow(row)
        # Reinsert under new parent with same item(s)
        new_parent.appendRow(taken)  # type: ignore[arg-type]
        return True

    # Export current hierarchy to map of desired rel paths for files
    def export_paths(self, project_root: str) -> Dict[str, str]:
        """Return mapping from current displayed tree (virtual) of files to rel paths under root.
        The keys are the virtual tree file names, but to correlate with real files, the app will
        compute mapping by matching names from original locations; we instead compute only the
        final desired relative paths for items present in the model.
        """
        mapping: Dict[QStandardItem, str] = {}

        def dfs(item: QStandardItem, cur_path: str) -> None:
            count = item.rowCount()
            for i in range(count):
                child = item.child(i)
                if child is None:
                    continue
                name = child.text()
                is_dir = bool(child.data(TargetTreeModel.ROLE_IS_DIR))
                next_path = norm_join(cur_path, name)
                if is_dir:
                    dfs(child, next_path)
                else:
                    mapping[child] = next_path

        root = self.invisibleRootItem()
        if root is not None:
            for i in range(root.rowCount()):
                child = root.child(i)
                if child is None:
                    continue
                base = child.text()
                cur = base
                if bool(child.data(TargetTreeModel.ROLE_IS_DIR)):
                    dfs(child, cur)
                else:
                    mapping[child] = cur

        # Convert to plain dict of virtual file names -> new rel path
        out: Dict[str, str] = {}
        for _, rel in mapping.items():
            out[rel] = rel  # value is rel path in target tree; key used later
        return out


# --------------------------- Source (FS) Tree Model --------------------------


class SourceTreeModel(QStandardItemModel):
    """Read-only view of the filesystem for dragging into the target tree.

    Emits text/uri-list mime data so the target tree can ingest absolute paths.
    """

    ROLE_ABS_PATH = Qt.ItemDataRole.UserRole + 101

    def __init__(self, root_path: str, parent: Optional[QObject] = None):
        super().__init__(parent)
        self.root_path = os.path.abspath(root_path)
        self.setHorizontalHeaderLabels(["Project Files"])  # type: ignore[arg-type]
        self.refresh()

    def set_root_path(self, root_path: str) -> None:
        self.root_path = os.path.abspath(root_path)
        self.refresh()

    def refresh(self) -> None:
        self.clear()
        self.setHorizontalHeaderLabels(["Project Files"])  # type: ignore[arg-type]
        root_item = self.invisibleRootItem()
        if root_item is None:
            return
        self._populate_dir(root_item, self.root_path)

    def _populate_dir(self, parent_item: Optional[QStandardItem], abs_dir: str) -> None:
        if parent_item is None:
            return
        try:
            entries = sorted(os.listdir(abs_dir))
        except Exception:
            return
        for name in entries:
            if name in PROJECT_IGNORES:
                continue
            p = os.path.join(abs_dir, name)
            if os.path.isdir(p):
                item = QStandardItem(name)
                item.setIcon(ICON_PROVIDER.icon(QFileIconProvider.IconType.Folder))
                item.setData(p, self.ROLE_ABS_PATH)
                item.setFlags(
                    Qt.ItemFlag.ItemIsEnabled
                    | Qt.ItemFlag.ItemIsSelectable
                    | Qt.ItemFlag.ItemIsDragEnabled
                )
                parent_item.appendRow(item)  # type: ignore[arg-type]
                self._populate_dir(item, p)
            else:
                item = QStandardItem(name)
                item.setIcon(ICON_PROVIDER.icon(QFileIconProvider.IconType.File))
                item.setData(p, self.ROLE_ABS_PATH)
                item.setFlags(
                    Qt.ItemFlag.ItemIsEnabled
                    | Qt.ItemFlag.ItemIsSelectable
                    | Qt.ItemFlag.ItemIsDragEnabled
                )
                parent_item.appendRow(item)  # type: ignore[arg-type]

    def mimeTypes(self) -> List[str]:
        return ["text/uri-list"]

    def mimeData(self, indexes: List[QModelIndex]) -> QMimeData:
        mime = QMimeData()
        # Only consider unique file entries from column 0
        paths: Set[str] = set()
        for idx in indexes:
            if not idx.isValid() or idx.column() != 0:
                continue
            p = idx.data(SourceTreeModel.ROLE_ABS_PATH)
            if p:
                paths.add(p)
        urls = [QUrl.fromLocalFile(p) for p in sorted(paths)]
        mime.setUrls(urls)  # type: ignore[arg-type]
        return mime


# ------------------------------ Preview Dialog -------------------------------


class PreviewDialog(QDialog):
    def __init__(self, parent: QWidget, plan: RefactorPlan):
        super().__init__(parent)
        self.setWindowTitle("Preview Changes")
        self.resize(900, 600)

        layout = QVBoxLayout(self)

        # Summary with more detail
        move_count = len(plan.move_plan.moves)
        text_count = sum(1 for up in plan.text_updates if up.has_changes())
        total_changes = sum(
            len(up.changes) for up in plan.text_updates if up.has_changes()
        )

        layout.addWidget(QLabel(f"Planned file moves: {move_count}"))
        layout.addWidget(QLabel(f"Files with text updates: {text_count}"))
        if total_changes > 0:
            layout.addWidget(QLabel(f"Total line changes: {total_changes}"))

        # Details area
        from PyQt6.QtWidgets import QTextEdit

        details = QTextEdit(self)
        details.setReadOnly(True)
        details.setLineWrapMode(QTextEdit.LineWrapMode.NoWrap)
        details.setFontFamily("Consolas")
        details.setFontPointSize(10)
        buf: List[str] = []
        buf.append("# Moves (old -> new):")
        for old, new in plan.move_plan.to_list():
            buf.append(f"- {old} -> {new}")
        buf.append("")
        buf.append("# Text Updates:")
        for up in plan.text_updates:
            if not up.has_changes():
                continue
            buf.append(f"\n[{up.file_rel}]")
            for ln, old, new in up.changes[:200]:  # cap
                old_s = old.rstrip("\n")
                new_s = new.rstrip("\n")
                buf.append(f"  L{ln}: {old_s}")
                buf.append(f"      => {new_s}")
            if len(up.changes) > 200:
                buf.append(f"  ... ({len(up.changes) - 200} more changes)")
        details.setPlainText("\n".join(buf))
        layout.addWidget(details)

        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Close,
            parent=self,
        )
        buttons.rejected.connect(self.reject)  # type: ignore[arg-type]
        buttons.accepted.connect(self.accept)  # type: ignore[arg-type]
        layout.addWidget(buttons)


# ------------------------------ Core Refactorer ------------------------------


class RefactorEngine:
    """Core logic to compute/refactor moves and update references."""

    def __init__(self, project_root: str, logger: Logger):
        self.project_root = os.path.abspath(project_root)
        self.logger = logger
        self.moved_map: Dict[str, str] = {}
        self.rev_map: Dict[str, str] = {}

    # --------- Planning ---------

    def plan_from_models(self, target_model: TargetTreeModel) -> MovePlan:
        """Generate a MovePlan by comparing the virtual target structure to the current FS.

        Strategy: Prefer exact mapping using the original absolute path stored on
        target items (ROLE_SRC_ABS). For each leaf in the target tree, compute its
        desired relative path and pair with the original file's relative path. If
        ROLE_SRC_ABS is missing (manually created node), fall back to a filename
        heuristic across the repository.
        """
        plan = MovePlan()

        # Index all real files under root for fallback mapping
        real_files: Dict[str, str] = {}
        for abs_path in self._walk_files():
            rp = relpath(abs_path, self.project_root)
            real_files[rp] = abs_path
        filename_index: Dict[str, List[str]] = {}
        for rp in real_files.keys():
            filename_index.setdefault(os.path.basename(rp), []).append(rp)

        # Traverse target tree and build mapping
        root_item = target_model.invisibleRootItem()

        def traverse(item: QStandardItem, prefix: str) -> None:
            for i in range(item.rowCount()):
                child = item.child(i)
                if child is None:
                    continue
                name = child.text()
                is_dir = bool(child.data(TargetTreeModel.ROLE_IS_DIR))
                new_rel = name if not prefix else f"{prefix}/{name}"
                if is_dir:
                    traverse(child, new_rel)
                else:
                    src_abs = child.data(TargetTreeModel.ROLE_SRC_ABS)
                    if src_abs:
                        old_rel = relpath(str(src_abs), self.project_root)
                        # Normalize both paths for comparison
                        old_rel_norm = old_rel.replace("\\", "/")
                        new_rel_norm = new_rel.replace("\\", "/")
                        if old_rel_norm != new_rel_norm:
                            plan.add(old_rel_norm, new_rel_norm)
                    else:
                        # Fallback by filename only
                        fname = os.path.basename(new_rel)
                        cand = filename_index.get(fname, [])
                        if cand:
                            # choose closest depth match
                            best = min(
                                cand,
                                key=lambda p: abs(p.count("/") - new_rel.count("/")),
                            )
                            if best != new_rel:
                                plan.add(best, new_rel)

        if root_item is not None:
            traverse(root_item, "")

        # Record maps for later updates
        self.moved_map = dict(plan.moves)
        self.rev_map = plan.invert()
        return plan

    # --------- Update discovery ---------

    def build_update_plan(self, move_plan: MovePlan) -> List[UpdatePlan]:
        updates: List[UpdatePlan] = []

        # Skip text updates if no moves are planned
        if not move_plan.moves:
            return updates

        # Build a resolver for includes - scan all C/C++ and CMake files
        index_all_files: Dict[str, str] = {}
        for abs_path in self._walk_files():
            rp = relpath(abs_path, self.project_root)
            index_all_files[rp] = abs_path

        # For each source-like text file, check if it needs updates
        for file_rel, abs_path in sorted(index_all_files.items()):
            if not is_text_file_ext(abs_path):
                continue

            old_abs = abs_path
            new_rel = move_plan.moves.get(
                file_rel, file_rel
            )  # Where this file will be after move

            text = read_text(old_abs)
            lines = text.splitlines(keepends=True)
            file_updates = UpdatePlan(file_rel=file_rel)

            # 1) Update includes in this file
            include_re = re.compile(r"^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]")
            for i, line in enumerate(lines, start=1):
                m = include_re.match(line)
                if not m:
                    continue
                inc_path = m.group(1)

                # Try to resolve include to a project file
                resolved = self._resolve_include(file_rel, inc_path)
                if not resolved:
                    # likely system or external include
                    continue

                # Check if the included file is being moved
                included_rel = relpath(resolved, self.project_root)
                included_new_rel = move_plan.moves.get(included_rel, included_rel)

                # Calculate what the include path should be from this file's new location
                new_include = self._relative_include_path(new_rel, included_new_rel)

                # Update if the include path needs to change
                if new_include and new_include != inc_path:
                    new_line = line.replace(inc_path, new_include)
                    file_updates.add_change(i, line, new_line)
                    lines[i - 1] = new_line

            # 2) Update CMake paths: replace moved file rel paths if they appear
            if (
                os.path.basename(file_rel) in CMAKE_FILE_NAMES
                or os.path.splitext(file_rel)[1] in CMAKE_EXTS
            ):
                # Do simple replacement: quoted or token
                cmake_text = "".join(lines)
                changed = False
                for old_rel, new_rel_path in move_plan.moves.items():
                    # Replace within quotes
                    q_pat = re.compile(rf"([\"']){re.escape(old_rel)}([\"'])")
                    cmake_text2 = q_pat.sub(rf"\\1{new_rel_path}\\2", cmake_text)
                    # Replace bare tokens (word boundary-ish). Keep conservative: spaces/newlines
                    t_pat = re.compile(
                        rf"(?<![\w\-/\.]){re.escape(old_rel)}(?![\w\-/\.])"
                    )
                    cmake_text3 = t_pat.sub(new_rel_path, cmake_text2)
                    if cmake_text3 != cmake_text:
                        cmake_text = cmake_text3
                        changed = True

                # For tests/CMakeLists.txt specifically, wrap bare add_executable calls with EXISTS checks
                if file_rel == "tests/CMakeLists.txt":
                    cmake_lines = cmake_text.splitlines()
                    new_cmake_lines: list[str] = []
                    i = 0
                    while i < len(cmake_lines):
                        line = cmake_lines[i]
                        # Look for bare add_executable calls (not already in if blocks)
                        if line.strip().startswith(
                            "add_executable("
                        ) and not line.strip().startswith("if("):
                            # Check if there's an if(EXISTS ...) check in the previous few lines
                            has_exists_check = False
                            for j in range(max(0, i - 5), i):
                                if (
                                    "EXISTS" in cmake_lines[j]
                                    and "CMAKE_CURRENT_SOURCE_DIR" in cmake_lines[j]
                                ):
                                    has_exists_check = True
                                    break

                            if not has_exists_check:
                                # Parse the add_executable line
                                match = re.match(
                                    r"^(\s*)(add_executable\s*\(\s*(\w+)\s+(.+?)\s*\))",
                                    line,
                                )
                                if match:
                                    indent, full_call, target_name, file_args = (
                                        match.groups()
                                    )
                                    # Find the source file
                                    source_file = None
                                    for arg in file_args.split():
                                        if arg.endswith(".c") or arg.endswith(".cpp"):
                                            source_file = arg
                                            break

                                    if source_file:
                                        # Wrap with EXISTS check
                                        new_cmake_lines.append(
                                            f"{indent}if(EXISTS ${{CMAKE_CURRENT_SOURCE_DIR}}/{source_file} AND NOT TARGET {target_name})"
                                        )
                                        new_cmake_lines.append(
                                            f"{indent}    {full_call}"
                                        )

                                        # Look ahead for target_link_libraries and other related lines
                                        j = i + 1
                                        while j < len(cmake_lines) and j < i + 10:
                                            next_line = cmake_lines[j]
                                            if target_name in next_line and (
                                                "target_link_libraries" in next_line
                                                or "target_compile_definitions"
                                                in next_line
                                                or "add_test" in next_line
                                                or "set_tests_properties" in next_line
                                            ):
                                                new_cmake_lines.append(
                                                    f"{indent}    {next_line.strip()}"
                                                )
                                                j += 1
                                            else:
                                                break

                                        new_cmake_lines.append(f"{indent}endif()")
                                        i = j  # Skip the lines we already processed
                                        changed = True
                                        continue

                        new_cmake_lines.append(line)
                        i += 1

                    if changed:
                        cmake_text = "\n".join(new_cmake_lines) + "\n"

                if changed:
                    # diff lines to record changes
                    new_lines = cmake_text.splitlines(keepends=True)
                    for i, (old_l, new_l) in enumerate(zip(lines, new_lines), start=1):
                        if old_l != new_l:
                            file_updates.add_change(i, old_l, new_l)
                    lines = new_lines

            # 3) Update hardcoded string paths for moved files (in C/C++ tests)
            hard_text = "".join(lines)
            changed_hard = False
            for old_rel, new_rel_path in move_plan.moves.items():
                pat = re.compile(rf"([\"']){re.escape(old_rel)}([\"'])")
                hard_text2 = pat.sub(rf"\\1{new_rel_path}\\2", hard_text)
                if hard_text2 != hard_text:
                    hard_text = hard_text2
                    changed_hard = True
            if changed_hard and not (
                os.path.basename(file_rel) in CMAKE_FILE_NAMES
                or os.path.splitext(file_rel)[1] in CMAKE_EXTS
            ):
                # Compute diffs
                new_lines = hard_text.splitlines(keepends=True)
                for i, (old_l, new_l) in enumerate(zip(lines, new_lines), start=1):
                    if old_l != new_l:
                        file_updates.add_change(i, old_l, new_l)
                lines = new_lines

            if file_updates.has_changes():
                updates.append(file_updates)

        return updates

    # --------- Apply ---------

    def apply(
        self,
        plan: RefactorPlan,
        dry_run: bool,
        backup: bool,
        build_after: bool,
        build_cmd: str,
        ask_overwrite: bool = True,
    ) -> Tuple[bool, str]:
        """Apply the move and text update plan. Returns (success, message)."""
        # Validate conflicts
        conflicts = plan.move_plan.conflicts()
        if conflicts:
            return False, f"Conflicting target paths for moves: {conflicts}"

        # Validate destination existence
        for old_rel, new_rel in plan.move_plan.moves.items():
            dest_abs = os.path.join(self.project_root, new_rel)
            if os.path.exists(dest_abs):
                if ask_overwrite:
                    return False, f"Destination already exists: {new_rel}"

        # Perform moves (git mv preferred)
        moved: List[Tuple[str, str]] = []
        try:
            for old_rel, new_rel in plan.move_plan.to_list():
                src_abs = os.path.join(self.project_root, old_rel)
                dst_abs = os.path.join(self.project_root, new_rel)
                self.logger.log(f"Move: {old_rel} -> {new_rel}")
                if dry_run:
                    continue
                ensure_dir(os.path.dirname(dst_abs))
                if self._git_available():
                    rc = subprocess.call(
                        ["git", "mv", "-f", src_abs, dst_abs], cwd=self.project_root
                    )
                    if rc != 0:
                        raise RuntimeError(f"git mv failed for {old_rel} -> {new_rel}")
                else:
                    shutil.move(src_abs, dst_abs)
                moved.append((old_rel, new_rel))
        except Exception as e:
            self.logger.log(f"ERROR during move: {e}\n{traceback.format_exc()}")
            # rollback moves
            if moved and not dry_run:
                for old_rel, new_rel in reversed(moved):
                    try:
                        src_abs = os.path.join(self.project_root, new_rel)
                        dst_abs = os.path.join(self.project_root, old_rel)
                        if self._git_available():
                            subprocess.call(
                                ["git", "mv", "-f", src_abs, dst_abs],
                                cwd=self.project_root,
                            )
                        else:
                            shutil.move(src_abs, dst_abs)
                    except Exception:
                        pass
            return False, f"Move failed and rolled back: {e}"

        # Apply text edits
        backups: List[str] = []
        try:
            for up in plan.text_updates:
                if not up.has_changes():
                    continue
                abs_old_rel = os.path.join(self.project_root, up.file_rel)
                # After moves, file may be at new path
                new_rel = plan.move_plan.moves.get(up.file_rel, up.file_rel)
                abs_path = os.path.join(self.project_root, new_rel)
                if not os.path.exists(abs_path):
                    # if not found at new, try old
                    abs_path = abs_old_rel
                try:
                    text = read_text(abs_path)
                except Exception:
                    # skip binary or unreadable
                    continue
                lines = text.splitlines(keepends=True)
                for ln, _old, new_line in up.changes:
                    idx = ln - 1
                    if 0 <= idx < len(lines):
                        lines[idx] = new_line
                new_text = "".join(lines)
                if new_text != text:
                    if backup and not dry_run:
                        backup_path = self._backup_path(new_rel)
                        ensure_dir(os.path.dirname(backup_path))
                        write_text(backup_path, text)
                        backups.append(backup_path)
                    self.logger.log(f"Edit: {new_rel} ({len(up.changes)} lines)")
                    if not dry_run:
                        write_text(abs_path, new_text)
        except Exception as e:
            self.logger.log(f"ERROR during text edits: {e}\n{traceback.format_exc()}")
            # Best-effort rollback edits
            if backup and not dry_run:
                for b in backups:
                    try:
                        rel = os.path.relpath(
                            b, os.path.join(self.project_root, DEFAULT_BACKUP_DIR)
                        )
                        # Original target file after apply
                        target_rel = rel.split("__backup__", 1)[-1]
                        target_abs = os.path.join(self.project_root, target_rel)
                        orig = read_text(b)
                        write_text(target_abs, orig)
                    except Exception:
                        pass
            return False, "Text edits failed; attempted rollback from backups"

        if build_after and not dry_run and build_cmd.strip():
            try:
                self.logger.log(f"Running build: {build_cmd}")
                rc = subprocess.call(build_cmd, shell=True, cwd=self.project_root)
                if rc != 0:
                    return False, f"Build command failed with exit code {rc}"
            except Exception as e:
                return False, f"Build command failed: {e}"

        return True, "Refactor applied successfully"

    # --------- Helpers ---------

    def _walk_files(self) -> List[str]:
        files: List[str] = []
        for root, dirs, filenames in os.walk(self.project_root):
            # filter dirs
            dirs[:] = [d for d in dirs if d not in PROJECT_IGNORES]
            for fn in filenames:
                p = os.path.join(root, fn)
                files.append(p)
        return files

    def _resolve_include(self, including_rel: str, include_str: str) -> Optional[str]:
        """Try to resolve an include path to an absolute file within the project.

        Tries:
        - Relative to the including file directory (old location)
        - Relative to project root
        - Common src/ patterns
        - Handle "core/" prefix patterns
        Returns absolute path if exists else None.
        """
        inc_norm = include_str.replace("\\", "/")

        # Skip obvious system headers (angle brackets usually indicate system)
        # but still process quoted includes that might be system-looking

        # Try relative to including file's directory first
        inc_abs = os.path.normpath(
            os.path.join(self.project_root, os.path.dirname(including_rel), inc_norm)
        )
        if os.path.exists(inc_abs):
            return inc_abs

        # Try relative to project root
        root_abs = os.path.normpath(os.path.join(self.project_root, inc_norm))
        if os.path.exists(root_abs):
            return root_abs

        # Try common patterns like src/ relative
        if not inc_norm.startswith("src/"):
            src_abs = os.path.normpath(os.path.join(self.project_root, "src", inc_norm))
            if os.path.exists(src_abs):
                return src_abs

        # Handle "core/" prefix by trying "src/core/"
        if inc_norm.startswith("core/") and not os.path.exists(
            os.path.join(self.project_root, inc_norm)
        ):
            src_core_abs = os.path.normpath(
                os.path.join(self.project_root, "src", inc_norm)
            )
            if os.path.exists(src_core_abs):
                return src_core_abs

        return None

    def _relative_include_path(
        self, including_new_rel: str, included_new_rel: str
    ) -> Optional[str]:
        inc_dir = os.path.dirname(including_new_rel)
        if not inc_dir:
            inc_dir = "."
        try:
            rp = os.path.relpath(included_new_rel, inc_dir)
            return rp.replace("\\", "/")
        except Exception:
            return None

    def _git_available(self) -> bool:
        try:
            rc = subprocess.call(
                ["git", "--version"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            return rc == 0
        except Exception:
            return False

    def _backup_path(self, rel_path: str) -> str:
        ts = time.strftime("%Y%m%d_%H%M%S")
        return os.path.join(
            self.project_root,
            DEFAULT_BACKUP_DIR,
            ts + "__backup__" + rel_path.replace("\\", "/"),
        )


# ---------------------------------- Main UI ----------------------------------


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("C Project Refactor Tool")
        self.resize(1200, 800)

        # State
        self.project_root = os.getcwd()
        self.logger = Logger(os.path.join(self.project_root, DEFAULT_LOG_FILE))
        self.engine = RefactorEngine(self.project_root, self.logger)

        # Central widget and main layout
        central = QWidget(self)
        self.setCentralWidget(central)
        vbox = QVBoxLayout(central)

        # Top controls: path, browse, options
        controls = QHBoxLayout()
        vbox.addLayout(controls)
        controls.addWidget(QLabel("Project Root:"))
        self.root_edit = QLineEdit(self.project_root)
        self.root_edit.setMinimumWidth(500)
        controls.addWidget(self.root_edit)
        browse_btn = QPushButton("Browse…")
        browse_btn.clicked.connect(self._browse_root)  # type: ignore[arg-type]
        controls.addWidget(browse_btn)

        self.dry_run_chk = QCheckBox("Dry-run")
        self.dry_run_chk.setChecked(True)
        controls.addWidget(self.dry_run_chk)

        self.backup_chk = QCheckBox("Backup text files")
        self.backup_chk.setChecked(True)
        controls.addWidget(self.backup_chk)

        self.build_chk = QCheckBox("Run build after apply")
        self.build_chk.setChecked(False)
        controls.addWidget(self.build_chk)
        controls.addStretch(1)
        # Right-side quick actions
        self.refresh_btn = QPushButton("Refresh Trees")
        self.refresh_btn.setToolTip(
            "Re-scan filesystem tree and rebuild target from disk"
        )
        self.refresh_btn.clicked.connect(self._refresh_trees)  # type: ignore[arg-type]
        controls.addWidget(self.refresh_btn)
        self.reset_target_btn = QPushButton("Reset Target")
        self.reset_target_btn.setToolTip(
            "Reset the target tree to mirror the current filesystem"
        )
        self.reset_target_btn.clicked.connect(self._rebuild_target_tree)  # type: ignore[arg-type]
        controls.addWidget(self.reset_target_btn)

        # Build command line
        build_box = QHBoxLayout()
        vbox.addLayout(build_box)
        build_box.addWidget(QLabel("Build Command:"))
        self.build_cmd_edit = QLineEdit(
            "cmake --build build --config Debug --parallel 8"
        )
        build_box.addWidget(self.build_cmd_edit)

        # Splitter with source and target trees
        self.splitter = QSplitter(Qt.Orientation.Horizontal, self)
        vbox.addWidget(self.splitter, 1)

        # Left: source FS tree (read-only)
        self.src_model = SourceTreeModel(self.project_root, self)
        self.src_view = QTreeView(self)
        self.src_view.setModel(self.src_model)
        self.src_view.setSelectionMode(QTreeView.SelectionMode.ExtendedSelection)
        self.src_view.setDragEnabled(True)
        self.src_view.setAcceptDrops(False)
        self.src_view.setDragDropMode(QTreeView.DragDropMode.DragOnly)
        self.src_view.setHeaderHidden(False)
        self.src_view.setColumnWidth(0, 350)
        self.src_view.setAlternatingRowColors(True)
        self.src_view.setStyleSheet(
            "QTreeView { font-family: Consolas; font-size: 10pt; }"
        )
        self.src_view.setToolTip("Drag files/folders into the Target to plan moves")

        # Right: target virtual tree (draggable + droppable)
        self.tgt_model = TargetTreeModel(self)
        self.tgt_view = QTreeView(self)
        self.tgt_view.setModel(self.tgt_model)
        self.tgt_view.setSelectionMode(QTreeView.SelectionMode.ExtendedSelection)
        self.tgt_view.setAcceptDrops(True)
        # Allow both internal reordering and external drops from source tree
        self.tgt_view.setDragDropMode(QTreeView.DragDropMode.DragDrop)
        self.tgt_view.setDragEnabled(True)
        self.tgt_view.setDefaultDropAction(Qt.DropAction.MoveAction)
        self.tgt_view.setHeaderHidden(False)
        self.tgt_view.setColumnWidth(0, 350)
        self.tgt_view.setAlternatingRowColors(True)
        self.tgt_view.setStyleSheet(
            "QTreeView { font-family: Consolas; font-size: 10pt; }"
        )
        self.tgt_view.setToolTip(
            "Arrange the desired structure. Drops store original paths for accurate mapping."
        )

        self.splitter.addWidget(self.src_view)
        self.splitter.addWidget(self.tgt_view)
        self.splitter.setSizes([650, 650])  # type: ignore[arg-type]

        # Bottom buttons
        btns = QHBoxLayout()
        vbox.addLayout(btns)
        self.preview_btn = QPushButton("Preview Changes")
        self.preview_btn.setToolTip(
            "Compute planned git mv operations and text updates"
        )
        self.preview_btn.clicked.connect(self.on_preview)  # type: ignore[arg-type]
        btns.addWidget(self.preview_btn)
        self.apply_btn = QPushButton("Apply Changes")
        self.apply_btn.setToolTip(
            "Execute moves and update text files; rollback on error"
        )
        self.apply_btn.clicked.connect(self.on_apply)  # type: ignore[arg-type]
        btns.addWidget(self.apply_btn)
        btns.addStretch(1)

        # Status bar
        self._status = QStatusBar(self)
        self.setStatusBar(self._status)

        # Initialize target tree from filesystem
        self._rebuild_target_tree()

    # -------------- UI helpers --------------

    def _browse_root(self) -> None:
        d = QFileDialog.getExistingDirectory(
            self, "Select Project Root", self.project_root
        )
        if d:
            self.project_root = d
            self.root_edit.setText(self.project_root)
            self.logger = Logger(os.path.join(self.project_root, DEFAULT_LOG_FILE))
            self.engine = RefactorEngine(self.project_root, self.logger)
            # Refresh source tree model
            self.src_model.set_root_path(self.project_root)
            self._rebuild_target_tree()

    def _rebuild_target_tree(self) -> None:
        self.tgt_model.clear()
        self.tgt_model.setHorizontalHeaderLabels(["Target Structure"])  # type: ignore[arg-type]
        root_item = self.tgt_model.invisibleRootItem()
        if root_item is not None:
            # Mirror filesystem into target, preserving original absolute paths
            self._populate_from_fs(root_item, self.project_root)
        self._status.showMessage("Target structure initialized from filesystem")

    def _refresh_trees(self) -> None:
        # Refresh source tree and rebuild target mirror of disk
        self.src_model.set_root_path(self.project_root)
        self._rebuild_target_tree()

    def _populate_from_fs(
        self, parent_item: Optional[QStandardItem], abs_dir: str
    ) -> None:
        if parent_item is None:
            return
        try:
            entries = sorted(os.listdir(abs_dir))
        except Exception:
            return
        for name in entries:
            if name in PROJECT_IGNORES:
                continue
            abs_path = os.path.join(abs_dir, name)
            if os.path.isdir(abs_path):
                # Folder: also store src_abs to allow drag-moving whole directories accurately
                item = TargetTreeModel.make_item(name, True, src_abs=abs_path)
                parent_item.appendRow(item)  # type: ignore[arg-type]
                self._populate_from_fs(item, abs_path)
            else:
                # File: create item with original absolute path for exact mapping
                item = TargetTreeModel.make_item(name, False, src_abs=abs_path)
                parent_item.appendRow(item)  # type: ignore[arg-type]

    # -------------- Plan & Apply --------------

    def _compute_plan(self) -> RefactorPlan:
        # Ensure engine has up-to-date root
        self.engine = RefactorEngine(self.project_root, self.logger)

        # Build moves by comparing virtual target to actual FS
        move_plan = self.engine.plan_from_models(self.tgt_model)

        # Compute text updates for includes/cmake
        updates = self.engine.build_update_plan(move_plan)

        return RefactorPlan(move_plan=move_plan, text_updates=updates)

    def on_preview(self) -> None:
        plan = self._compute_plan()
        # Debug: Show some info in status bar
        move_count = len(plan.move_plan.moves)
        text_count = sum(1 for up in plan.text_updates if up.has_changes())
        total_changes = sum(
            len(up.changes) for up in plan.text_updates if up.has_changes()
        )

        if move_count > 0:
            self._status.showMessage(
                f"Found {move_count} moves, {text_count} files with {total_changes} total text changes"
            )
        else:
            self._status.showMessage("No moves detected - target matches filesystem")
        dlg = PreviewDialog(self, plan)
        dlg.exec()

    def on_apply(self) -> None:
        plan = self._compute_plan()
        if plan.is_empty():
            QMessageBox.information(self, "Apply Changes", "No changes to apply.")
            return
        # Confirm
        move_count = len(plan.move_plan.moves)
        text_count = sum(1 for up in plan.text_updates if up.has_changes())
        yn = QMessageBox.question(
            self,
            "Confirm Apply",
            f"Apply {move_count} moves and update {text_count} files?",
        )
        if yn != QMessageBox.StandardButton.Yes:
            return

        dry_run = self.dry_run_chk.isChecked()
        backup = self.backup_chk.isChecked()
        build_after = self.build_chk.isChecked()
        build_cmd = self.build_cmd_edit.text().strip()

        ok, msg = self.engine.apply(
            plan=plan,
            dry_run=dry_run,
            backup=backup,
            build_after=build_after,
            build_cmd=build_cmd,
        )
        if ok:
            QMessageBox.information(self, "Refactor", msg)
            # Refresh trees
            self.src_model.set_root_path(self.project_root)
            self._rebuild_target_tree()
        else:
            QMessageBox.critical(self, "Refactor Failed", msg)


# ---------------------------------- Entrypoint --------------------------------


def main() -> int:
    app = QApplication(sys.argv)
    win = MainWindow()
    win.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())

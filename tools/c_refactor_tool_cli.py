#!/usr/bin/env python3
"""
CLI version of C Refactor Tool with enhanced CMakeLists.txt handling

This tool can:
1. Detect git staged file moves
2. Choose to either revert moves OR update references in source files
3. Specifically handle CMakeLists.txt path updates for subdirectory moves
4. Handle complex EXISTS checks and conditional CMake logic

Usage:
  python c_refactor_tool_cli.py --help
  python c_refactor_tool_cli.py --update-refs   # Update references instead of reverting moves
  python c_refactor_tool_cli.py --revert-moves  # Revert moves (default behavior)
  python c_refactor_tool_cli.py --dry-run       # Show what would be done
"""
from __future__ import annotations

import os
import re
import sys
import argparse
import subprocess
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass, field


@dataclass
class GitMove:
    """Represents a file move detected from git"""
    old_path: str
    new_path: str

    def __str__(self) -> str:
        return f"{self.old_path} -> {self.new_path}"


@dataclass
class FileUpdate:
    """Represents an update to a file"""
    file_path: str
    line_number: int
    old_content: str
    new_content: str


def get_project_root() -> str:
    """Get the git repository root directory"""
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--show-toplevel"],
            capture_output=True,
            text=True,
            check=True
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError:
        raise RuntimeError("Not in a git repository or git not available")


def detect_staged_moves() -> List[GitMove]:
    """Detect staged file moves from git status"""
    try:
        result = subprocess.run(
            ["git", "status", "--porcelain"],
            capture_output=True,
            text=True,
            check=True
        )

        moves = []
        lines = result.stdout.strip().split('\n')

        for line in lines:
            if not line:
                continue

            # Look for rename entries (R)
            if line.startswith('R'):
                # Format: R  old_path -> new_path (with spaces, not tabs)
                # Extract the path part after the status
                path_part = line[2:].strip()  # Remove 'R ' and any extra spaces
                if ' -> ' in path_part:
                    old_path, new_path = path_part.split(' -> ', 1)
                    moves.append(GitMove(old_path.strip(), new_path.strip()))

        return moves

    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Failed to get git status: {e}")


def revert_staged_moves(moves: List[GitMove], dry_run: bool = False) -> None:
    """Revert staged moves by moving files back"""
    print(f"{'[DRY RUN] ' if dry_run else ''}Reverting {len(moves)} staged moves...")

    for move in moves:
        print(f"{'[DRY RUN] ' if dry_run else ''}Reverting: {move}")

        if not dry_run:
            try:
                # Use git mv to revert the move
                subprocess.run(
                    ["git", "mv", move.new_path, move.old_path],
                    check=True,
                    capture_output=True
                )
                print(f"  ✓ Reverted: {move.new_path} -> {move.old_path}")

            except subprocess.CalledProcessError as e:
                print(f"  ✗ Failed to revert {move}: {e}")


def update_cmake_references(moves: List[GitMove], project_root: str, dry_run: bool = False) -> None:
    """Update CMakeLists.txt files to reflect the new file paths"""
    print(f"{'[DRY RUN] ' if dry_run else ''}Updating CMakeLists.txt references...")

    # Find all CMakeLists.txt files
    cmake_files = []
    for root, dirs, files in os.walk(project_root):
        # Skip ignored directories
        dirs[:] = [d for d in dirs if d not in {'.git', 'build', '.venv', 'venv', '__pycache__'}]

        for file in files:
            if file == 'CMakeLists.txt':
                cmake_files.append(os.path.join(root, file))

    print(f"Found {len(cmake_files)} CMakeLists.txt files")

    # Process each CMakeLists.txt
    for cmake_file in cmake_files:
        rel_cmake_path = os.path.relpath(cmake_file, project_root)
        print(f"Processing {rel_cmake_path}...")

        updates = find_cmake_updates(cmake_file, moves, project_root)
        if updates:
            print(f"  Found {len(updates)} updates needed")

            if not dry_run:
                apply_file_updates(cmake_file, updates)
                print(f"  ✓ Updated {rel_cmake_path}")
            else:
                for update in updates:
                    print(f"  [DRY RUN] Line {update.line_number}: {update.old_content.strip()} -> {update.new_content.strip()}")


def find_cmake_updates(cmake_file: str, moves: List[GitMove], project_root: str) -> List[FileUpdate]:
    """Find necessary updates in a CMakeLists.txt file"""
    updates = []

    try:
        with open(cmake_file, 'r', encoding='utf-8') as f:
            lines = f.readlines()
    except Exception as e:
        print(f"  ✗ Failed to read {cmake_file}: {e}")
        return updates

    # Get the directory containing this CMakeLists.txt for relative path resolution
    cmake_dir = os.path.dirname(cmake_file)

    for line_num, line in enumerate(lines, 1):
        for move in moves:
            # Convert absolute project paths to paths relative to the CMakeLists.txt directory
            old_abs_path = os.path.join(project_root, move.old_path)
            new_abs_path = os.path.join(project_root, move.new_path)

            old_rel_path = os.path.relpath(old_abs_path, cmake_dir).replace('\\', '/')
            new_rel_path = os.path.relpath(new_abs_path, cmake_dir).replace('\\', '/')

            # Check if this line references the moved file using the relative path
            if old_rel_path in line:
                # Case 1: Direct file reference with quotes
                if f'"{old_rel_path}"' in line or f"'{old_rel_path}'" in line:
                    new_line = line.replace(f'"{old_rel_path}"', f'"{new_rel_path}"')
                    new_line = new_line.replace(f"'{old_rel_path}'", f"'{new_rel_path}'")
                    if new_line != line:
                        updates.append(FileUpdate(cmake_file, line_num, line, new_line))
                        print(f"  [UPDATE] Line {line_num}: Found quoted path reference")

                # Case 2: Bare path reference (most common in CMake)
                elif old_rel_path in line and not line.strip().startswith('#'):
                    # Use word boundaries to avoid partial matches
                    pattern = r'\b' + re.escape(old_rel_path) + r'\b'
                    if re.search(pattern, line):
                        new_line = re.sub(pattern, new_rel_path, line)
                        if new_line != line:
                            updates.append(FileUpdate(cmake_file, line_num, line, new_line))
                            print(f"  [UPDATE] Line {line_num}: Found bare path reference")

                # Case 3: EXISTS check references
                elif 'EXISTS' in line and old_rel_path in line:
                    new_line = line.replace(old_rel_path, new_rel_path)
                    if new_line != line:
                        updates.append(FileUpdate(cmake_file, line_num, line, new_line))
                        print(f"  [UPDATE] Line {line_num}: Found EXISTS check reference")

    return updates


def apply_file_updates(file_path: str, updates: List[FileUpdate]) -> None:
    """Apply updates to a file"""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()

        # Apply updates in reverse order to maintain line numbers
        for update in sorted(updates, key=lambda u: u.line_number, reverse=True):
            if update.line_number <= len(lines):
                lines[update.line_number - 1] = update.new_content

        with open(file_path, 'w', encoding='utf-8') as f:
            f.writelines(lines)

    except Exception as e:
        raise RuntimeError(f"Failed to update {file_path}: {e}")


def update_source_references(moves: List[GitMove], project_root: str, dry_run: bool = False) -> None:
    """Update #include references in source files"""
    print(f"{'[DRY RUN] ' if dry_run else ''}Updating #include references...")

    source_extensions = {'.c', '.cpp', '.cc', '.cxx', '.h', '.hpp', '.hh', '.hxx'}
    source_files = []

    # Find all source files
    for root, dirs, files in os.walk(project_root):
        # Skip ignored directories
        dirs[:] = [d for d in dirs if d not in {'.git', 'build', '.venv', 'venv', '__pycache__'}]

        for file in files:
            if any(file.endswith(ext) for ext in source_extensions):
                source_files.append(os.path.join(root, file))

    print(f"Scanning {len(source_files)} source files...")

    total_updates = 0
    for source_file in source_files:
        updates = find_include_updates(source_file, moves, project_root)
        if updates:
            total_updates += len(updates)
            rel_path = os.path.relpath(source_file, project_root)
            print(f"  {rel_path}: {len(updates)} updates")

            if not dry_run:
                apply_file_updates(source_file, updates)

    print(f"{'[DRY RUN] ' if dry_run else ''}Updated {total_updates} #include references")


def find_include_updates(source_file: str, moves: List[GitMove], project_root: str) -> List[FileUpdate]:
    """Find necessary #include updates in a source file"""
    updates = []

    try:
        with open(source_file, 'r', encoding='utf-8') as f:
            lines = f.readlines()
    except Exception as e:
        print(f"  ✗ Failed to read {source_file}: {e}")
        return updates

    for line_num, line in enumerate(lines, 1):
        # Look for #include statements
        include_match = re.match(r'^\s*#include\s+["\']([^"\']+)["\']', line)
        if include_match:
            included_path = include_match.group(1)

            # Check if any move affects this include
            for move in moves:
                # Convert absolute paths to relative if needed
                old_basename = os.path.basename(move.old_path)
                new_basename = os.path.basename(move.new_path)

                # Simple case: if the included file matches the moved file basename
                if included_path.endswith(old_basename):
                    # Calculate new relative path
                    source_dir = os.path.dirname(source_file)
                    new_abs_path = os.path.join(project_root, move.new_path)
                    new_rel_path = os.path.relpath(new_abs_path, source_dir)

                    # Normalize path separators
                    new_rel_path = new_rel_path.replace('\\', '/')

                    # Replace the include
                    new_line = line.replace(included_path, new_rel_path)
                    if new_line != line:
                        updates.append(FileUpdate(source_file, line_num, line, new_line))

    return updates


def main() -> int:
    parser = argparse.ArgumentParser(
        description='CLI C Refactor Tool - Handle git staged moves and update references',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python c_refactor_tool_cli.py --update-refs    # Update all references to moved files
  python c_refactor_tool_cli.py --revert-moves   # Revert staged moves (default)
  python c_refactor_tool_cli.py --dry-run        # Show what would be done
        """
    )

    parser.add_argument(
        '--update-refs',
        action='store_true',
        help='Update references instead of reverting moves'
    )
    parser.add_argument(
        '--revert-moves',
        action='store_true',
        help='Revert staged moves (default behavior)'
    )
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Show what would be done without making changes'
    )
    parser.add_argument(
        '--cmake-only',
        action='store_true',
        help='Only update CMake files, skip source file includes'
    )

    args = parser.parse_args()

    # Default to revert behavior if no action specified
    if not args.update_refs and not args.revert_moves:
        args.revert_moves = True

    try:
        # Get project root and detect moves
        project_root = get_project_root()
        print(f"Project root: {project_root}")

        moves = detect_staged_moves()

        if not moves:
            print("No staged file moves detected.")
            return 0

        print(f"Detected {len(moves)} staged moves:")
        for move in moves:
            print(f"  {move}")

        if args.revert_moves:
            revert_staged_moves(moves, dry_run=args.dry_run)
        elif args.update_refs:
            print("\nUpdating references to moved files...")
            update_cmake_references(moves, project_root, dry_run=args.dry_run)

            if not args.cmake_only:
                update_source_references(moves, project_root, dry_run=args.dry_run)

            print(f"{'[DRY RUN] ' if args.dry_run else ''}Reference updates complete!")

        return 0

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

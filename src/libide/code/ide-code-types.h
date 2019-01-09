/* ide-code-types.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if !defined (IDE_CODE_INSIDE) && !defined (IDE_CODE_COMPILATION)
# error "Only <libide-code.h> can be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

typedef struct _IdeBuffer IdeBuffer;
typedef struct _IdeBufferAddin IdeBufferAddin;
typedef struct _IdeBufferChangeMonitor IdeBufferChangeMonitor;
typedef struct _IdeCodeIndexEntries IdeCodeIndexEntries;
typedef struct _IdeCodeIndexEntry IdeCodeIndexEntry;
typedef struct _IdeCodeIndexer IdeCodeIndexer;
typedef struct _IdeBufferManager IdeBufferManager;
typedef struct _IdeDiagnostic IdeDiagnostic;
typedef struct _IdeDiagnosticProvider IdeDiagnosticProvider;
typedef struct _IdeDiagnostics IdeDiagnostics;
typedef struct _IdeDiagnosticsManager IdeDiagnosticsManager;
typedef struct _IdeFile IdeFile;
typedef struct _IdeFileSettings IdeFileSettings;
typedef struct _IdeFormatter IdeFormatter;
typedef struct _IdeFormatterOptions IdeFormatterOptions;
typedef struct _IdeHighlightEngine IdeHighlightEngine;
typedef struct _IdeHighlightIndex IdeHighlightIndex;
typedef struct _IdeHighlighter IdeHighlighter;
typedef struct _IdeLocation IdeLocation;
typedef struct _IdeRange IdeRange;
typedef struct _IdeRenameProvider IdeRenameProvider;
typedef struct _IdeSymbol IdeSymbol;
typedef struct _IdeSymbolNode IdeSymbolNode;
typedef struct _IdeSymbolResolver IdeSymbolResolver;
typedef struct _IdeSymbolTree IdeSymbolTree;
typedef struct _IdeTextEdit IdeTextEdit;
typedef struct _IdeUnsavedFile IdeUnsavedFile;
typedef struct _IdeUnsavedFiles IdeUnsavedFiles;

G_END_DECLS

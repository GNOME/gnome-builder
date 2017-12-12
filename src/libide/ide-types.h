/* ide-types.h
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _IdeBackForwardItem             IdeBackForwardItem;
typedef struct _IdeBackForwardList             IdeBackForwardList;

typedef struct _IdeBuffer                      IdeBuffer;
typedef struct _IdeBufferChangeMonitor         IdeBufferChangeMonitor;
typedef struct _IdeBufferManager               IdeBufferManager;

typedef struct _IdeBuildCommand                IdeBuildCommand;
typedef struct _IdeBuildCommandQueue           IdeBuildCommandQueue;
typedef struct _IdeBuilder                     IdeBuilder;
typedef struct _IdeBuildManager                IdeBuildManager;
typedef struct _IdeBuildPipeline               IdeBuildPipeline;
typedef struct _IdeBuildResult                 IdeBuildResult;
typedef struct _IdeBuildStage                  IdeBuildStage;
typedef struct _IdeBuildSystem                 IdeBuildSystem;
typedef struct _IdeBuildTarget                 IdeBuildTarget;

typedef struct _IdeConfiguration               IdeConfiguration;
typedef struct _IdeConfigurationManager        IdeConfigurationManager;

typedef struct _IdeContext                     IdeContext;

typedef struct _IdeDebugManager                IdeDebugManager;
typedef struct _IdeDebugger                    IdeDebugger;
typedef struct _IdeDebuggerBreakpoint          IdeDebuggerBreakpoint;
typedef struct _IdeDebuggerFrame               IdeDebuggerFrame;
typedef struct _IdeDebuggerInstruction         IdeDebuggerInstruction;
typedef struct _IdeDebuggerRegister            IdeDebuggerRegister;
typedef struct _IdeDebuggerVariable            IdeDebuggerVariable;

typedef struct _IdeDevice                      IdeDevice;
typedef struct _IdeDeviceManager               IdeDeviceManager;
typedef struct _IdeDeviceProvider              IdeDeviceProvider;

typedef struct _IdeDiagnostic                  IdeDiagnostic;
typedef struct _IdeDiagnosticProvider          IdeDiagnosticProvider;
typedef struct _IdeDiagnostics                 IdeDiagnostics;
typedef struct _IdeDiagnosticsManager          IdeDiagnosticsManager;

typedef struct _IdeDocumentation               IdeDocumentation;
typedef struct _IdeDocumentationInfo           IdeDocumentationInfo;
typedef struct _IdeDocumentationProposal       IdeDocumentationProposal;

typedef struct _IdeEnvironment                 IdeEnvironment;
typedef struct _IdeEnvironmentVariable         IdeEnvironmentVariable;

typedef struct _IdeFile                        IdeFile;

typedef struct _IdeFileSettings                IdeFileSettings;

typedef struct _IdeFixit                       IdeFixit;

typedef struct _IdeHighlighter                 IdeHighlighter;
typedef struct _IdeHighlightEngine             IdeHighlightEngine;

typedef struct _IdeIndenter                    IdeIndenter;

typedef struct _IdeObject                      IdeObject;

typedef struct _IdePausable                    IdePausable;

typedef struct _IdeProgress                    IdeProgress;

typedef struct _IdeProject                     IdeProject;

typedef struct _IdeProjectItem                 IdeProjectItem;

typedef struct _IdeProjectEdit                 IdeProjectEdit;

typedef struct _IdeProjectFile                 IdeProjectFile;

typedef struct _IdeProjectFiles                IdeProjectFiles;

typedef struct _IdeRenameProvider              IdeRenameProvider;

typedef struct _IdeRunner                      IdeRunner;
typedef struct _IdeRunManager                  IdeRunManager;

typedef struct _IdeRuntime                     IdeRuntime;
typedef struct _IdeRuntimeManager              IdeRuntimeManager;
typedef struct _IdeRuntimeProvider             IdeRuntimeProvider;

typedef struct _IdeSearchEngine                IdeSearchEngine;
typedef struct _IdeSearchProvider              IdeSearchProvider;
typedef struct _IdeSearchResult                IdeSearchResult;

typedef struct _IdeService                     IdeService;

typedef struct _IdeSettings                    IdeSettings;

typedef struct _IdeSourceLocation              IdeSourceLocation;
typedef struct _IdeSourceRange                 IdeSourceRange;

typedef struct _IdeSourceSnippet               IdeSourceSnippet;
typedef struct _IdeSourceSnippetChunk          IdeSourceSnippetChunk;
typedef struct _IdeSourceSnippetContext        IdeSourceSnippetContext;
typedef struct _IdeSourceSnippets              IdeSourceSnippets;
typedef struct _IdeSourceSnippetsManager       IdeSourceSnippetsManager;

typedef struct _IdeSubprocess                  IdeSubprocess;
typedef struct _IdeSubprocessLauncher          IdeSubprocessLauncher;

typedef struct _IdeSymbol                      IdeSymbol;
typedef struct _IdeSymbolResolver              IdeSymbolResolver;

typedef struct _IdeTest                        IdeTest;
typedef struct _IdeTestManager                 IdeTestManager;
typedef struct _IdeTestProvider                IdeTestProvider;

typedef struct _IdeTransferManager             IdeTransferManager;
typedef struct _IdeTransfer                    IdeTransfer;

typedef struct _IdeUnsavedFile                 IdeUnsavedFile;
typedef struct _IdeUnsavedFiles                IdeUnsavedFiles;

typedef struct _IdeVcs                         IdeVcs;
typedef struct _IdeVcsMonitor                  IdeVcsMonitor;

G_END_DECLS

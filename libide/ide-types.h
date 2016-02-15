/* ide-types.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_TYPES_H
#define IDE_TYPES_H

#include <glib-object.h>

#include "ide-macros.h"

G_BEGIN_DECLS

typedef struct _IdeBackForwardItem             IdeBackForwardItem;

typedef struct _IdeBackForwardList             IdeBackForwardList;

typedef struct _IdeBuffer                      IdeBuffer;

typedef struct _IdeBufferChangeMonitor         IdeBufferChangeMonitor;

typedef struct _IdeBufferManager               IdeBufferManager;

typedef struct _IdeBuilder                     IdeBuilder;

typedef struct _IdeBuildResult                 IdeBuildResult;

typedef struct _IdeBuildSystem                 IdeBuildSystem;

typedef struct _IdeConfiguration               IdeConfiguration;
typedef struct _IdeConfigurationManager        IdeConfigurationManager;

typedef struct _IdeContext                     IdeContext;

typedef struct _IdeDebugger                    IdeDebugger;
typedef struct _IdeDebuggerInterface           IdeDebuggerInterface;

typedef struct _IdeDeployer                    IdeDeployer;

typedef struct _IdeDevice                      IdeDevice;
typedef struct _IdeDeviceManager               IdeDeviceManager;
typedef struct _IdeDeviceProvider              IdeDeviceProvider;

typedef struct _IdeDiagnostic                  IdeDiagnostic;

typedef struct _IdeDiagnostics                 IdeDiagnostics;

typedef struct _IdeDiagnostician               IdeDiagnostician;

typedef struct _IdeDiagnosticProvider          IdeDiagnosticProvider;

typedef struct _IdeEnvironment                 IdeEnvironment;
typedef struct _IdeEnvironmentVariable         IdeEnvironmentVariable;

typedef struct _IdeExecuter                    IdeExecuter;
typedef struct _IdeExecuterInterface           IdeExecuterInterface;

typedef struct _IdeExecutable                  IdeExecutable;
typedef struct _IdeExecutableInterface         IdeExecutableInterface;

typedef struct _IdeFile                        IdeFile;

typedef struct _IdeFileSettings                IdeFileSettings;

typedef struct _IdeFixit                       IdeFixit;

typedef struct _IdeHighlighter                 IdeHighlighter;

typedef struct _IdeIndenter                    IdeIndenter;

typedef struct _IdeLanguage                    IdeLanguage;

typedef struct _IdeObject                      IdeObject;

typedef struct _IdeProcess                     IdeProcess;
typedef struct _IdeProcessInterface            IdeProcessInterface;

typedef struct _IdeProgress                    IdeProgress;

typedef struct _IdeProject                     IdeProject;

typedef struct _IdeProjectItem                 IdeProjectItem;

typedef struct _IdeProjectFile                 IdeProjectFile;

typedef struct _IdeProjectFiles                IdeProjectFiles;

typedef struct _IdeRefactory                   IdeRefactory;
typedef struct _IdeRefactoryInterface          IdeRefactoryInterface;

typedef struct _IdeRuntime                     IdeRuntime;
typedef struct _IdeRuntimeManager              IdeRuntimeManager;
typedef struct _IdeRuntimeProvider             IdeRuntimeProvider;

typedef struct _IdeScript                      IdeScript;

typedef struct _IdeScriptManager               IdeScriptManager;

typedef struct _IdeSearchContext               IdeSearchContext;

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

typedef struct _IdeSymbol                      IdeSymbol;

typedef struct _IdeSymbolResolver              IdeSymbolResolver;
typedef struct _IdeSymbolResolverInterface     IdeSymbolResolverInterface;

typedef struct _IdeTarget                      IdeTarget;
typedef struct _IdeTargetInterface             IdeTargetInterface;

typedef struct _IdeTestCase                    IdeTestCase;
typedef struct _IdeTestCaseInterface           IdeTestCaseInterface;

typedef struct _IdeTestSuite                   IdeTestSuite;
typedef struct _IdeTestSuiteInterface          IdeTestSuiteInterface;

typedef struct _IdeUnsavedFiles                IdeUnsavedFiles;

typedef struct _IdeUnsavedFile                 IdeUnsavedFile;

typedef struct _IdeVcs                         IdeVcs;

typedef struct _IdeHighlightEngine             IdeHighlightEngine;

G_END_DECLS

#endif /* IDE_TYPES_H */

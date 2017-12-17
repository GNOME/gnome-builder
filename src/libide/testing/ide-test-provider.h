/* ide-test-provider.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#include "ide-version-macros.h"

#include "ide-object.h"

#include "buildsystem/ide-build-pipeline.h"
#include "testing/ide-test.h"

G_BEGIN_DECLS

#define IDE_TYPE_TEST_PROVIDER (ide_test_provider_get_type ())

G_DECLARE_DERIVABLE_TYPE (IdeTestProvider, ide_test_provider, IDE, TEST_PROVIDER, IdeObject)

struct _IdeTestProviderClass
{
  IdeObjectClass parent_class;

  void     (*run_async)  (IdeTestProvider      *self,
                          IdeTest              *test,
                          IdeBuildPipeline     *pipeline,
                          GCancellable         *cancellable,
                          GAsyncReadyCallback   callback,
                          gpointer              user_data);
  gboolean (*run_finish) (IdeTestProvider      *self,
                          GAsyncResult         *result,
                          GError              **error);
  void     (*reload)     (IdeTestProvider      *self);

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

IDE_AVAILABLE_IN_3_28
gboolean ide_test_provider_get_loading (IdeTestProvider      *self);
IDE_AVAILABLE_IN_3_28
void     ide_test_provider_set_loading (IdeTestProvider      *self,
                                        gboolean              loading);
IDE_AVAILABLE_IN_3_28
void     ide_test_provider_clear       (IdeTestProvider      *self);
IDE_AVAILABLE_IN_3_28
void     ide_test_provider_add         (IdeTestProvider      *self,
                                        IdeTest              *test);
IDE_AVAILABLE_IN_3_28
void     ide_test_provider_remove      (IdeTestProvider      *self,
                                        IdeTest              *test);
IDE_AVAILABLE_IN_3_28
void     ide_test_provider_run_async   (IdeTestProvider      *self,
                                        IdeTest              *test,
                                        IdeBuildPipeline     *pipeline,
                                        GCancellable         *cancellable,
                                        GAsyncReadyCallback   callback,
                                        gpointer              user_data);
IDE_AVAILABLE_IN_3_28
gboolean ide_test_provider_run_finish  (IdeTestProvider      *self,
                                        GAsyncResult         *result,
                                        GError              **error);
IDE_AVAILABLE_IN_3_28
void     ide_test_provider_reload      (IdeTestProvider      *self);

G_END_DECLS

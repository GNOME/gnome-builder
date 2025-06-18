/* gbp-file-search-result.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gbp-file-search-result"

#include <libide-editor.h>
#include <libide-gui.h>

#include "gbp-file-search-result.h"

struct _GbpFileSearchResult
{
  IdeSearchResult  parent_instance;
  char            *path;
};

G_DEFINE_FINAL_TYPE (GbpFileSearchResult, gbp_file_search_result, IDE_TYPE_SEARCH_RESULT)

enum {
  PROP_0,
  PROP_PATH,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_file_search_result_activate (IdeSearchResult *result,
                                 GtkWidget       *last_focus)
{
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GFile) file = NULL;
  IdeWorkbench *workbench;
  IdeContext *context;

  g_assert (GBP_IS_FILE_SEARCH_RESULT (result));
  g_assert (!last_focus || GTK_IS_WIDGET (last_focus));

  if (!last_focus)
    return;

  if (!(workbench = ide_widget_get_workbench (last_focus)) ||
      !(context = ide_workbench_get_context (workbench)) ||
      !(workdir = ide_context_ref_workdir (context)))
    return;

  file = g_file_get_child (workdir, GBP_FILE_SEARCH_RESULT (result)->path);

  ide_workbench_open_async (workbench, file, NULL, 0, NULL, NULL, NULL, NULL);
}

static IdeSearchPreview *
gbp_file_search_result_load_preview (IdeSearchResult *result,
                                     IdeContext      *context)
{
  GbpFileSearchResult *self = (GbpFileSearchResult *)result;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GFile) file = NULL;
  IdeSearchPreview *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FILE_SEARCH_RESULT (self));
  g_assert (IDE_IS_CONTEXT (context));

  workdir = ide_context_ref_workdir (context);
  file = g_file_get_child (workdir, self->path);
  ret = ide_file_search_preview_new (file);

  IDE_RETURN (ret);
}

static void
gbp_file_search_result_finalize (GObject *object)
{
  GbpFileSearchResult *self = (GbpFileSearchResult *)object;

  g_clear_pointer (&self->path, g_free);

  G_OBJECT_CLASS (gbp_file_search_result_parent_class)->finalize (object);
}

static void
gbp_file_search_result_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GbpFileSearchResult *self = (GbpFileSearchResult *)object;

  switch (prop_id)
    {
    case PROP_PATH:
      g_value_set_string (value, self->path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_file_search_result_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GbpFileSearchResult *self = (GbpFileSearchResult *)object;

  switch (prop_id)
    {
    case PROP_PATH:
      self->path = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_file_search_result_class_init (GbpFileSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSearchResultClass *result_class = IDE_SEARCH_RESULT_CLASS (klass);

  object_class->finalize = gbp_file_search_result_finalize;
  object_class->get_property = gbp_file_search_result_get_property;
  object_class->set_property = gbp_file_search_result_set_property;

  result_class->activate = gbp_file_search_result_activate;
  result_class->load_preview = gbp_file_search_result_load_preview;

  properties [PROP_PATH] =
    g_param_spec_string ("path",
                         "Path",
                         "The relative path to the file.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_file_search_result_init (GbpFileSearchResult *self)
{
}

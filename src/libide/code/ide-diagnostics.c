/* ide-diagnostics.c
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-diagnostics"

#include "config.h"

#include "ide-diagnostic.h"
#include "ide-diagnostics.h"
#include "ide-location.h"

typedef struct
{
  GPtrArray  *items;
  GHashTable *caches;
  guint       n_warnings;
  guint       n_errors;
} IdeDiagnosticsPrivate;

typedef struct
{
  gint                  line : 28;
  IdeDiagnosticSeverity severity : 4;
} IdeDiagnosticsCacheLine;

typedef struct
{
  GFile  *file;
  GArray *lines;
} IdeDiagnosticsCache;

typedef struct
{
  guint begin;
  guint end;
} LookupKey;

enum {
  PROP_0,
  PROP_HAS_ERRORS,
  PROP_HAS_WARNINGS,
  PROP_N_ERRORS,
  PROP_N_WARNINGS,
  N_PROPS
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeDiagnostics, ide_diagnostics, IDE_TYPE_OBJECT,
                         G_ADD_PRIVATE (IdeDiagnostics)
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties [N_PROPS];

static void
ide_diagnostics_cache_free (gpointer data)
{
  IdeDiagnosticsCache *cache = data;

  g_clear_object (&cache->file);
  g_clear_pointer (&cache->lines, g_array_unref);
  g_slice_free (IdeDiagnosticsCache, cache);
}

static void
ide_diagnostics_finalize (GObject *object)
{
  IdeDiagnostics *self = (IdeDiagnostics *)object;
  IdeDiagnosticsPrivate *priv = ide_diagnostics_get_instance_private (self);

  g_clear_pointer (&priv->items, g_ptr_array_unref);
  g_clear_pointer (&priv->caches, g_hash_table_unref);

  G_OBJECT_CLASS (ide_diagnostics_parent_class)->finalize (object);
}

static void
ide_diagnostics_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeDiagnostics *self = IDE_DIAGNOSTICS (object);

  switch (prop_id)
    {
    case PROP_HAS_WARNINGS:
      g_value_set_boolean (value, ide_diagnostics_get_has_warnings (self));
      break;

    case PROP_HAS_ERRORS:
      g_value_set_boolean (value, ide_diagnostics_get_has_errors (self));
      break;

    case PROP_N_ERRORS:
      g_value_set_uint (value, ide_diagnostics_get_n_errors (self));
      break;

    case PROP_N_WARNINGS:
      g_value_set_uint (value, ide_diagnostics_get_n_warnings (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_diagnostics_class_init (IdeDiagnosticsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_diagnostics_finalize;
  object_class->get_property = ide_diagnostics_get_property;

  properties [PROP_HAS_WARNINGS] =
    g_param_spec_boolean ("has-warnings",
                         "Has Warnings",
                         "If there are any warnings in the diagnostic set",
                         FALSE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_HAS_ERRORS] =
    g_param_spec_boolean ("has-errors",
                         "Has Errors",
                         "If there are any errors in the diagnostic set",
                         FALSE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_N_WARNINGS] =
    g_param_spec_uint ("n-warnings",
                       "N Warnings",
                       "Number of warnings in diagnostic set",
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_N_ERRORS] =
    g_param_spec_uint ("n-errors",
                       "N Errors",
                       "Number of errors in diagnostic set",
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_diagnostics_init (IdeDiagnostics *self)
{
  IdeDiagnosticsPrivate *priv = ide_diagnostics_get_instance_private (self);

  priv->items = g_ptr_array_new_with_free_func (g_object_unref);
}

IdeDiagnostics *
ide_diagnostics_new (void)
{
  return g_object_new (IDE_TYPE_DIAGNOSTICS, NULL);
}

void
ide_diagnostics_add (IdeDiagnostics *self,
                     IdeDiagnostic  *diagnostic)
{
  g_return_if_fail (IDE_IS_DIAGNOSTICS (self));
  g_return_if_fail (IDE_IS_DIAGNOSTIC (diagnostic));

  ide_diagnostics_take (self, g_object_ref (diagnostic));
}

void
ide_diagnostics_take (IdeDiagnostics *self,
                      IdeDiagnostic  *diagnostic)
{
  IdeDiagnosticsPrivate *priv = ide_diagnostics_get_instance_private (self);
  IdeDiagnosticSeverity severity;
  guint position;

  g_return_if_fail (IDE_IS_DIAGNOSTICS (self));
  g_return_if_fail (IDE_IS_DIAGNOSTIC (diagnostic));

  severity = ide_diagnostic_get_severity (diagnostic);

  position = priv->items->len;
  g_ptr_array_add (priv->items, g_steal_pointer (&diagnostic));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);

  switch (severity)
    {
    case IDE_DIAGNOSTIC_ERROR:
    case IDE_DIAGNOSTIC_FATAL:
      priv->n_errors++;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_ERRORS]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_N_ERRORS]);
      break;

    case IDE_DIAGNOSTIC_WARNING:
    case IDE_DIAGNOSTIC_DEPRECATED:
    case IDE_DIAGNOSTIC_UNUSED:
      priv->n_warnings++;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_WARNINGS]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_N_WARNINGS]);
      break;

    case IDE_DIAGNOSTIC_IGNORED:
    case IDE_DIAGNOSTIC_NOTE:
    default:
      break;
    }
}

void
ide_diagnostics_merge (IdeDiagnostics *self,
                       IdeDiagnostics *other)
{
  IdeDiagnosticsPrivate *priv = ide_diagnostics_get_instance_private (self);
  IdeDiagnosticsPrivate *other_priv = ide_diagnostics_get_instance_private (other);
  guint position;

  g_return_if_fail (IDE_IS_DIAGNOSTICS (self));
  g_return_if_fail (IDE_IS_DIAGNOSTICS (other));

  position = priv->items->len;

  for (guint i = 0; i < other_priv->items->len; i++)
    {
      IdeDiagnostic *diagnostic = g_ptr_array_index (other_priv->items, i);
      ide_diagnostics_take (self, g_object_ref (diagnostic));
    }

  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, other_priv->items->len);
}

gboolean
ide_diagnostics_get_has_errors (IdeDiagnostics *self)
{
  IdeDiagnosticsPrivate *priv = ide_diagnostics_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTICS (self), FALSE);

  return priv->n_errors > 0;
}

guint
ide_diagnostics_get_n_errors (IdeDiagnostics *self)
{
  IdeDiagnosticsPrivate *priv = ide_diagnostics_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTICS (self), 0);

  return priv->n_errors;
}

gboolean
ide_diagnostics_get_has_warnings (IdeDiagnostics *self)
{
  IdeDiagnosticsPrivate *priv = ide_diagnostics_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTICS (self), FALSE);

  return priv->n_warnings > 0;
}

guint
ide_diagnostics_get_n_warnings (IdeDiagnostics *self)
{
  IdeDiagnosticsPrivate *priv = ide_diagnostics_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTICS (self), 0);

  return priv->n_warnings;
}

static GType
ide_diagnostics_get_item_type (GListModel *model)
{
  return IDE_TYPE_DIAGNOSTIC;
}

static guint
ide_diagnostics_get_n_items (GListModel *model)
{
  IdeDiagnostics *self = (IdeDiagnostics *)model;
  IdeDiagnosticsPrivate *priv = ide_diagnostics_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTICS (self), 0);

  return priv->items->len;
}

static gpointer
ide_diagnostics_get_item (GListModel *model,
                          guint       position)
{
  IdeDiagnostics *self = (IdeDiagnostics *)model;
  IdeDiagnosticsPrivate *priv = ide_diagnostics_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTICS (self), NULL);

  if (position < priv->items->len)
    return g_object_ref (g_ptr_array_index (priv->items, position));

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = ide_diagnostics_get_n_items;
  iface->get_item_type = ide_diagnostics_get_item_type;
  iface->get_item = ide_diagnostics_get_item;
}

static gint
compare_lines (gconstpointer a,
               gconstpointer b)
{
  const IdeDiagnosticsCacheLine *line_a = a;
  const IdeDiagnosticsCacheLine *line_b = b;

  return line_a->line - line_b->line;
}

static void
ide_diagnostics_build_caches (IdeDiagnostics *self)
{
  IdeDiagnosticsPrivate *priv = ide_diagnostics_get_instance_private (self);
  g_autoptr(GHashTable) caches = NULL;
  IdeDiagnosticsCache *cache;
  GHashTableIter iter;
  GFile *file;

  g_assert (IDE_IS_DIAGNOSTICS (self));
  g_assert (priv->caches == NULL);

  caches = g_hash_table_new_full (g_file_hash,
                                  (GEqualFunc)g_file_equal,
                                  g_object_unref,
                                  ide_diagnostics_cache_free);

  for (guint i = 0; i < priv->items->len; i++)
    {
      IdeDiagnostic *diag = g_ptr_array_index (priv->items, i);
      IdeDiagnosticsCacheLine val;
      IdeLocation *location;

      if (!(file = ide_diagnostic_get_file (diag)))
        continue;

      if (!(location = ide_diagnostic_get_location (diag)))
        continue;

      if (!(cache = g_hash_table_lookup (caches, file)))
        {
          cache = g_slice_new0 (IdeDiagnosticsCache);
          cache->file = g_object_ref (file);
          cache->lines = g_array_new (FALSE, FALSE, sizeof (IdeDiagnosticsCacheLine));
          g_hash_table_insert (caches, g_object_ref (file), cache);
        }

      val.severity = ide_diagnostic_get_severity (diag);
      val.line = ide_location_get_line (location);

      g_array_append_val (cache->lines, val);
    }

  g_hash_table_iter_init (&iter, caches);

  while (g_hash_table_iter_next (&iter, (gpointer *)&file, (gpointer *)&cache))
    g_array_sort (cache->lines, compare_lines);

  priv->caches = g_steal_pointer (&caches);
}

/**
 * ide_diagnostics_foreach_line_in_range:
 * @self: an #IdeDiagnostics
 * @file: a #GFile
 * @begin_line: the starting line
 * @end_line: the ending line
 * @callback: (scope call): a callback to execute for each matching line
 * @user_data: user data for @callback
 *
 * This function calls @callback for every line with diagnostics between
 * @begin_line and @end_line. This is useful when drawing information about
 * diagnostics in an editor where a known number of lines are visible.
 */
void
ide_diagnostics_foreach_line_in_range (IdeDiagnostics             *self,
                                       GFile                      *file,
                                       guint                       begin_line,
                                       guint                       end_line,
                                       IdeDiagnosticsLineCallback  callback,
                                       gpointer                    user_data)
{
  IdeDiagnosticsPrivate *priv = ide_diagnostics_get_instance_private (self);
  const IdeDiagnosticsCache *cache;

  g_return_if_fail (IDE_IS_DIAGNOSTICS (self));
  g_return_if_fail (G_IS_FILE (file));

  if (priv->items->len == 0)
    return;

  if (priv->caches == NULL)
    ide_diagnostics_build_caches (self);

  if (!(cache = g_hash_table_lookup (priv->caches, file)))
    return;

  for (guint i = 0; i < cache->lines->len; i++)
    {
      const IdeDiagnosticsCacheLine *line = &g_array_index (cache->lines, IdeDiagnosticsCacheLine, i);

      if (line->line < begin_line)
        continue;

      if (line->line > end_line)
        break;

      callback (line->line, line->severity, user_data);
    }
}

/**
 * ide_diagnostics_get_diagnostic_at_line:
 * @self: a #IdeDiagnostics
 * @file: the target file
 * @line: a line number
 *
 * Locates an #IdeDiagnostic in @file at @line.
 *
 * Returns: (transfer none) (nullable): an #IdeDiagnostic or %NULL
 */
IdeDiagnostic *
ide_diagnostics_get_diagnostic_at_line (IdeDiagnostics *self,
                                        GFile          *file,
                                        guint           line)
{
  IdeDiagnosticsPrivate *priv = ide_diagnostics_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTICS (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  for (guint i = 0; i < priv->items->len; i++)
    {
      IdeDiagnostic *diag = g_ptr_array_index (priv->items, i);
      IdeLocation *loc = ide_diagnostic_get_location (diag);
      GFile *loc_file;
      guint loc_line;

      if (loc == NULL)
        continue;

      loc_file = ide_location_get_file (loc);
      loc_line = ide_location_get_line (loc);

      if (loc_line == line && g_file_equal (file, loc_file))
        return diag;
    }

  return NULL;
}

/**
 * ide_diagnostics_get_diagnostics_at_line:
 * @self: a #IdeDiagnostics
 * @file: the target file
 * @line: a line number
 *
 * Locates all #IdeDiagnostic in @file at @line.
 *
 * Returns: (transfer full) (element-type IdeDiagnostic) (nullable): an #GPtrArray or %NULL
 */
GPtrArray *
ide_diagnostics_get_diagnostics_at_line (IdeDiagnostics *self,
                                         GFile          *file,
                                         guint           line)
{
  IdeDiagnosticsPrivate *priv = ide_diagnostics_get_instance_private (self);
  g_autoptr(GPtrArray) valid_diag = NULL;

  g_return_val_if_fail (IDE_IS_DIAGNOSTICS (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  valid_diag = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < priv->items->len; i++)
    {
      IdeDiagnostic *diag = g_ptr_array_index (priv->items, i);
      IdeLocation *loc = ide_diagnostic_get_location (diag);
      GFile *loc_file;
      guint loc_line;

      if (loc == NULL)
        continue;

      loc_file = ide_location_get_file (loc);
      loc_line = ide_location_get_line (loc);

      if (loc_line == line && g_file_equal (file, loc_file))
        g_ptr_array_add (valid_diag, g_object_ref(diag));
    }

  if (valid_diag->len != 0)
    return IDE_PTR_ARRAY_STEAL_FULL (&valid_diag);

  return NULL;
}

/**
 * ide_diagnostics_new_from_array:
 * @array: (nullable) (element-type IdeDiagnostic): optional array
 *   of diagnostics to add.
 *
 * Returns: (transfer full): an #IdeDiagnostics
 */
IdeDiagnostics *
ide_diagnostics_new_from_array (GPtrArray *array)
{
  IdeDiagnostics *ret = ide_diagnostics_new ();

  if (array != NULL)
    {
      for (guint i = 0; i < array->len; i++)
        ide_diagnostics_add (ret, g_ptr_array_index (array, i));
    }

  return g_steal_pointer (&ret);
}

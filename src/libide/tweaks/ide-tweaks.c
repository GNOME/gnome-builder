/* ide-tweaks.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-tweaks"

#include "config.h"

#include <gtk/gtk.h>

#include "ide-tweaks.h"
#include "ide-tweaks-section.h"

struct _IdeTweaks
{
  IdeTweaksItem    parent_instance;
  IdeContext      *context;
  GtkBuilder      *builder;
  GtkBuilderScope *scope;
  char            *project_id;
};

G_DEFINE_FINAL_TYPE (IdeTweaks, ide_tweaks, IDE_TYPE_TWEAKS_ITEM)

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_PROJECT_ID,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

G_GNUC_PRINTF (2, 3)
static char *
ide_tweaks_format (IdeTweaks  *self,
                   const char *format,
                   ...)
{
  va_list args;
  char *ret;

  g_assert (IDE_IS_TWEAKS (self));

  va_start (args, format);
  ret = g_strdup_vprintf (format, args);
  va_end (args);

  return ret;
}

static gboolean
ide_tweaks_accepts (IdeTweaksItem *item,
                    IdeTweaksItem *child)
{
  g_assert (IDE_IS_TWEAKS_ITEM (item));
  g_assert (IDE_IS_TWEAKS_ITEM (child));

  return IDE_IS_TWEAKS_SECTION (child);
}

static void
ide_tweaks_constructed (GObject *object)
{
  IdeTweaks *self = (IdeTweaks *)object;

  G_OBJECT_CLASS (ide_tweaks_parent_class)->constructed (object);

  if (self->context != NULL)
    {
      if (self->project_id == NULL)
        self->project_id = ide_context_dup_project_id (self->context);
      ide_tweaks_expose_object (self, "IdeContext", G_OBJECT (self->context));
    }
}

static void
ide_tweaks_dispose (GObject *object)
{
  IdeTweaks *self = (IdeTweaks *)object;

  g_clear_object (&self->builder);
  g_clear_object (&self->scope);
  g_clear_pointer (&self->project_id, g_free);

  G_OBJECT_CLASS (ide_tweaks_parent_class)->dispose (object);
}

static void
ide_tweaks_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  IdeTweaks *self = IDE_TWEAKS (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, ide_tweaks_get_context (self));
      break;

    case PROP_PROJECT_ID:
      g_value_set_string (value, ide_tweaks_get_project_id (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  IdeTweaks *self = IDE_TWEAKS (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      self->context = g_value_dup_object (value);
      break;

    case PROP_PROJECT_ID:
      ide_tweaks_set_project_id (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_class_init (IdeTweaksClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTweaksItemClass *item_class = IDE_TWEAKS_ITEM_CLASS (klass);

  object_class->constructed = ide_tweaks_constructed;
  object_class->dispose = ide_tweaks_dispose;
  object_class->get_property = ide_tweaks_get_property;
  object_class->set_property = ide_tweaks_set_property;

  item_class->accepts = ide_tweaks_accepts;

  properties[PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_PROJECT_ID] =
    g_param_spec_string ("project-id", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_init (IdeTweaks *self)
{
  self->scope = g_object_new (GTK_TYPE_BUILDER_CSCOPE, NULL);
  self->builder = gtk_builder_new ();
  gtk_builder_set_current_object (self->builder, G_OBJECT (self));
  gtk_builder_set_scope (self->builder, self->scope);

  gtk_builder_cscope_add_callback_symbol (GTK_BUILDER_CSCOPE (self->scope),
                                          "format",
                                          G_CALLBACK (ide_tweaks_format));
}

IdeTweaks *
ide_tweaks_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS, NULL);
}

gboolean
ide_tweaks_load_from_file (IdeTweaks     *self,
                           GFile         *file,
                           GCancellable  *cancellable,
                           GError       **error)
{
  g_autofree char *contents = NULL;
  gsize length;

  g_return_val_if_fail (IDE_IS_TWEAKS (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (!g_file_load_contents (file, cancellable, &contents, &length, NULL, error))
    return FALSE;

  return gtk_builder_extend_with_template (self->builder,
                                           G_OBJECT (self), G_OBJECT_TYPE (self),
                                           contents, length,
                                           error);
}

void
ide_tweaks_expose_object (IdeTweaks  *self,
                          const char *name,
                          GObject    *object)
{
  g_return_if_fail (IDE_IS_TWEAKS (self));
  g_return_if_fail (name != NULL);
  g_return_if_fail (G_IS_OBJECT (object));

  gtk_builder_expose_object (self->builder, name, object);
}

/**
 * ide_tweaks_add_callback:
 * @self: a #IdeTweaks
 * @name: the name of the callback
 * @callback: (scope forever): the callback represented by @name
 *
 * Adds @callback to the scope used when expanding future templates
 * from @self.
 */
void
ide_tweaks_add_callback (IdeTweaks  *self,
                         const char *name,
                         GCallback   callback)
{
  g_return_if_fail (IDE_IS_TWEAKS (self));
  g_return_if_fail (GTK_IS_BUILDER_CSCOPE (self->scope));
  g_return_if_fail (name != NULL);
  g_return_if_fail (callback != NULL);

  gtk_builder_cscope_add_callback_symbol (GTK_BUILDER_CSCOPE (self->scope), name, callback);
}

const char *
ide_tweaks_get_project_id (IdeTweaks *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS (self), NULL);

  return self->project_id;
}

void
ide_tweaks_set_project_id (IdeTweaks  *self,
                           const char *project_id)
{
  g_return_if_fail (IDE_IS_TWEAKS (self));

  if (g_set_str (&self->project_id, project_id))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROJECT_ID]);
}

/**
 * ide_tweaks_get_object:
 * @self: a #IdeTweaks
 *
 * Gets the object with @name.
 *
 * Returns: (transfer none) (nullable): a #GObject or %NULL
 */
GObject *
ide_tweaks_get_object (IdeTweaks  *self,
                       const char *name)
{
  g_return_val_if_fail (IDE_IS_TWEAKS (self), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return gtk_builder_get_object (self->builder, name);
}

/**
 * ide_tweaks_new_for_context:
 * @context: (nullable): an #IdeContext or %NULL
 *
 * Creates a new #IdeTweaks for @context.
 *
 * If @context is %NULL, this function acts the same as ide_tweaks_new().
 *
 * If @context is non-%NULL, this function will expose @context as
 * "IdeContext" to the templates as well as with the #IdeTweaks:context
 * property to make property bindings easier with lookup.
 *
 * #IdeTweaks:project-id is also set when @context is non-%NULL.
 *
 * Returns: (transfer full): a new #IdeTweaks
 */
IdeTweaks *
ide_tweaks_new_for_context (IdeContext *context)
{
  g_return_val_if_fail (!context || IDE_IS_CONTEXT (context), NULL);

  return g_object_new (IDE_TYPE_TWEAKS,
                       "context", context,
                       NULL);
}

/**
 * ide_tweaks_get_context:
 * @self: a #IdeTweaks
 *
 * Gets the #IdeContext if any.
 *
 * Returns: (nullable) (transfer none): an #IdeContext or %NULL
 */
IdeContext *
ide_tweaks_get_context (IdeTweaks *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS (self), NULL);

  return self->context;
}

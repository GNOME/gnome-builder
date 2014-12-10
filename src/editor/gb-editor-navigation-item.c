/* gb-editor-navigation-item.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "editor-navigation"

#include <glib/gi18n.h>

#include "gb-editor-navigation-item.h"
#include "gb-log.h"
#include "gb-workbench.h"

struct _GbEditorNavigationItemPrivate
{
  GFile       *file;
  guint        line;
  guint        line_offset;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbEditorNavigationItem, gb_editor_navigation_item,
                            GB_TYPE_NAVIGATION_ITEM)

enum {
  PROP_0,
  PROP_FILE,
  PROP_LINE,
  PROP_LINE_OFFSET,
  PROP_TAB,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GbNavigationItem *
gb_editor_navigation_item_new (GFile *file,
                               guint  line,
                               guint  line_offset)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (GB_TYPE_EDITOR_NAVIGATION_ITEM,
                       "file", file,
                       "line", line,
                       "line-offset", line_offset,
                       NULL);
}

GFile *
gb_editor_navigation_item_get_file (GbEditorNavigationItem *item)
{
  g_return_val_if_fail (GB_IS_EDITOR_NAVIGATION_ITEM (item), NULL);

  return item->priv->file;
}

static void
gb_editor_navigation_item_set_file (GbEditorNavigationItem *item,
                                    GFile                  *file)
{
  g_return_if_fail (GB_IS_EDITOR_NAVIGATION_ITEM (item));
  g_return_if_fail (!file || G_IS_FILE (file));

  if (file == item->priv->file)
    return;

  g_clear_object (&item->priv->file);
  item->priv->file = file ? g_object_ref (file) : NULL;
  g_object_notify_by_pspec (G_OBJECT (item), gParamSpecs [PROP_FILE]);
}

guint
gb_editor_navigation_item_get_line (GbEditorNavigationItem *item)
{
  g_return_val_if_fail (GB_IS_EDITOR_NAVIGATION_ITEM (item), 0);

  return item->priv->line;
}

static void
gb_editor_navigation_item_set_line (GbEditorNavigationItem *item,
                                    guint                   line)
{
  g_return_if_fail (GB_IS_EDITOR_NAVIGATION_ITEM (item));

  item->priv->line = line;
  g_object_notify_by_pspec (G_OBJECT (item), gParamSpecs [PROP_LINE]);
}

guint
gb_editor_navigation_item_get_line_offset (GbEditorNavigationItem *item)
{
  g_return_val_if_fail (GB_IS_EDITOR_NAVIGATION_ITEM (item), 0);

  return item->priv->line_offset;
}

static void
gb_editor_navigation_item_set_line_offset (GbEditorNavigationItem *item,
                                           guint                   line_offset)
{
  g_return_if_fail (GB_IS_EDITOR_NAVIGATION_ITEM (item));

  item->priv->line_offset = line_offset;
  g_object_notify_by_pspec (G_OBJECT (item), gParamSpecs [PROP_LINE_OFFSET]);
}

static void
gb_editor_navigation_item_activate (GbNavigationItem *item)
{
  GbEditorNavigationItem *self = (GbEditorNavigationItem *)item;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_NAVIGATION_ITEM (self));

  /*
   * TODO: Load document from document manager.
   *       Focus document.
   *       Restore line/column
   */

  EXIT;
}

static void
gb_editor_navigation_item_finalize (GObject *object)
{
  GbEditorNavigationItemPrivate *priv = GB_EDITOR_NAVIGATION_ITEM (object)->priv;

  g_clear_object (&priv->file);

  G_OBJECT_CLASS (gb_editor_navigation_item_parent_class)->finalize (object);
}

static void
gb_editor_navigation_item_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GbEditorNavigationItem *self = GB_EDITOR_NAVIGATION_ITEM (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, gb_editor_navigation_item_get_file (self));
      break;

    case PROP_LINE:
      g_value_set_uint (value, gb_editor_navigation_item_get_line (self));
      break;

    case PROP_LINE_OFFSET:
      g_value_set_uint (value, gb_editor_navigation_item_get_line_offset (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_navigation_item_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GbEditorNavigationItem *self = GB_EDITOR_NAVIGATION_ITEM (object);

  switch (prop_id)
    {
    case PROP_FILE:
      gb_editor_navigation_item_set_file (self, g_value_get_object (value));
      break;

    case PROP_LINE:
      gb_editor_navigation_item_set_line (self, g_value_get_uint (value));
      break;

    case PROP_LINE_OFFSET:
      gb_editor_navigation_item_set_line_offset (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_navigation_item_class_init (GbEditorNavigationItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GbNavigationItemClass *item_class = GB_NAVIGATION_ITEM_CLASS (klass);

  object_class->finalize = gb_editor_navigation_item_finalize;
  object_class->get_property = gb_editor_navigation_item_get_property;
  object_class->set_property = gb_editor_navigation_item_set_property;

  item_class->activate = gb_editor_navigation_item_activate;

  gParamSpecs [PROP_FILE] =
    g_param_spec_object ("file",
                         _("File"),
                         _("The file that is being edited."),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FILE,
                                   gParamSpecs [PROP_FILE]);

  gParamSpecs [PROP_LINE] =
    g_param_spec_uint ("line",
                       _("Line"),
                       _("The line number within the file."),
                       0,
                       G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_LINE,
                                   gParamSpecs [PROP_LINE]);

  gParamSpecs [PROP_LINE_OFFSET] =
    g_param_spec_uint ("line-offset",
                       _("Line Offset"),
                       _("The offset within the line."),
                       0,
                       G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_LINE_OFFSET,
                                   gParamSpecs [PROP_LINE_OFFSET]);
}

static void
gb_editor_navigation_item_init (GbEditorNavigationItem *self)
{
  self->priv = gb_editor_navigation_item_get_instance_private (self);
}

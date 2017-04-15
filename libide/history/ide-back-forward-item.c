/* ide-back-forward-item.c
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

#define G_LOG_DOMAIN "ide-back-forward-item"

#include <glib/gi18n.h>

#include "diagnostics/ide-source-location.h"
#include "files/ide-file.h"
#include "history/ide-back-forward-item.h"
#include "ide-macros.h"

#define NUM_LINES_CHAIN_MAX 5

struct _IdeBackForwardItem
{
  IdeObject  parent_instance;
  IdeUri    *uri;
  GtkTextMark *mark;
};

G_DEFINE_TYPE (IdeBackForwardItem, ide_back_forward_item, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_URI,
  PROP_MARK,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

IdeBackForwardItem *
ide_back_forward_item_new (IdeContext  *context,
			   IdeUri      *uri,
			   GtkTextMark *mark)
{
  return g_object_new (IDE_TYPE_BACK_FORWARD_ITEM,
                       "context", context,
                       "uri", uri,
		       "mark", mark,
                       NULL);
}

IdeUri *
ide_back_forward_item_get_uri (IdeBackForwardItem *self)
{
  g_return_val_if_fail (IDE_IS_BACK_FORWARD_ITEM (self), NULL);

  return self->uri;
}

static void
ide_back_forward_item_set_uri (IdeBackForwardItem *self,
                               IdeUri             *uri)
{
  g_return_if_fail (IDE_IS_BACK_FORWARD_ITEM (self));
  g_return_if_fail (uri != NULL);

  if (uri != self->uri)
    {
      g_clear_pointer (&self->uri, ide_uri_unref);
      self->uri = ide_uri_ref (uri);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_URI]);
    }
}

static void
ide_back_forward_item_set_mark (IdeBackForwardItem *self,
                                GtkTextMark        *mark)
{
  g_return_if_fail (IDE_IS_BACK_FORWARD_ITEM (self));

  /* Mark can be NULL as in the case of loading on startup */
  if (ide_set_weak_pointer (&self->mark, mark))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MARK]);
}

GtkTextMark *
ide_back_forward_item_get_mark (IdeBackForwardItem *self)
{
  g_return_val_if_fail (IDE_IS_BACK_FORWARD_ITEM (self), NULL);

  return self->mark;
}


static void
ide_back_forward_item_finalize (GObject *object)
{
  IdeBackForwardItem *self = (IdeBackForwardItem *)object;

  g_clear_pointer (&self->uri, ide_uri_unref);
  ide_clear_weak_pointer (&self->mark);

  G_OBJECT_CLASS (ide_back_forward_item_parent_class)->finalize (object);
}

static void
ide_back_forward_item_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeBackForwardItem *self = IDE_BACK_FORWARD_ITEM (object);

  switch (prop_id)
    {
    case PROP_URI:
      g_value_set_boxed (value, ide_back_forward_item_get_uri (self));
      break;

    case PROP_MARK:
      g_value_set_object (value, ide_back_forward_item_get_mark (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_back_forward_item_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeBackForwardItem *self = IDE_BACK_FORWARD_ITEM (object);

  switch (prop_id)
    {
    case PROP_URI:
      ide_back_forward_item_set_uri (self, g_value_get_boxed (value));
      break;

    case PROP_MARK:
      ide_back_forward_item_set_mark (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_back_forward_item_class_init (IdeBackForwardItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_back_forward_item_finalize;
  object_class->get_property = ide_back_forward_item_get_property;
  object_class->set_property = ide_back_forward_item_set_property;

  /**
   * IdeBackForwardItem:uri:
   *
   * The #IdeBackForwardItem:uri property contains the location for the
   * back/forward item.
   *
   * This might be a uri to a file, including a line number.
   *
   * #IdeWorkbenchAddin can hook how these are loaded, by implementing the
   * IdeWorkbenchAddin::can_open() vfunc and associated functions.
   */
  properties [PROP_URI] =
    g_param_spec_boxed ("uri",
                        "Uri",
                        "The location of the navigation item.",
                        IDE_TYPE_URI,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));

  properties [PROP_MARK] =
  g_param_spec_object ("mark",
                       "Mark",
                       "The GtkTextMark for the location",
                       GTK_TYPE_TEXT_MARK,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS |
			G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_back_forward_item_init (IdeBackForwardItem *self)
{
}

gboolean
ide_back_forward_item_chain (IdeBackForwardItem *self,
                             IdeBackForwardItem *other)
{
  const gchar *tmp1;
  const gchar *tmp2;
  gint line1 = 0;
  gint line2 = 0;

  g_return_val_if_fail (IDE_IS_BACK_FORWARD_ITEM (self), FALSE);
  g_return_val_if_fail (IDE_IS_BACK_FORWARD_ITEM (other), FALSE);

  tmp1 = ide_uri_get_scheme (self->uri);
  tmp2 = ide_uri_get_scheme (other->uri);
  if (!ide_str_equal0 (tmp1, tmp2))
    return FALSE;

  tmp1 = ide_uri_get_host (self->uri);
  tmp2 = ide_uri_get_host (other->uri);
  if (!ide_str_equal0 (tmp1, tmp2))
    return FALSE;

  tmp1 = ide_uri_get_path (self->uri);
  tmp2 = ide_uri_get_path (other->uri);
  if (!ide_str_equal0 (tmp1, tmp2))
    return FALSE;

  tmp1 = ide_uri_get_fragment (self->uri);
  tmp2 = ide_uri_get_fragment (other->uri);
  if ((tmp1 == NULL) || (tmp2 == NULL))
    return FALSE;

  if ((1 != sscanf (tmp1, "L%u_", &line1)) ||
      (1 != sscanf (tmp2, "L%u_", &line2)))
    return FALSE;

  if (line1 >= G_MAXINT || line2 >= G_MAXINT)
    return FALSE;

  if (ABS (line1 - line2) < 10)
    return TRUE;

  return FALSE;
}

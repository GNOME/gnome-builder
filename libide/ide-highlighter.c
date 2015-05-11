/* ide-highlighter.c
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
#include <glib/gi18n.h>

#include "ide-highlighter.h"
#include "ide-internal.h"

typedef struct
{
  IdeHighlightEngine *engine;
} IdeHighlighterPrivate;

enum {
  PROP_0,
  PROP_HIGHLIGHT_ENGINE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeHighlighter, ide_highlighter, IDE_TYPE_OBJECT)

/**
 * ide_highlighter_get_highlight_engine:
 * @self: A #IdeHighlighter.
 *
 * Gets the IdeHighlightEngine property.
 *
 * Returns: (transfer none): An #IdeHighlightEngine.
 */
IdeHighlightEngine *
ide_highlighter_get_highlight_engine (IdeHighlighter *self)
{
  IdeHighlighterPrivate *priv = ide_highlighter_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_HIGHLIGHTER (self), NULL);

  return priv->engine;
}


static void
ide_highlighter_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeHighlighter *self = IDE_HIGHLIGHTER (object);

  switch (prop_id)
    {
    case PROP_HIGHLIGHT_ENGINE:
      g_value_set_object (value, ide_highlighter_get_highlight_engine (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_highlighter_dispose (GObject *object)
{
  IdeHighlighter *self = (IdeHighlighter *)object;
  IdeHighlighterPrivate *priv = ide_highlighter_get_instance_private (self);

  ide_clear_weak_pointer (&priv->engine);

  G_OBJECT_CLASS (ide_highlighter_parent_class)->dispose (object);
}

static void
ide_highlighter_class_init (IdeHighlighterClass *klass)
{

  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_highlighter_dispose;
  object_class->get_property = ide_highlighter_get_property;

  gParamSpecs [PROP_HIGHLIGHT_ENGINE] =
    g_param_spec_object ("highlight-engine",
                         _("Highlight engine"),
                         _("The highlight engine of this highlighter."),
                         IDE_TYPE_HIGHLIGHT_ENGINE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
ide_highlighter_init (IdeHighlighter *self)
{
}

void
_ide_highlighter_set_highlighter_engine (IdeHighlighter     *self,
                                         IdeHighlightEngine *engine)
{
  IdeHighlighterPrivate *priv = ide_highlighter_get_instance_private (self);

  g_return_if_fail (IDE_IS_HIGHLIGHTER (self));
  g_return_if_fail (IDE_IS_HIGHLIGHT_ENGINE (engine));

  if (ide_set_weak_pointer (&priv->engine, engine))
    g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_HIGHLIGHT_ENGINE]);
}

/**
 * ide_highlighter_update:
 * @self: A #IdeHighlighter.
 * @callback: (scope call): A callback to apply a given style.
 * @range_begin: The beginning of the range to update.
 * @range_end: The end of the range to update.
 * @location: (out): How far the highlighter got in the update.
 *
 * Incrementally processes more of the buffer for highlighting.  If @callback
 * returns %IDE_HIGHLIGHT_STOP, then this vfunc should stop processing and
 * return, having set @location to the current position of processing.
 *
 * If processing the entire range was successful, then @location should be set
 * to @range_end.
 */
void
ide_highlighter_update (IdeHighlighter       *self,
                        IdeHighlightCallback  callback,
                        const GtkTextIter    *range_begin,
                        const GtkTextIter    *range_end,
                        GtkTextIter          *location)
{
  g_return_if_fail (IDE_IS_HIGHLIGHTER (self));
  g_return_if_fail (range_begin);
  g_return_if_fail (range_end);

  if (IDE_HIGHLIGHTER_GET_CLASS (self)->update)
    IDE_HIGHLIGHTER_GET_CLASS (self)->update (self, callback, range_begin, range_end, location);
}

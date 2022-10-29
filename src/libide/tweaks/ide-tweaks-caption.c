/* ide-tweaks-caption.c
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

#define G_LOG_DOMAIN "ide-tweaks-caption"

#include "config.h"

#include "ide-tweaks-caption.h"
#include "ide-tweaks-item-private.h"

struct _IdeTweaksCaption
{
  IdeTweaksWidget parent_instance;
  char *text;
};

enum {
  PROP_0,
  PROP_TEXT,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksCaption, ide_tweaks_caption, IDE_TYPE_TWEAKS_WIDGET)

static GParamSpec *properties [N_PROPS];

static GtkWidget *
ide_tweaks_caption_create_for_item (IdeTweaksWidget *widget,
                                    IdeTweaksItem   *item)
{
  int margin_top = 0;

  g_assert (IDE_IS_TWEAKS_CAPTION (widget));
  g_assert (IDE_IS_TWEAKS_CAPTION (item));

  for (IdeTweaksItem *iter = ide_tweaks_item_get_previous_sibling (item);
       iter;
       iter = ide_tweaks_item_get_previous_sibling (iter))
    {
      if (!IDE_IS_TWEAKS_WIDGET (iter) || _ide_tweaks_item_is_hidden (iter, NULL))
        continue;

      margin_top = 12;
      break;
    }

  return g_object_new (GTK_TYPE_LABEL,
                       "css-classes", IDE_STRV_INIT ("caption", "dim-label"),
                       "label", IDE_TWEAKS_CAPTION (item)->text,
                       "margin-top", margin_top,
                       "use-markup", TRUE,
                       "xalign", .0f,
                       "wrap", TRUE,
                       NULL);
}

static void
ide_tweaks_caption_dispose (GObject *object)
{
  IdeTweaksCaption *self = (IdeTweaksCaption *)object;

  g_clear_pointer (&self->text, g_free);

  G_OBJECT_CLASS (ide_tweaks_caption_parent_class)->dispose (object);
}

static void
ide_tweaks_caption_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeTweaksCaption *self = IDE_TWEAKS_CAPTION (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, ide_tweaks_caption_get_text (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_caption_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeTweaksCaption *self = IDE_TWEAKS_CAPTION (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      ide_tweaks_caption_set_text (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_caption_class_init (IdeTweaksCaptionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTweaksWidgetClass *widget_class = IDE_TWEAKS_WIDGET_CLASS (klass);

  object_class->dispose = ide_tweaks_caption_dispose;
  object_class->get_property = ide_tweaks_caption_get_property;
  object_class->set_property = ide_tweaks_caption_set_property;

  widget_class->create_for_item = ide_tweaks_caption_create_for_item;

  properties [PROP_TEXT] =
    g_param_spec_string ("text", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY |G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_caption_init (IdeTweaksCaption *self)
{
}

IdeTweaksCaption *
ide_tweaks_caption_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS_CAPTION, NULL);
}

const char *
ide_tweaks_caption_get_text (IdeTweaksCaption *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_CAPTION (self), NULL);

  return self->text;
}

void
ide_tweaks_caption_set_text (IdeTweaksCaption *self,
                             const char       *text)
{
  g_return_if_fail (IDE_IS_TWEAKS_CAPTION (self));

  if (g_set_str (&self->text, text))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TEXT]);
}

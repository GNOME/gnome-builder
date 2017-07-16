/* gbp-spell-buffer-addin.c
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

#define G_LOG_DOMAIN "gbp-spell-buffer-addin"

#include "gbp-spell-buffer-addin.h"

struct _GbpSpellBufferAddin
{
  GObject parent_instance;

  /* Unowned reference to buffer */
  IdeBuffer *buffer;

  /* Owned spellchecker instance */
  GspellChecker *spellchecker;

  guint enabled : 1;
};

enum {
  PROP_0,
  PROP_ENABLED,
  N_PROPS
};

static void
gbp_spell_buffer_addin_apply (GbpSpellBufferAddin *self)
{
  GspellTextBuffer *spell_buffer;

  IDE_ENTRY;

  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));

  if (self->enabled == FALSE || self->buffer == NULL)
    {
      g_clear_object (&self->spellchecker);
      return;
    }

  /* The returned GspellTextBuffer is owned by self->buffer */
  spell_buffer = gspell_text_buffer_get_from_gtk_text_buffer (GTK_TEXT_BUFFER (self->buffer));
  g_assert (GSPELL_IS_TEXT_BUFFER (spell_buffer));

  /* Setup the spell checker for the buffer. We retrain the spellchecker
   * instance so that we can add words/modify the dictionary at runtime.
   */
  self->spellchecker = gspell_checker_new (NULL);
  gspell_text_buffer_set_spell_checker (spell_buffer, self->spellchecker);

  IDE_EXIT;
}

static void
gbp_spell_buffer_addin_load (IdeBufferAddin *addin,
                             IdeBuffer      *buffer)
{
  GbpSpellBufferAddin *self = (GbpSpellBufferAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  self->buffer = buffer;

  gbp_spell_buffer_addin_apply (self);

  IDE_EXIT;
}

static void
gbp_spell_buffer_addin_unload (IdeBufferAddin *addin,
                               IdeBuffer      *buffer)
{
  GbpSpellBufferAddin *self = (GbpSpellBufferAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  self->buffer = NULL;

  gbp_spell_buffer_addin_apply (self);

  IDE_EXIT;
}

static void
buffer_addin_iface_init (IdeBufferAddinInterface *iface)
{
  iface->load = gbp_spell_buffer_addin_load;
  iface->unload = gbp_spell_buffer_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpSpellBufferAddin, gbp_spell_buffer_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUFFER_ADDIN, buffer_addin_iface_init))

static GParamSpec *properties [N_PROPS];

static void
gbp_spell_buffer_addin_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GbpSpellBufferAddin *self = GBP_SPELL_BUFFER_ADDIN (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, self->enabled);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_spell_buffer_addin_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GbpSpellBufferAddin *self = GBP_SPELL_BUFFER_ADDIN (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      self->enabled = g_value_get_boolean (value);
      gbp_spell_buffer_addin_apply (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_spell_buffer_addin_class_init (GbpSpellBufferAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gbp_spell_buffer_addin_get_property;
  object_class->set_property = gbp_spell_buffer_addin_set_property;

  properties [PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
                          "Enabled",
                          "If the spellchecker is enabled",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_spell_buffer_addin_init (GbpSpellBufferAddin *self)
{
}

/**
 * gbp_spell_buffer_addin_get_checker:
 * @self: a #GbpSpellBufferAddin
 *
 * Gets the #GspellChecker used by the underlying buffer, or %NULL if
 * no spellchecker is active.
 *
 * Returns: (transfer none): A #GspellChecker
 *
 * Since: 3.26
 */
GspellChecker *
gbp_spell_buffer_addin_get_checker (GbpSpellBufferAddin *self)
{
  g_return_val_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self), NULL);

  return self->spellchecker;
}

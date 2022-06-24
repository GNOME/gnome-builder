/* editor-spell-menu.c
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <glib/gi18n.h>

#include "editor-spell-language-info.h"
#include "editor-spell-menu.h"
#include "editor-spell-provider.h"

#define MAX_CORRECTIONS 5

#define EDITOR_TYPE_SPELL_CORRECTIONS (editor_spell_corrections_get_type())
G_DECLARE_FINAL_TYPE (EditorSpellCorrections, editor_spell_corrections, EDITOR, SPELL_CORRECTIONS, GMenuModel)

struct _EditorSpellCorrections
{
  GMenuModel parent_instance;
  char *word;
  char **corrections;
};

G_DEFINE_TYPE (EditorSpellCorrections, editor_spell_corrections, G_TYPE_MENU_MODEL)

static int
editor_spell_corrections_get_n_items (GMenuModel *model)
{
  EditorSpellCorrections *self = EDITOR_SPELL_CORRECTIONS (model);
  return self->corrections ? g_strv_length (self->corrections) : 0;
}

static gboolean
editor_spell_corrections_is_mutable (GMenuModel *model)
{
  return TRUE;
}

static GMenuModel *
editor_spell_corrections_get_item_link (GMenuModel *model,
                                        int         position,
                                        const char *link)
{
  return NULL;
}

static void
editor_spell_corrections_get_item_links (GMenuModel  *model,
                                         int          position,
                                         GHashTable **links)
{
  *links = NULL;
}

static void
editor_spell_corrections_get_item_attributes (GMenuModel  *model,
                                              int          position,
                                              GHashTable **attributes)
{
  EditorSpellCorrections *self = EDITOR_SPELL_CORRECTIONS (model);
  const char *correction;
  GHashTable *ht;

  *attributes = NULL;

  if (position < 0 ||
      self->corrections == NULL ||
      position >= g_strv_length (self->corrections))
    return;

  correction = self->corrections[position];

  ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_variant_unref);
  g_hash_table_insert (ht, g_strdup (G_MENU_ATTRIBUTE_ACTION), g_variant_ref_sink (g_variant_new_string ("spelling.correct")));
  g_hash_table_insert (ht, g_strdup (G_MENU_ATTRIBUTE_TARGET), g_variant_ref_sink (g_variant_new_string (correction)));
  g_hash_table_insert (ht, g_strdup (G_MENU_ATTRIBUTE_LABEL), g_variant_ref_sink (g_variant_new_string (correction)));

  *attributes = ht;
}

static void
editor_spell_menu_dispose (GObject *object)
{
  EditorSpellCorrections *self = (EditorSpellCorrections *)object;

  g_clear_pointer (&self->word, g_free);
  g_clear_pointer (&self->corrections, g_strfreev);

  G_OBJECT_CLASS (editor_spell_corrections_parent_class)->dispose (object);
}

static void
editor_spell_corrections_class_init (EditorSpellCorrectionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GMenuModelClass *menu_model_class = G_MENU_MODEL_CLASS (klass);

  object_class->dispose = editor_spell_menu_dispose;

  menu_model_class->get_n_items = editor_spell_corrections_get_n_items;
  menu_model_class->is_mutable = editor_spell_corrections_is_mutable;
  menu_model_class->get_item_link = editor_spell_corrections_get_item_link;
  menu_model_class->get_item_links = editor_spell_corrections_get_item_links;
  menu_model_class->get_item_attributes = editor_spell_corrections_get_item_attributes;
}

static void
editor_spell_corrections_init (EditorSpellCorrections *self)
{
}

static void
editor_spell_corrections_set (EditorSpellCorrections *self,
                              const char             *word,
                              const char * const     *corrections)
{
  guint removed = 0;
  guint added = 0;

  g_assert (EDITOR_IS_SPELL_CORRECTIONS (self));

  if (corrections == (const char * const *)self->corrections)
    return;

  if (g_strcmp0 (word, self->word) == 0)
    return;

  if (self->corrections != NULL)
    removed = g_strv_length (self->corrections);

  if (corrections != NULL)
    added = g_strv_length ((char **)corrections);

  g_free (self->word);
  self->word = g_strdup (word);
  g_strfreev (self->corrections);
  self->corrections = g_strdupv ((char **)corrections);
  g_menu_model_items_changed (G_MENU_MODEL (self), 0, removed, added);
}

static GMenuModel *
editor_spell_corrections_new (void)
{
  return g_object_new (EDITOR_TYPE_SPELL_CORRECTIONS, NULL);
}

static int
count_groups (GPtrArray *infos)
{
  g_autoptr(GHashTable) groups = g_hash_table_new (g_str_hash, g_str_equal);

  g_assert (infos != NULL);

  for (guint i = 0; i < infos->len; i++)
    {
      EditorSpellLanguageInfo *info = g_ptr_array_index (infos, i);
      const char *group = editor_spell_language_info_get_group (info);

      if (group != NULL && group[0] != 0 && !g_hash_table_contains (groups, group))
        g_hash_table_insert (groups, (char *)group, NULL);
    }

  return g_hash_table_size (groups);
}

static void
populate_languages (GMenu *menu)
{
  EditorSpellProvider *provider = editor_spell_provider_get_default ();
  g_autoptr(GPtrArray) infos = editor_spell_provider_list_languages (provider);
  g_autoptr(GHashTable) groups = NULL;

  if (infos == NULL)
    return;

  groups = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  /* First setup our groups. We do that up front so we can avoid
   * checking below, but also so we can hoist a single group up
   * into the parent menu if necessary.
   */
  if (count_groups (infos) > 1)
    {
      for (guint i = 0; i < infos->len; i++)
        {
          EditorSpellLanguageInfo *info = g_ptr_array_index (infos, i);
          const char *group = editor_spell_language_info_get_group (info);
          GMenu *group_menu;

          if (group == NULL || group[0] == 0)
            continue;

          if (!g_hash_table_contains (groups, group))
            {
              group_menu = g_menu_new ();
              g_menu_append_submenu (menu, group, G_MENU_MODEL (group_menu));
              g_hash_table_insert (groups,
                                   g_strdup (group),
                                   g_steal_pointer (&group_menu));
            }
        }
    }

  for (guint i = 0; i < infos->len; i++)
    {
      EditorSpellLanguageInfo *info = g_ptr_array_index (infos, i);
      const char *name = editor_spell_language_info_get_name (info);
      const char *group = editor_spell_language_info_get_group (info);
      const char *code = editor_spell_language_info_get_code (info);
      g_autoptr(GMenuItem) item = NULL;
      GMenu *group_menu;

      if (group == NULL || !(group_menu = g_hash_table_lookup (groups, group)))
        group_menu = menu;

      g_assert (G_IS_MENU (group_menu));

      item = g_menu_item_new (name, NULL);
      g_menu_item_set_action_and_target (item, "spelling.language", "s", code);
      g_menu_append_item (group_menu, item);
    }
}

GMenuModel *
editor_spell_menu_new (void)
{
  static GMenu *languages_menu;
  static GMenuItem *languages_item;
  g_autoptr(GMenu) menu = g_menu_new ();
  g_autoptr(GMenuModel) corrections_menu = editor_spell_corrections_new ();
  g_autoptr(GMenuItem) add_item = g_menu_item_new (_("Add to Dictionary"), "spelling.add");
  g_autoptr(GMenuItem) ignore_item = g_menu_item_new (_("Ignore"), "spelling.ignore");
  g_autoptr(GMenuItem) check_item = g_menu_item_new (_("Check Spelling"), "spelling.enabled");

  if (languages_menu == NULL)
    {
      languages_menu = g_menu_new ();
      populate_languages (languages_menu);
    }

  if (languages_item == NULL)
    languages_item = g_menu_item_new_submenu (_("Languages"), G_MENU_MODEL (languages_menu));

  g_menu_item_set_attribute (add_item, "hidden-when", "s", "action-disabled");
  g_menu_item_set_attribute (ignore_item, "hidden-when", "s", "action-disabled");
  g_menu_item_set_attribute (check_item, "role", "s", "check");
  g_menu_item_set_attribute (languages_item, "submenu-action", "s", "spellcheck.enabled");

  g_menu_append_section (menu, NULL, G_MENU_MODEL (corrections_menu));
  g_menu_append_item (menu, add_item);
  g_menu_append_item (menu, ignore_item);
  g_menu_append_item (menu, check_item);
  g_menu_append_item (menu, languages_item);

  g_object_set_data_full (G_OBJECT (menu),
                          "CORRECTIONS_MENU",
                          g_object_ref (corrections_menu),
                          g_object_unref);

  return G_MENU_MODEL (g_steal_pointer (&menu));
}

void
editor_spell_menu_set_corrections (GMenuModel         *menu,
                                   const char         *word,
                                   const char * const *words)
{
  EditorSpellCorrections *corrections_menu;

  g_return_if_fail (G_IS_MENU_MODEL (menu));

  if ((corrections_menu = g_object_get_data (G_OBJECT (menu), "CORRECTIONS_MENU")))
    {
      g_assert (EDITOR_IS_SPELL_CORRECTIONS (corrections_menu));
      editor_spell_corrections_set (corrections_menu, word, words);
    }
}

/* ide-content-type.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-glib"

#include "config.h"

#include "../../gconstructor.h"

#include "ide-content-type.h"

static gchar bundled_lookup_table[256];
static GIcon *x_zerosize_icon;
static GHashTable *bundled_by_content_type;
static GHashTable *bundled_by_full_filename;
/* This ensures those files get a proper icon when they end with .md
 * (markdown files).  It can't be fixed in the shared-mime-info db because
 * otherwise they wouldn't get detected as markdown anymore.
 */
static const struct {
  const gchar *searched_prefix;
  const gchar *icon_name;
} bundled_check_by_name_prefix[] = {
  { "README", "text-x-readme-symbolic" },
  { "NEWS", "text-x-changelog-symbolic" },
  { "CHANGELOG", "text-x-changelog-symbolic" },
  { "COPYING", "text-x-copying-symbolic" },
  { "LICENSE", "text-x-copying-symbolic" },
  { "AUTHORS", "text-x-authors-symbolic" },
  { "MAINTAINERS", "text-x-authors-symbolic" },
  { "Dockerfile", "text-makefile-symbolic" },
  { "Containerfile", "text-makefile-symbolic" },
  { "package.json", "text-makefile-symbolic" },
  { "pom.xml", "text-makefile-symbolic" },
  { "build.gradle", "text-makefile-symbolic" },
  { "Cargo.toml", "text-makefile-symbolic" },
  { "pyproject.toml", "text-makefile-symbolic" },
  { "requirements.txt", "text-makefile-symbolic" },
  { "go.mod", "text-makefile-symbolic" },
  { "wscript", "text-makefile-symbolic" },
  { "sketch.yaml", "text-makefile-symbolic" },
  { "sketch.yml", "text-makefile-symbolic" },
};

static const struct {
  const char *suffix;
  const char *content_type;
} suffix_content_type_overrides[] = {
  { ".md", "text-markdown-symbolic" },
  { ".swift", "text-swift-symbolic" },
  { ".ino", "text-arduino-symbolic" },
};

#if defined (G_HAS_CONSTRUCTORS)
# ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
#  pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(ide_io_init_ctor)
# endif
G_DEFINE_CONSTRUCTOR(ide_io_init_ctor)
#else
# error Your platform/compiler is missing constructor support
#endif

static void
ide_io_init_ctor (void)
{
  bundled_by_content_type = g_hash_table_new (g_str_hash, g_str_equal);
  bundled_by_full_filename = g_hash_table_new (g_str_hash, g_str_equal);

  /*
   * This needs to be updated when we add icons for specific mime-types
   * because of how icon theme loading works (and it wanting to use
   * Adwaita generic icons before our hicolor specific icons).
   */
#define ADD_ICON(t, n, v) g_hash_table_insert (t, (gpointer)n, v ? (gpointer)v : (gpointer)n)
  /* We don't get GThemedIcon fallbacks in an order that prioritizes some
   * applications over something more generic like text-x-script, so we need
   * to map the higher priority symbolic first.
   */
  ADD_ICON (bundled_by_content_type, "application-x-php-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "application-x-ruby-symbolic", "text-x-ruby-symbolic");
  ADD_ICON (bundled_by_content_type, "application-javascript-symbolic", "text-x-javascript-symbolic");
  ADD_ICON (bundled_by_content_type, "application-json-symbolic", "text-x-javascript-symbolic");
  ADD_ICON (bundled_by_content_type, "application-sql-symbolic", "text-sql-symbolic");

  ADD_ICON (bundled_by_content_type, "text-css-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-html-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-markdown-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-rust-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-sql-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-authors-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-blueprint-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-changelog-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-chdr-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-copying-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-c++src-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-csrc-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-go-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-javascript-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-python-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-python3-symbolic", "text-x-python-symbolic");
  ADD_ICON (bundled_by_content_type, "text-x-readme-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-ruby-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-script-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-vala-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-xml-symbolic", NULL);
  ADD_ICON (bundled_by_content_type, "text-x-meson", "text-makefile-symbolic");
  ADD_ICON (bundled_by_content_type, "text-x-cmake", "text-makefile-symbolic");
  ADD_ICON (bundled_by_content_type, "text-x-makefile", "text-makefile-symbolic");

  ADD_ICON (bundled_by_full_filename, ".editorconfig", "format-indent-more-symbolic");
  ADD_ICON (bundled_by_full_filename, ".gitignore", "builder-vcs-git-symbolic");
  ADD_ICON (bundled_by_full_filename, ".gitattributes", "builder-vcs-git-symbolic");
  ADD_ICON (bundled_by_full_filename, ".gitmodules", "builder-vcs-git-symbolic");
#undef ADD_ICON

  /* Create faster check than doing full string checks */
  for (guint i = 0; i < G_N_ELEMENTS (bundled_check_by_name_prefix); i++)
    bundled_lookup_table[(guint)bundled_check_by_name_prefix[i].searched_prefix[0]] = 1;

  x_zerosize_icon = g_themed_icon_new ("text-x-generic-symbolic");
}

/**
 * ide_g_content_type_get_symbolic_icon:
 * @content_type: the content-type to lookup
 *
 * This function is similar to g_content_type_get_symbolic_icon() except that
 * it takes our bundled icons into account to ensure that they are taken at a
 * higher priority than the fallbacks from the current icon theme such as
 * Adwaita.
 *
 * In 3.40, this function was modified to add the @filename parameter.
 *
 * Returns: (transfer full) (nullable): A #GIcon or %NULL
 */
GIcon *
ide_g_content_type_get_symbolic_icon (const gchar *content_type,
                                      const gchar *filename)
{
  g_autoptr(GIcon) icon = NULL;
  const char * const *names;
  const char *replacement_by_filename;
  const char *suffix;

  g_return_val_if_fail (content_type != NULL, NULL);

  /* Special case folders to never even try to use an overridden icon. For
   * example in the case of the LICENSES folder required by the REUSE licensing
   * helpers, the icon would be the copyright icon. Even if in this particular
   * case it might make sense to keep the copyright icon, it's just really
   * confusing to have a folder without a folder icon, especially since it becomes
   * an expanded folder icon when opening it in the project tree.
   */
  if (strcmp (content_type, "inode/directory") == 0)
    return g_content_type_get_symbolic_icon (content_type);
  else if (strcmp (content_type, "application/x-zerosize") == 0)
    return g_object_ref (x_zerosize_icon);

  /* Special case some weird content-types in the wild, particularly when Wine is
   * installed and taking over a content-type we would otherwise not expect.
   */
  if ((suffix = filename ? strrchr (filename, '.') : NULL))
    {
      for (guint i = 0; i < G_N_ELEMENTS (suffix_content_type_overrides); i++)
        {
          if (strcmp (suffix, suffix_content_type_overrides[i].suffix) == 0)
            {
              content_type = suffix_content_type_overrides[i].content_type;
              break;
            }
        }
    }

  icon = g_content_type_get_symbolic_icon (content_type);

  if (filename != NULL && bundled_lookup_table [(guint8)filename[0]])
    {
      for (guint j = 0; j < G_N_ELEMENTS (bundled_check_by_name_prefix); j++)
        {
          const gchar *searched_prefix = bundled_check_by_name_prefix[j].searched_prefix;

          /* Check prefix but ignore case, because there might be some files named e.g. ReadMe.txt */
          if (g_ascii_strncasecmp (filename, searched_prefix, strlen (searched_prefix)) == 0)
            return g_icon_new_for_string (bundled_check_by_name_prefix[j].icon_name, NULL);
        }
    }

  if (filename != NULL)
    {
      if ((replacement_by_filename = g_hash_table_lookup (bundled_by_full_filename, filename)))
        return g_icon_new_for_string (replacement_by_filename, NULL);
    }

  if (G_IS_THEMED_ICON (icon))
    {
      names = g_themed_icon_get_names (G_THEMED_ICON (icon));

      if (names != NULL)
        {
          gboolean fallback = FALSE;

          for (guint i = 0; names[i] != NULL; i++)
            {
              const gchar *replace = g_hash_table_lookup (bundled_by_content_type, names[i]);

              if (replace != NULL)
                return g_icon_new_for_string (replace, NULL);

              fallback |= (g_str_equal (names[i], "text-plain") ||
                           g_str_equal (names[i], "application-octet-stream"));
            }

          if (fallback)
            return g_icon_new_for_string ("text-x-generic-symbolic", NULL);
        }
    }

  return g_steal_pointer (&icon);
}


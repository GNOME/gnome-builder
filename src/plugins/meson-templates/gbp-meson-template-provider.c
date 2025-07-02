/* gbp-meson-template-provider.c
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

#define G_LOG_DOMAIN "gbp-meson-template-provider"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-projects.h>

#include "gbp-meson-template.h"
#include "gbp-meson-template-provider.h"

struct _GbpMesonTemplateProvider
{
  GObject parent_instance;
};

typedef struct _GbpMesonTemplateInfo
{
  int                                  priority;
  const char                          *id;
  const char                          *name;
  const char                          *description;
  const char * const                  *languages;
  const GbpMesonTemplateExpansion     *expansions;
  guint                                n_expansions;
  const GbpMesonTemplateLanguageScope *language_scope;
  guint                                n_language_scope;
  const char * const                  *extra_scope;
} GbpMesonTemplateInfo;

static GbpMesonTemplateExpansion gtk4_expansions[] = {
  { "meson.build",                                         "meson.build" },
  { "flatpak.json",                                        "{{appid}}.json" },
  { "README.md",                                           "README.md" },
  { "data/hello.desktop.in",                               "data/{{appid}}.desktop.in" },
  { "data/hello.metainfo.xml.in",                          "data/{{appid}}.metainfo.xml.in" },
  { "data/hello.service.in",                               "data/{{appid}}.service.in" },
  { "data/hello.gschema.xml",                              "data/{{appid}}.gschema.xml" },
  { "data/meson.build",                                    "data/meson.build" },
  { "data/icons/meson.build",                              "data/icons/meson.build" },
  { "data/icons/hicolor/scalable/apps/hello.svg",          "data/icons/hicolor/scalable/apps/{{appid}}.svg" },
  { "data/icons/hicolor/symbolic/apps/hello-symbolic.svg", "data/icons/hicolor/symbolic/apps/{{appid}}-symbolic.svg" },
  { "po/LINGUAS",                                          "po/LINGUAS" },
  { "po/meson.build",                                      "po/meson.build" },
  { "po/POTFILES.in",                                      "po/POTFILES.in" },
  { "src/shortcuts-file.ui",                               "src/{{shortcuts_path}}.ui" },

  /* C */
  { "src/application-gtk4.c", "src/{{prefix}}-application.c", IDE_STRV_INIT ("C") },
  { "src/application-gtk4.h", "src/{{prefix}}-application.h", IDE_STRV_INIT ("C") },
  { "src/hello.gresource.xml", "src/{{prefix}}.gresource.xml", IDE_STRV_INIT ("C") },
  { "src/main-gtk4.c", "src/main.c", IDE_STRV_INIT ("C") },
  { "src/meson-c-vala.build", "src/meson.build", IDE_STRV_INIT ("C") },
  { "src/window-gtk4.ui", "src/{{prefix}}-window.ui", IDE_STRV_INIT ("C") },
  { "src/window-gtk4.c", "src/{{prefix}}-window.c", IDE_STRV_INIT ("C") },
  { "src/window-gtk4.h", "src/{{prefix}}-window.h", IDE_STRV_INIT ("C") },

  /* JavaScript */
  { "src/hello.gresource.xml", "src/{{appid}}.data.gresource.xml", IDE_STRV_INIT ("JavaScript") },
  { "src/hello.js.in", "src/{{appid}}.in", IDE_STRV_INIT ("JavaScript"), TRUE },
  { "src/hello.src.gresource.xml", "src/{{appid}}.src.gresource.xml", IDE_STRV_INIT ("JavaScript") },
  { "src/main-gtk4.js.tmpl", "src/main.js", IDE_STRV_INIT ("JavaScript") },
  { "src/meson-js.build", "src/meson.build", IDE_STRV_INIT ("JavaScript") },
  { "src/window-gtk4.ui", "src/window.ui", IDE_STRV_INIT ("JavaScript") },
  { "src/window-gtk4.js", "src/window.js", IDE_STRV_INIT ("JavaScript") },

  /* Python */
  { "src/__init__.py", "src/__init__.py", IDE_STRV_INIT ("Python") },
  { "src/hello.gresource.xml", "src/{{prefix}}.gresource.xml", IDE_STRV_INIT ("Python") },
  { "src/hello.py.in", "src/{{name}}.in", IDE_STRV_INIT ("Python"), TRUE },
  { "src/main-gtk4.py", "src/main.py", IDE_STRV_INIT ("Python") },
  { "src/meson-py-gtk4.build", "src/meson.build", IDE_STRV_INIT ("Python") },
  { "src/window-gtk4.py", "src/window.py", IDE_STRV_INIT ("Python") },
  { "src/window-gtk4.ui", "src/window.ui", IDE_STRV_INIT ("Python") },

  /* Rust */
  { "src/Cargo-gtk4.toml", "Cargo.toml", IDE_STRV_INIT ("Rust") },
  { "src/application-gtk4.rs", "src/application.rs", IDE_STRV_INIT ("Rust") },
  { "src/config-gtk4.rs.in", "src/config.rs.in", IDE_STRV_INIT ("Rust") },
  { "src/hello.gresource.xml", "src/{{prefix}}.gresource.xml", IDE_STRV_INIT ("Rust") },
  { "src/main-gtk4.rs", "src/main.rs", IDE_STRV_INIT ("Rust") },
  { "src/meson-rs-gtk4.build", "src/meson.build", IDE_STRV_INIT ("Rust") },
  { "src/window-gtk4.rs", "src/window.rs", IDE_STRV_INIT ("Rust") },
  { "src/window-gtk4.ui", "src/window.ui", IDE_STRV_INIT ("Rust") },

  /* Vala */
  { "src/application-gtk4.vala", "src/application.vala", IDE_STRV_INIT ("Vala") },
  { "src/hello.gresource.xml", "src/{{prefix}}.gresource.xml", IDE_STRV_INIT ("Vala") },
  { "src/main-gtk4.vala", "src/main.vala", IDE_STRV_INIT ("Vala") },
  { "src/config.vapi", "src/config.vapi", IDE_STRV_INIT ("Vala") },
  { "src/meson-c-vala.build", "src/meson.build", IDE_STRV_INIT ("Vala") },
  { "src/window-gtk4.ui", "src/window.ui", IDE_STRV_INIT ("Vala") },
  { "src/window-gtk4.vala", "src/window.vala", IDE_STRV_INIT ("Vala") },
};

static const GbpMesonTemplateLanguageScope gtk4_language_scope[] = {
  { "C",          IDE_STRV_INIT ("ui_file={{prefix}}-window.ui") },
  { "JavaScript", IDE_STRV_INIT ("exec_name={{appid}}") },
};

static GbpMesonTemplateExpansion library_expansions[] = {
  { "meson.build", "meson.build" },
  { "README.md", "README.md" },
  { "src/meson-clib.build", "src/meson.build" },
  { "src/hello.c", "src/{{name}}.c" },
  { "src/hello.h", "src/{{name}}.h" },
  { "src/hello-version.h.in", "src/{{name}}-version.h.in" },
};

static GbpMesonTemplateExpansion cli_expansions[] = {
  /* Shared */
  { "meson.build", "meson.build" },
  { "flatpak.json", "{{appid}}.json" },
  { "README.md", "README.md" },

  /* C */
  { "src/meson-cli.build", "src/meson.build", IDE_STRV_INIT ("C") },
  { "src/main-cli.c", "src/main.c", IDE_STRV_INIT ("C") },

  /* C++ */
  { "src/meson-cli.build", "src/meson.build", IDE_STRV_INIT ("C++") },
  { "src/main-cli.cpp", "src/main.cpp", IDE_STRV_INIT ("C++") },

  /* Python */
  { "src/meson-py-cli.build", "src/meson.build", IDE_STRV_INIT ("Python") },
  { "src/hello-cli.py.in", "src/{{name}}.in", IDE_STRV_INIT ("Python") },
  { "src/__init__.py", "src/__init__.py", IDE_STRV_INIT ("Python") },
  { "src/main-cli.py", "src/main.py", IDE_STRV_INIT ("Python") },

  /* Rust */
  { "src/meson-cli.build", "src/meson.build", IDE_STRV_INIT ("Rust") },
  { "src/Cargo-cli.toml", "Cargo.toml", IDE_STRV_INIT ("Rust") },
  { "src/main-cli.rs", "src/main.rs", IDE_STRV_INIT ("Rust") },

  /* Vala */
  { "src/meson-cli.build", "src/meson.build", IDE_STRV_INIT ("Vala") },
  { "src/main-cli.vala", "src/main.vala", IDE_STRV_INIT ("Vala") },
};

static GbpMesonTemplateExpansion empty_expansions[] = {
  /* Shared */
  { "meson.build", "meson.build" },
  { "flatpak.json", "{{appid}}.json" },
  { "README.md", "README.md" },
  { "src/meson-empty.build", "src/meson.build" },

  /* Rust */
  { "src/Cargo-cli.toml", "Cargo.toml", IDE_STRV_INIT ("Rust") },
};

static const GbpMesonTemplateInfo templates[] = {
  {
    -1000,
    "adwaita",
    N_("GNOME Application"),
    N_("A Meson-based project for GNOME using GTK 4 and libadwaita"),
    IDE_STRV_INIT ("C", "JavaScript", "Python", "Rust", "Vala"),
    gtk4_expansions, G_N_ELEMENTS (gtk4_expansions),
    gtk4_language_scope, G_N_ELEMENTS (gtk4_language_scope),
    IDE_STRV_INIT ("is_adwaita=true",
                   "is_gtk4=true",
                   "enable_i18n=true",
                   "enable_gnome=true",
                   "ui_file=window.ui",
                   "exec_name={{name}}",
                   "shortcuts_path=shortcuts-dialog"),
  },
  {
    -900,
    "gtk4",
    N_("GTK 4 Application"),
    N_("A Meson-based project using GTK 4"),
    IDE_STRV_INIT ("C", "JavaScript", "Python", "Rust", "Vala"),
    gtk4_expansions, G_N_ELEMENTS (gtk4_expansions),
    gtk4_language_scope, G_N_ELEMENTS (gtk4_language_scope),
    IDE_STRV_INIT ("is_adwaita=false",
                   "is_gtk4=true",
                   "enable_i18n=true",
                   "enable_gnome=true",
                   "ui_file=window.ui",
                   "exec_name={{name}}",
                   "shortcuts_path=gtk/help-overlay"),
  },
  {
    -800,
    "library",
    N_("Shared Library"),
    N_("A Meson-based project for a shared library"),
    IDE_STRV_INIT ("C"),
    library_expansions, G_N_ELEMENTS (library_expansions),
  },
  {
    -700,
    "empty",
    N_("Command Line Tool"),
    N_("An Meson-based project for a command-line program"),
    IDE_STRV_INIT ("C", "C++", "Python", "Rust", "Vala"),
    cli_expansions, G_N_ELEMENTS (cli_expansions),
    NULL, 0,
    IDE_STRV_INIT ("is_cli=true", "exec_name={{name}}"),
  },
  {
    -600,
    "empty",
    N_("Empty Meson Project"),
    N_("An empty Meson project skeleton"),
    IDE_STRV_INIT ("C", "C++", "C♯", "JavaScript", "Python", "Rust", "Vala"),
    empty_expansions, G_N_ELEMENTS (empty_expansions),
    NULL, 0,
    IDE_STRV_INIT ("is_cli=true", "exec_name={{name}}"),
  },
};

static GList *
gbp_meson_template_provider_get_project_templates (IdeTemplateProvider *provider)
{
  GList *list = NULL;

  g_assert (GBP_IS_MESON_TEMPLATE_PROVIDER (provider));

  for (guint i = 0; i < G_N_ELEMENTS (templates); i++)
    {
      g_autofree char *id = NULL;
      g_autoptr(GbpMesonTemplate) template = NULL;

      id = g_strdup_printf ("meson-templates:%s", templates[i].id);
      template = g_object_new (GBP_TYPE_MESON_TEMPLATE,
                               "description", g_dgettext (GETTEXT_PACKAGE, templates[i].description),
                               "id", id,
                               "languages", templates[i].languages,
                               "name", g_dgettext (GETTEXT_PACKAGE, templates[i].name),
                               "priority", templates[i].priority,
                               NULL);
      gbp_meson_template_set_expansions (template,
                                         templates[i].expansions,
                                         templates[i].n_expansions);
      gbp_meson_template_set_extra_scope (template, templates[i].extra_scope);
      gbp_meson_template_set_language_scope (template,
                                             templates[i].language_scope,
                                             templates[i].n_language_scope);
      list = g_list_prepend (list, g_steal_pointer (&template));
    }

  return list;
}

static void
template_provider_iface_init (IdeTemplateProviderInterface *iface)
{
  iface->get_project_templates = gbp_meson_template_provider_get_project_templates;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpMesonTemplateProvider, gbp_meson_template_provider, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_TEMPLATE_PROVIDER, template_provider_iface_init))

static void
gbp_meson_template_provider_class_init (GbpMesonTemplateProviderClass *klass)
{
}

static void
gbp_meson_template_provider_init (GbpMesonTemplateProvider *self)
{
}

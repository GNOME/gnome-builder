/* test-ide-source-view.c
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ide.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <girepository.h>

#define ADD_CLASS(widget,name) \
  gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(widget)), name)

static IdeContext     *gContext;
static GtkWindow      *gWindow;
static GtkStack       *gDocStack;
static GtkMenuButton  *gDocname;
static GtkProgressBar *gProgress;
static GHashTable     *gBufferToView;
static GList          *gFilesToOpen;
static gint            gExitCode = EXIT_SUCCESS;
static gboolean        gWordCompletion;
static gboolean        gDarkMode;
static gboolean        gSearchShadow;
static gboolean        gSmartBackspace;
static gboolean        gDebugScrollOffset;
static gchar          *gCss = "\
@binding-set file-keybindings { \
    bind \"<ctrl>s\" { \"action\" (\"file\", \"save\", \"\") }; \
} \
IdeSourceView { \
    gtk-key-bindings: file-keybindings; \
} \
";

static void
quit (int exit_code)
{
  gExitCode = exit_code;
  gtk_main_quit ();
  return;
}

static void
parsing_error_cb (GtkCssProvider *provider,
                  GtkCssSection  *section,
                  GError         *error,
                  gpointer        user_data)
{
  guint begin;
  guint end;

  begin = gtk_css_section_get_start_line (section);
  end = gtk_css_section_get_end_line (section);

  g_printerr ("CSS parsing error between lines %u and %u: %s\n",
              begin, end, error->message);
}

static void
idedit__context_unload_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  IdeContext *context = (IdeContext *)object;
  GError *error = NULL;

  if (!ide_context_unload_finish (context, result, &error))
    {
      g_printerr ("%s\n", error->message);
      g_clear_error (&error);
    }

  gtk_window_close (gWindow);
}

static gboolean
delete_event_cb (GtkWindow *window,
                 GdkEvent  *event,
                 gpointer   user_data)
{
  if (gContext)
    {
      ide_context_unload_async (gContext,
                                NULL,
                                idedit__context_unload_cb,
                                NULL);
      g_clear_object (&gContext);
      return TRUE;
    }

  gtk_main_quit ();

  return FALSE;
}

static gboolean
debug_draw (IdeSourceView *sv,
            cairo_t       *cr,
            gpointer       user_data)
{
  static GdkRGBA rgba;
  static guint rgba_set;
  GdkRectangle rect;

  g_assert (IDE_IS_SOURCE_VIEW (sv));

  if (G_UNLIKELY (!rgba_set))
    {
      gdk_rgba_parse (&rgba, "#729fcf");
      rgba.alpha = 0.2;
    }

  ide_source_view_get_visible_rect (sv, &rect);
  gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (sv),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         rect.x, rect.y, &rect.x, &rect.y);
  gdk_cairo_rectangle (cr, &rect);
  gdk_cairo_set_source_rgba (cr, &rgba);
  cairo_fill (cr);

  return FALSE;
}

static void
add_buffer (IdeBuffer *buffer)
{
  IdeSourceView *view;
  IdeBackForwardList *bflist;

  bflist = ide_context_get_back_forward_list (gContext);

  view = g_hash_table_lookup (gBufferToView, buffer);

  if (!view)
    {
      GtkScrolledWindow *scroller;
      GtkSourceCompletion *completion;

      scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                               "visible", TRUE,
                               NULL);
      view = g_object_new (IDE_TYPE_SOURCE_VIEW,
                           "auto-indent", TRUE,
                           "back-forward-list", bflist,
                           "buffer", buffer,
                           "enable-word-completion", gWordCompletion,
                           "highlight-current-line", TRUE,
                           "insert-matching-brace", TRUE,
                           "overwrite-braces", TRUE,
                           "scroll-offset", gDebugScrollOffset ? 5 : 0,
                           "sensitive", FALSE,
                           "show-grid-lines", TRUE,
                           "show-line-changes", TRUE,
                           "show-line-numbers", TRUE,
                           "show-right-margin", TRUE,
                           "show-search-bubbles", TRUE,
                           "show-search-shadow", gSearchShadow,
                           "smart-backspace", gSmartBackspace,
                           "snippet-completion", TRUE,
                           "visible", TRUE,
                           NULL);
      completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (view));
      g_object_set (completion, "show-headers", FALSE, NULL);
      if (gDebugScrollOffset)
        g_signal_connect_after (view, "draw", G_CALLBACK (debug_draw), NULL);
      gtk_container_add (GTK_CONTAINER (scroller), GTK_WIDGET (view));
      gtk_container_add (GTK_CONTAINER (gDocStack), GTK_WIDGET (scroller));
      g_hash_table_insert (gBufferToView, buffer, view);
    }
}

static void
load_buffer_cb (IdeBufferManager *bufmgr,
                IdeBuffer        *buffer,
                gpointer          user_data)
{
  add_buffer (buffer);
}

static void
buffer_loaded_cb (IdeBufferManager *bufmgr,
                  IdeBuffer        *buffer,
                  gpointer          user_data)
{
  add_buffer (buffer);
}

static void
switch_to_buffer (IdeBuffer *buffer,
                  guint      line,
                  guint      line_offset)
{
  IdeSourceView *view;
  GtkTextIter iter;
  GtkWidget *parent;

  view = g_hash_table_lookup (gBufferToView, buffer);
  g_assert (view);

  parent = gtk_widget_get_parent (GTK_WIDGET (view));
  gtk_stack_set_visible_child (GTK_STACK (gDocStack), parent);

  gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &iter, line);
  for (; line_offset; line_offset--)
    if (gtk_text_iter_ends_line (&iter) || !gtk_text_iter_forward_char (&iter))
      break;
  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &iter, &iter);
  gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
                                gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer)),
                                0.0, TRUE, 1.0, 0.5);
}

static void
idedit__bufmgr_load_file_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr(IdeBuffer) buf = NULL;
  g_autoptr(IdeSourceLocation) srcloc = user_data;
  GError *error = NULL;
  GtkWidget *view;

  buf = ide_buffer_manager_load_file_finish (IDE_BUFFER_MANAGER (object), result, &error);

  if (!buf)
    {
      g_printerr ("%s\n", error->message);
      g_clear_error (&error);
    }

  if (srcloc)
    {
      guint line;
      guint line_offset;

      line = ide_source_location_get_line (srcloc);
      line_offset = ide_source_location_get_line_offset (srcloc);

      switch_to_buffer (buf, line, line_offset);
    }

  view = g_hash_table_lookup (gBufferToView, buf);
  if (view)
    {
      GtkSourceStyleScheme *scheme;
      GtkSourceStyleSchemeManager *schememgr;
      const gchar *name = gDarkMode ? "builder-dark" : "builder";

      schememgr = gtk_source_style_scheme_manager_get_default ();
      scheme = gtk_source_style_scheme_manager_get_scheme (schememgr, name);
      gtk_source_buffer_set_style_scheme (GTK_SOURCE_BUFFER (buf), scheme);

      ide_buffer_set_highlight_diagnostics (buf, TRUE);

      gtk_widget_set_sensitive (view, TRUE);
      gtk_widget_grab_focus (view);
    }
}

static void
notify_visible_child_cb (GtkStack   *stack,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
  GtkScrolledWindow *child;
  IdeSourceView *view;
  IdeBuffer *buffer;

  child = GTK_SCROLLED_WINDOW (gtk_stack_get_visible_child (stack));

  if (child)
    {
      const gchar *title;

      view = IDE_SOURCE_VIEW (gtk_bin_get_child (GTK_BIN (child)));
      buffer = IDE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
      title = ide_buffer_get_title (buffer);
      gtk_window_set_title (gWindow, title);
      gtk_button_set_label (GTK_BUTTON (gDocname), title);
    }
}

static void
hide_callback (gpointer data)
{
  GtkWidget *widget = data;

  gtk_widget_hide (widget);
  gtk_widget_set_opacity (widget, 1.0);
  g_object_unref (widget);
}

void
widget_fade_hide (GtkWidget *widget)
{
  GdkFrameClock *frame_clock;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (gtk_widget_get_visible (widget))
    {
      frame_clock = gtk_widget_get_frame_clock (widget);
      ide_object_animate_full (widget,
                               IDE_ANIMATION_LINEAR,
                               1000,
                               frame_clock,
                               hide_callback,
                               g_object_ref (widget),
                               "opacity", 0.0,
                               NULL);
    }
}

static void
progress_completed (IdeProgress *progress,
                    GParamSpec  *pspec,
                    GtkWidget   *widget)
{
  widget_fade_hide (widget);
}

static void
save_activate (GSimpleAction *action,
               GVariant      *param,
               gpointer       user_data)
{
  GtkWidget *current;

  current = gtk_stack_get_visible_child (gDocStack);
  if (current != NULL)
    {
      current = gtk_bin_get_child (GTK_BIN (current));
      if (IDE_IS_SOURCE_VIEW (current))
        {
          IdeBufferManager *bufmgr;
          IdeBuffer *buffer;
          IdeFile *file;
          IdeProgress *progress = NULL;

          buffer = IDE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (current)));
          bufmgr = ide_context_get_buffer_manager (gContext);
          file = ide_buffer_get_file (buffer);
          ide_buffer_manager_save_file_async (bufmgr, buffer, file, &progress, NULL, NULL, NULL);

          g_object_bind_property (progress, "fraction", gProgress, "fraction", G_BINDING_SYNC_CREATE);
          g_signal_connect (progress, "notify::completed", G_CALLBACK (progress_completed), gProgress);
          gtk_widget_show (GTK_WIDGET (gProgress));
        }
    }
}

static void
go_forward_activate (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  IdeBackForwardList *list;

  list = ide_context_get_back_forward_list (gContext);

  if (ide_back_forward_list_get_can_go_forward (list))
    ide_back_forward_list_go_forward (list);
}

static void
go_backward_activate (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  IdeBackForwardList *list;

  list = ide_context_get_back_forward_list (gContext);

  if (ide_back_forward_list_get_can_go_backward (list))
    ide_back_forward_list_go_backward (list);
}


static void
navigate_to_cb (IdeBackForwardList *list,
                IdeBackForwardItem *item,
                gpointer            user_data)
{
  IdeBufferManager *bufmgr;
  IdeSourceLocation *srcloc;
  IdeBuffer *buffer;
  IdeFile *file;
  GFile *gfile;
  guint line;
  guint line_offset;

  srcloc = ide_back_forward_item_get_location (item);
  file = ide_source_location_get_file (srcloc);
  line = ide_source_location_get_line (srcloc);
  line_offset = ide_source_location_get_line_offset (srcloc);
  gfile = ide_file_get_file (file);

  bufmgr = ide_context_get_buffer_manager (gContext);
  buffer = ide_buffer_manager_find_buffer (bufmgr, gfile);

  if (buffer)
    {
      switch_to_buffer (buffer, line, line_offset);
      return;
    }

  ide_buffer_manager_load_file_async (bufmgr, file, FALSE, NULL, NULL,
                                      idedit__bufmgr_load_file_cb,
                                      ide_source_location_ref (srcloc));
}

static void
create_window (void)
{
  IdeBackForwardList *bflist;
  GtkHeaderBar *header;
  GtkMenuButton *langbtn;
  GtkBox *box;
  GtkBox *hbox;
  GtkBox *hbox2;
  GtkButton *back;
  GtkButton *forward;
  GtkSeparator *sep;
  GtkButton *closebtn;
  GtkOverlay *overlay;
  GSimpleActionGroup *group;
  GtkCssProvider *css;
  static const GActionEntry entries[] = {
    { "save", save_activate },
  };
  GSimpleActionGroup *nav_group;
  static const GActionEntry nav_entries[] = {
    { "go-backward", go_backward_activate },
    { "go-forward", go_forward_activate },
  };

  css = gtk_css_provider_new ();
  g_signal_connect (css, "parsing-error", G_CALLBACK (parsing_error_cb), NULL);
  gtk_css_provider_load_from_data (css, gCss, -1, NULL);
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (css),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_clear_object (&css);

  gWindow = g_object_new (GTK_TYPE_WINDOW,
                          "default-width", 1280,
                          "default-height", 720,
                          "title", _("idedit"),
                          NULL);
  g_signal_connect (gWindow, "delete-event", G_CALLBACK (delete_event_cb), NULL);

  bflist = ide_context_get_back_forward_list (gContext);
  g_signal_connect (bflist, "navigate-to", G_CALLBACK (navigate_to_cb), NULL);

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), NULL);
  gtk_widget_insert_action_group (GTK_WIDGET (gWindow), "file", G_ACTION_GROUP (group));

  nav_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (nav_group), nav_entries, G_N_ELEMENTS (nav_entries), NULL);
  gtk_widget_insert_action_group (GTK_WIDGET (gWindow), "navigation", G_ACTION_GROUP (nav_group));

  g_object_bind_property (bflist, "can-go-backward",
                          g_action_map_lookup_action (G_ACTION_MAP (nav_group), "go-backward"), "enabled",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (bflist, "can-go-forward",
                          g_action_map_lookup_action (G_ACTION_MAP (nav_group), "go-forward"), "enabled",
                          G_BINDING_SYNC_CREATE);

  header = g_object_new (GTK_TYPE_HEADER_BAR,
                         "show-close-button", TRUE,
                         "title", "idedit",
                         "visible", TRUE,
                         NULL);
  gtk_window_set_titlebar (gWindow, GTK_WIDGET (header));

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (gWindow), GTK_WIDGET (box));

  hbox = g_object_new (GTK_TYPE_BOX,
                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                       "expand", FALSE,
                       "visible", TRUE,
                       NULL);
  ADD_CLASS (hbox, "notebook");
  ADD_CLASS (hbox, "header");
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (hbox));

  /* hack so we can do this with css */
  hbox2 = g_object_new (GTK_TYPE_BOX,
                        "margin-top", 3,
                        "margin-bottom", 3,
                        "margin-start", 6,
                        "margin-end", 6,
                        "orientation", GTK_ORIENTATION_HORIZONTAL,
                        "expand", TRUE,
                        "visible", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (hbox), GTK_WIDGET (hbox2));

  back = g_object_new (GTK_TYPE_BUTTON,
                       "action-name", "navigation.go-backward",
                       "child", g_object_new (GTK_TYPE_IMAGE,
                                              "icon-name", "go-previous-symbolic",
                                              "visible", TRUE,
                                              NULL),
                       "visible", TRUE,
                       NULL);
  ADD_CLASS (back, "image-button");
  ADD_CLASS (back, "flat");
  g_object_bind_property (bflist, "can-go-backward", back, "sensitive", G_BINDING_SYNC_CREATE);
  gtk_box_pack_start (hbox2, GTK_WIDGET (back), FALSE, FALSE, 0);

  forward = g_object_new (GTK_TYPE_BUTTON,
                          "action-name", "navigation.go-forward",
                          "child", g_object_new (GTK_TYPE_IMAGE,
                                                 "icon-name", "go-next-symbolic",
                                                 "visible", TRUE,
                                                 NULL),
                          "visible", TRUE,
                          NULL);
  ADD_CLASS (forward, "image-button");
  ADD_CLASS (forward, "flat");
  g_object_bind_property (bflist, "can-go-forward", forward, "sensitive", G_BINDING_SYNC_CREATE);
  gtk_box_pack_start (hbox2, GTK_WIDGET (forward), FALSE, FALSE, 0);

  sep = g_object_new (GTK_TYPE_SEPARATOR,
                      "margin-top", 3,
                      "margin-bottom", 3,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "visible", TRUE,
                      NULL);
  gtk_box_pack_start (hbox2, GTK_WIDGET (sep), FALSE, FALSE, 0);

  /* document name */
  gDocname = g_object_new (GTK_TYPE_MENU_BUTTON,
                          "label", "my-document.c",
                          "hexpand", TRUE,
                          "visible", TRUE,
                          NULL);
  ADD_CLASS (gDocname, "text-button");
  ADD_CLASS (gDocname, "flat");
  gtk_box_set_center_widget (hbox2, GTK_WIDGET (gDocname));

  closebtn = g_object_new (GTK_TYPE_BUTTON,
                           "child", g_object_new (GTK_TYPE_IMAGE,
                                                  "visible", TRUE,
                                                  "icon-name", "window-close-symbolic",
                                                  NULL),
                           "visible", TRUE,
                           NULL);
  ADD_CLASS (closebtn, "image-button");
  ADD_CLASS (closebtn, "flat");
  gtk_box_pack_end (hbox2, GTK_WIDGET (closebtn), FALSE, FALSE, 0);

  sep = g_object_new (GTK_TYPE_SEPARATOR,
                      "margin-top", 3,
                      "margin-bottom", 3,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "visible", TRUE,
                      NULL);
  gtk_box_pack_end (hbox2, GTK_WIDGET (sep), FALSE, FALSE, 0);

  /* language button */
  langbtn = g_object_new (GTK_TYPE_MENU_BUTTON,
                          "label", "C",
                          "hexpand", FALSE,
                          "visible", TRUE,
                          NULL);
  ADD_CLASS (langbtn, "text-button");
  ADD_CLASS (langbtn, "flat");
  gtk_box_pack_end (hbox2, GTK_WIDGET (langbtn), FALSE, FALSE, 0);

  sep = g_object_new (GTK_TYPE_SEPARATOR,
                      "margin-top", 3,
                      "margin-bottom", 3,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "visible", TRUE,
                      NULL);
  gtk_box_pack_end (hbox2, GTK_WIDGET (sep), FALSE, FALSE, 0);

  overlay = g_object_new (GTK_TYPE_OVERLAY,
                          "expand", TRUE,
                          "visible", TRUE,
                          NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (overlay));

  gProgress = g_object_new (GTK_TYPE_PROGRESS_BAR,
                            "valign", GTK_ALIGN_START,
                            "orientation", GTK_ORIENTATION_HORIZONTAL,
                            "visible", FALSE,
                            NULL);
  ADD_CLASS (gProgress, "osd");
  gtk_overlay_add_overlay (overlay, GTK_WIDGET (gProgress));

  gDocStack = g_object_new (GTK_TYPE_STACK,
                            "expand", TRUE,
                            "visible", TRUE,
                            NULL);
  g_signal_connect (gDocStack, "notify::visible-child", G_CALLBACK (notify_visible_child_cb), NULL);
  gtk_container_add (GTK_CONTAINER (overlay), GTK_WIDGET (gDocStack));
}

static void
idedit__context_new_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  GError *error = NULL;
  IdeBufferManager *bufmgr;
  g_autoptr(GPtrArray) bufs = NULL;
  GList *iter;
  gsize i;

  gContext = ide_context_new_finish (result, &error);

  if (!gContext)
    {
      g_printerr ("%s\n", error->message);
      g_clear_error (&error);
      quit (EXIT_FAILURE);
      return;
    }

  create_window ();

  /* now open all the requested buffers */
  gBufferToView = g_hash_table_new (g_direct_hash, g_direct_equal);

  bufmgr = ide_context_get_buffer_manager (gContext);
  g_signal_connect (bufmgr, "load-buffer", G_CALLBACK (load_buffer_cb), NULL);
  g_signal_connect (bufmgr, "buffer-loaded", G_CALLBACK (buffer_loaded_cb), NULL);

  bufs = ide_buffer_manager_get_buffers (bufmgr);
  for (i = 0; i < bufs->len; i++)
    add_buffer (g_ptr_array_index (bufs, i));

  for (iter = gFilesToOpen; iter; iter = iter->next)
    {
      const gchar *path = iter->data;
      IdeProject *project;
      IdeFile *file;

      project = ide_context_get_project (gContext);
      g_assert (project);
      g_assert (IDE_IS_PROJECT (project));

      file = ide_project_get_file_for_path (project, path);
      g_assert (file);
      g_assert (IDE_IS_FILE (file));

      ide_buffer_manager_load_file_async (bufmgr, file, FALSE, NULL, NULL,
                                          idedit__bufmgr_load_file_cb, NULL);

      g_object_unref (file);
    }

  gtk_window_present (gWindow);
}

static gboolean
increase_verbosity (void)
{
  ide_log_increase_verbosity ();
  return TRUE;
}

static void
load_css_resource (const gchar *path)
{
  GtkCssProvider *provider;

  provider = gtk_css_provider_new ();
  g_signal_connect (provider, "parsing-error", G_CALLBACK (parsing_error_cb), NULL);
  gtk_css_provider_load_from_resource (provider, path);
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default(),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);
}

int
main (int argc,
      char *argv[])
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GFile) project_dir = NULL;
  GError *error = NULL;
  gboolean emacs = FALSE;
  gboolean vim = FALSE;
  gsize i;
  const GOptionEntry entries[] = {
    { "words", 'w', 0, G_OPTION_ARG_NONE, &gWordCompletion,
      N_("Use words in all buffers for autocompletion") },
    { "verbose", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
      increase_verbosity, N_("Increase logging verbosity.") },
    { "emacs", 'e', 0, G_OPTION_ARG_NONE, &emacs, N_("Use emacs keybindings") },
    { "vim", 'm', 0, G_OPTION_ARG_NONE, &vim, N_("Use Vim keybindings") },
    { "dark", 'd', 0, G_OPTION_ARG_NONE, &gDarkMode, N_("Use dark mode") },
    { "shadow", 's', 0, G_OPTION_ARG_NONE, &gSearchShadow, N_("Show shadow when searching") },
    { "smart-backspace", 'b', 0, G_OPTION_ARG_NONE, &gSmartBackspace, N_("Enable smart backspace") },
    { "debug-scroll-offset", 0, 0, G_OPTION_ARG_NONE, &gDebugScrollOffset,
      N_("Render a rectangle over the visible region taking scroll offset into account.") },
    { NULL }
  };

  g_irepository_require_private (g_irepository_get_default (),
                                 BUILDDIR,
                                 "Ide", "1.0", 0, NULL);

  ide_set_program_name ("gnome-builder");
  ide_log_init (TRUE, NULL);

  context = g_option_context_new (_("[FILES...] - A mini editor for libide"));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gtk_get_option_group (TRUE));

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_clear_error (&error);
      return EXIT_FAILURE;
    }

  if (argc < 2)
    {
      g_printerr (_("Please specify a file to edit.\n"));
      return EXIT_FAILURE;
    }

  for (i = 1; i < argc; i++)
    gFilesToOpen = g_list_append (gFilesToOpen, g_strdup (argv [i]));

  project_dir = g_file_new_for_path (".");

  ide_context_new_async (project_dir,
                         NULL,
                         idedit__context_new_cb,
                         NULL);

  if (emacs && vim)
    {
      g_printerr ("You're crazy, you can't have both emacs and vim!\n");
      return EXIT_FAILURE;
    }

  if (emacs)
    load_css_resource ("/org/gnome/libide/keybindings/emacs.css");

  if (vim)
    load_css_resource ("/org/gnome/libide/keybindings/vim.css");

  if (gDarkMode)
    g_object_set (gtk_settings_get_default (),
                  "gtk-application-prefer-dark-theme", TRUE,
                  NULL);

  gtk_main ();

  return gExitCode;
}

#########################
Extending the Editor Page
#########################

You might want to add additional widgets or perform various operations on the view as the user interacts with the code-editor.
One way to do this is to create an ``Ide.EditorPageAddin`` in your plugin.
Implementing this interface allows you to be notified whenever a new view is created as well as when the current language changes.

Additionally, if you only want your plugin to be enabled when certain languages are active, you can set the ``X-Editor-Page-Languages=c,cplusplus`` keyword in your ``.plugin`` file.
In the example provided, the plugin interface would be enabled for ``C`` and ``C++`` files.

.. code-block:: python3

   # my_plugin.py

   import gi

   from gi.repository import GObject, Gtk, GtkSource, Ide

   class MyEditorPageAddin(GObject.Object, Ide.EditorPageAddin):

       def do_load(self, page: Ide.EditorPage):
           """
           @page: an Ide.EditorPage which contains the buffer and sourceview.
           """

           # You get get the Ide.Buffer using
           buffer = page.get_buffer()

           # Or the Ide.SourcePage widget
           source_view = page.get_view()

           # Toggle the overview map
           page.set_show_map(True)
           page.set_auto_hide_map(True)

           # Scroll to a given line
           page.scroll_to_line(100)

           # Change the language
           lang = GtkSource.LanguageManager.get_default().get_language('c')
           page.set_language(lang)

           # Jump to the next error
           page.move_next_error()

           # Jump to the next search result
           page.move_next_search_result()

       def do_unload(self, page: Ide.EditorPage):
           """
           This should undo anything you setup when loading the addin.
           """

       def do_language_changed(self, lang_id: str):
           print("language was changed to", lang_id)

       def do_stack_set(self, stack: Ide.Frame):
           print("Page was moved to document stack", stack)

           # If you have an Ide.LayoutStackAddin, you might want to coordinate
           # between them. To locate your stack addin, you can fetch it like
           # the following:
           #
           # Note that the module name must match Module= inyour .plugin file.
           module_name = 'my-plugin-module'
           other_addin = Ide.LayoutStackAddin.find_by_module_name(stack, module_name)


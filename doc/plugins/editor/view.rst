#########################
Extending the Editor View
#########################

You might want to add additional widgets or perform various operations on the view as the user interacts with the code-editor.
One way to do this is to create an ``Ide.EditorViewAddin`` in your plugin.
Implementing this interface allows you to be notified whenever a new view is created as well as when the current language changes.

Additionally, if you only want your plugin to be enabled when certain languages are active, you can set the ``X-Editor-View-Languages=c,cplusplus`` keyword in your ``.plugin`` file.
In the example provided, the plugin interface would be enabled for ``C`` and ``C++`` files.

.. code-block:: python3

   # my_plugin.py

   import gi

   from gi.repository import GObject
   from gi.repository import Gtk
   from gi.repository import GtkSource
   from gi.repository import Ide

   class MyEditorViewAddin(GObject.Object, Ide.EditorViewAddin):

       def do_load(self, view):
           """
           @view is an Ide.EditorView which contains the buffer and sourceview.
           """

           # You get get the Ide.Buffer using
           buffer = view.get_buffer()

           # Or the Ide.SourceView widget
           source_view = view.get_view()

           # Toggle the overview map
           view.set_show_map(True)
           view.set_auto_hide_map(True)

           # Scroll to a given line
           view.scroll_to_line(100)

           # Change the language
           lang = GtkSource.LanguageManager.get_default().get_language('c')
           view.set_language(lang)

           # Jump to the next error
           view.move_next_error()

           # Jump to the next search result
           view.move_next_search_result()

       def do_unload(self, view):
           """
           This should undo anything you setup when loading the addin.
           """

       def do_language_changed(self, lang_id):
           print("language was changed to", lang_id)

       def do_stack_set(self, stack):
           print("View was moved to document stack", stack)


##############################
Providing Completion Proposals
##############################

Builder has a robust completion engine that allows for efficent access to results.
Plugins should implement the ``Ide.CompletionProvider`` interface to provide a ``Gio.ListModel`` of ``Ide.CompletionProposal``.
Plugins have an opportunity to refine results as the user continues to provide input.

.. note:: You can learn more about the implementation at https://blogs.gnome.org/chergert/2018/06/09/a-new-completion-engine-for-builder/


Example Completion Provider
===========================

.. code-block:: python3

   # my_plugin.py

   import gi

   from gi.repository import GObject
   from gi.repository import Gio
   from gi.repository import Ide

   class MyCompletionProvider(GLib.Object, Ide.CompletionProvider):

       def do_populate_async(self, context, cancellable, callback, data):
           """
           @context: an Ide.CompletionContext containing state for the completion request
           @cancellable: a Gio.Cancellable that can be used to cancel the request
           @callback: a callback to execute after proposals are generated
           @data: closure data (unnecessary for us since PyGObject handles it)
           """

           # Create a task for our async operation
           task = Ide.Task.new(self, cancellable, callback)

           # Create a container for all our results. You may consider implementing
           # Gio.ListModel directly if performance is of high concern.
           task.proposals = Gio.ListStore(type(Ide.CompletionProposal))

           item = MyProposal('my proposal')
           task.proposals.append(item)

           # Complete the task so that the completion engine calls us back at
           # do_populate_finish() to complete the operation.
           task.return_boolean(True)


       def do_populate_finish(self, task):
           """
           @task: the task we created in do_populate_async()

           The goal here is to copmlete the operation by returning our Gio.ListModel
           back to the caller.
           """
           if task.propagate_boolean():
               return task.proposals


       def do_display_proposal(self, row, context, typed_text, proposal):
           """
           We need to update the state of @row using the info from our proposal.
           This is called as the user moves through the result set.

           @row: the Ide.CompletionListBoxRow to display
           @context: the Ide.CompletionContext
           @typed_text: what the user typed
           @proposal: the proposal to display using @row
           """
           row.set_icon_name()
           row.set_left('Left hand side')
           row.set_right('Right hand side')
           row.set_center(proposal.title)

           # You might find fuzzy highlighting useful for creating Pango markup
           markup = Ide.Completion.fuzzy_highlight(proposal.title, typed_text)

           # If you have Pango markup, you can use set_center_markup()
           row.set_center_markup(markup)


       def do_activate_proposal(self, context, proposal, key):
           """
           @proposal: the proposal to be activated
           @key: the Gdk.EventKey representing what the user pressed to activate the row
                 you may want to use this to alter what is inserted (such as converting
                 . to -> in a C/C++ language provider).

           This is where we do the work to insert the proposal. You may want to
           check out the snippet engine to use snippets instead, as some of the
           providers do which auto-complete parameters.
           """

           buffer = context.get_buffer()

           # Start a "user action" so that all the changes are coalesced into a
           # single undo action.
           buffer.begin_user_action()

           # Delete the typed text if any
           has_selection, begin, end = context.get_bounds()
           if has_selection:
               buffer.delete(begin, end)

           # Now insert our proposal
           buffer.insert(begin, proposal.title, len(proposal.title))

           # Complete the user action
           buffer.begin_end_action()


       def do_refilter(self, context, proposals):
           """
           If you can refilter the results based on updated typed text, this
           is where you would adjust @proposals to do that. @proposals is the
           Gio.ListModel returned from do_populate_finish().
           """
           typed_text = context.get_word()
           # filter results...
           return True


   class MyProposal(GObject.Object, Ide.CompletionProposal):
       def __init__(self, title):
           super().__init__()
           self.title = title


There are a number of additional things you can implement in your provider.
See the IdeCompletionProvider implementation for a description of the interface.


Examples from Builder
=====================

 * `Includes completion provider`_ for header includes in C/C++ files
 * `Jedi completion provider`_ for Python completions
 * `Ctags completion provider`_ for lightning-fast ctags completion
 * `Clang completion provider`_ for a fast, caching, completion provider based on libclang
 * `Vala completion provider`_ for completions in the Vala language


.. _`Includes completion provider`: https://gitlab.gnome.org/GNOME/gnome-builder/blob/master/src/plugins/c-pack/cpack-completion-provider.c
.. _`Jedi completion provider`: https://gitlab.gnome.org/GNOME/gnome-builder/blob/master/src/plugins/jedi/jedi_plugin.py
.. _`Ctags completion provider`: https://gitlab.gnome.org/GNOME/gnome-builder/blob/master/src/plugins/ctags/ide-ctags-completion-provider.c
.. _`Clang completion provider`: https://gitlab.gnome.org/GNOME/gnome-builder/blob/master/src/plugins/clang/ide-clang-completion-provider.c
.. _`Vala completion provider`: https://gitlab.gnome.org/GNOME/gnome-builder/blob/master/src/plugins/vala-pack/ide-vala-completion-provider.vala

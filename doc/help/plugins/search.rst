########################
Extending Project Search
########################

Builder provides a global search box in the top right of the workbench.
It can be extended to provide new types of search results.

For example, the ``file-search`` extension provides search results based on files found in the project tree.

.. image:: ../figures/file-search.png
   :width: 629 px
   :align: center

Additionally, the ``code-index`` extension provides search results based on an index built from your project code.

.. image:: ../figures/symbol-search.png
   :width: 605 px
   :align: center

To add new search results, implement the ``Ide.SearchProvider`` interface in your plugin.

.. code-block:: python3

   # my_plugin.py

   import gi

   from gi.repository import GObject
   from gi.repository import Gio
   from gi.repository import Ide

   class MySearchResult(Ide.SearchResult):
       def __index__(self, context, i)
           self.context = context
           self.title = 'Item ' + str(i)
           self.score = i / 100.0

       def do_get_source_location(self):
           """
           Currently, search results must point to a source location.

           This may change in a future release to allow for more
           flexability. Get in touch with us if you need this.
           """
           return Ide.SourceLocation.new(self.context, line, line_offset, 0)

   class MySearchProvider(Ide.Object, Ide.SearchProvider):

       def do_search_async(self, query, max_results, cancellable, callback, data):
           """
           Asynchronously searches for results.

           The search engine will take the results from do_search_finish()
           and add it to the search results list, sorted by score.
           """
           task = Gio.Task.new(self, cancellable, callback)
           task.results = []

           for i in range(0, 10):
               result = MySearchResult(self.get_context(), i)
               task.results.append(result)

           task.return_boolean(True)

       def do_search_finish(self, task):
           return task.results



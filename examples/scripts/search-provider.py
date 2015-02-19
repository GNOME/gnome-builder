# ~/.config/gnome-builder/scripts/script1.py

from gi.repository import GObject, Ide

# just some examples of things you can access
Project = Context.get_project()
BuildSystem = Context.get_build_system()
Vcs = Context.get_vcs()

# get a handle to the search engine
SearchEngine = Context.get_search_engine()

# create a custom provider subclass
class MySearchProvider(Ide.SearchProvider):
    # perform the search operation. can be asynchronous
    def do_populate(self, search_context, search_terms, max_results, cancellable):
        result = Ide.SearchResult(Context, 'weeee', 'more weeeeeee', 0.5)
        search_context.add_result(self, result)
        search_context.provider_completed(self)

    # this is what is shown in the search ui describing the action
    def do_get_verb(self):
        return "foobar"

# add our custom provider to the search engine
SearchEngine.add_provider(MySearchProvider(context=Context))

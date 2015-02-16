/* ~/.config/gnome-builder/scripts/script1.js */

const Ide = imports.gi.Ide;
const GObject = imports.gi.GObject;

/* just some examples of things you can access */
let Project = Context.get_project();
let BuildSystem = Context.get_build_system();
let Vcs = Context.get_vcs();

/* get a handle to the search engine */
let SearchEngine = Context.get_search_engine();

/* create a custom provider subclass */
const MySearchProvider = new GObject.Class({
    Name: 'MySearchProvider',
    Extends: Ide.SearchProvider,

    /* perform the search operation. can be asynchronous */
    vfunc_populate: function(search_context, search_terms, max_results, cancellable) {
        let result = new Ide.SearchResult({
            'context': Context,
            'title': 'weeee',
            'subtitle': 'more weeeeeee',
            'score': 0.5
        });
        search_context.add_result(this, result);
        search_context.provider_completed(this);
    },

    /* this is what is shown in the search ui describing the action */ 
    vfunc_get_verb: function() {
        return "foobar";
    }
});

/* add our custom provider to the search engine */
SearchEngine.add_provider(new MySearchProvider({'context': Context}));

const Gtk = imports.gi.Gtk;
const Lang = imports.lang;

var {{PreFix}}Window = new Lang.Class({
    Name: '{{PreFix}}Window',
    GTypeName: '{{PreFix}}Window',
    Extends: Gtk.ApplicationWindow,
    Template: 'resource://{{appid_path}}/{{ui_file}}',
    InternalChildren: ['label'],

    _init(application) {
        this.parent({
            application,
        });
    },
});


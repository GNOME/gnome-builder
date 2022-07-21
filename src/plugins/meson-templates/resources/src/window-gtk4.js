{{include "license.js"}}

import GObject from 'gi://GObject';
import Gtk from 'gi://Gtk';
{{if is_adwaita}}
import Adw from 'gi://Adw';
{{end}}

export const {{PreFix}}Window = GObject.registerClass({
    GTypeName: '{{PreFix}}Window',
    Template: 'resource://{{appid_path}}/{{ui_file}}',
    InternalChildren: ['label'],
}, class {{PreFix}}Window extends {{if is_adwaita}}Adw{{else}}Gtk{{end}}.ApplicationWindow {
    constructor(application) {
        super({ application });
    }
});


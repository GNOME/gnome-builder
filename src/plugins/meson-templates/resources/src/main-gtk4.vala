{{include "license.vala"}}

int main (string[] args) {
    var app = new {{PreFix}}.Application ();
    return app.run (args);
}

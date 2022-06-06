marked.setOptions({
  renderer: new marked.Renderer(),
  gfm: true,
  tables: true,
  breaks: false,
  pedantic: false,
  sanitize: false,
  smartLists: true,
  smartypants: false
});

function preview() {
  document.getElementById('preview').innerHTML = marked(document.getElementById('markdown-source').childNodes[0].nodeValue);
}

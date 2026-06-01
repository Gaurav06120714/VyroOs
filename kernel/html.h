#ifndef HTML_H
#define HTML_H

// Render a minimal HTML string onto the framebuffer.
// Supports: <h1> <h2> <p> <b> <a> <ul> <li> <br> and text.
// Returns when the user presses ESC.
void html_render(const char* html);

#endif

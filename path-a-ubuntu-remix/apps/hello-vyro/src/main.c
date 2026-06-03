/*
 * hello-vyro — sample GTK4 application
 *
 * Demonstrates the Vyro design tokens on a real GTK4 app. Drops into the
 * Vyro OS Activities view as "Hello, Vyro" and shows a card-on-glass UI
 * with the accent button, an entry field, and a level bar.
 *
 * Build:  meson setup builddir && meson compile -C builddir
 * Run:    ./builddir/hello-vyro
 */

#include <gtk/gtk.h>

static const char VYRO_CSS[] =
    ".vyro-card {"
    "  background-color: rgba(20, 22, 32, 0.78);"
    "  border: 1px solid rgba(255, 255, 255, 0.08);"
    "  border-radius: 14px;"
    "  padding: 32px;"
    "  box-shadow: 0 12px 40px rgba(0, 0, 0, 0.45);"
    "}"
    ".vyro-title {"
    "  font-family: 'Inter', sans-serif;"
    "  font-size: 22pt;"
    "  font-weight: 600;"
    "  color: #E8E9F1;"
    "}"
    ".vyro-subtitle {"
    "  font-family: 'Inter', sans-serif;"
    "  font-size: 11pt;"
    "  color: #A0A4B6;"
    "  margin-bottom: 16px;"
    "}"
    "window {"
    "  background-color: #0B0B12;"
    "}";

static void on_greet(GtkButton *btn, GtkLabel *out) {
    const char *who = g_object_get_data(G_OBJECT(btn), "entry-text");
    if (!who || !*who) who = "world";
    gchar *msg = g_strdup_printf("Hello, %s — welcome to Vyro OS.", who);
    gtk_label_set_text(out, msg);
    g_free(msg);
}

static void on_entry_changed(GtkEditable *e, GtkButton *btn) {
    g_object_set_data_full(G_OBJECT(btn), "entry-text",
                           g_strdup(gtk_editable_get_text(e)), g_free);
}

static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css, VYRO_CSS);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkWidget *win = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win), "Hello, Vyro");
    gtk_window_set_default_size(GTK_WINDOW(win), 520, 380);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(root, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(root, GTK_ALIGN_CENTER);
    gtk_window_set_child(GTK_WINDOW(win), root);

    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(card, "vyro-card");
    gtk_box_append(GTK_BOX(root), card);

    GtkWidget *title = gtk_label_new("Hello, Vyro");
    gtk_widget_add_css_class(title, "vyro-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(card), title);

    GtkWidget *sub = gtk_label_new("A real GTK4 app using the Vyro design tokens.");
    gtk_widget_add_css_class(sub, "vyro-subtitle");
    gtk_widget_set_halign(sub, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(card), sub);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Your name");
    gtk_box_append(GTK_BOX(card), entry);

    GtkWidget *btn = gtk_button_new_with_label("Say hello");
    gtk_widget_add_css_class(btn, "suggested-action");
    gtk_box_append(GTK_BOX(card), btn);

    GtkWidget *out = gtk_label_new("");
    gtk_widget_set_halign(out, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(card), out);

    g_signal_connect(entry, "changed", G_CALLBACK(on_entry_changed), btn);
    g_signal_connect(btn,   "clicked", G_CALLBACK(on_greet),         out);

    gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("os.vyro.HelloVyro", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}

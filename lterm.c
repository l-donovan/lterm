#include <vte/vte.h>
#include <gtk/gtk.h>
#include <libconfig.h>
#include <stdlib.h>
#include <string.h>

#define CLR_R(x)   (((x) & 0xff0000) >> 16)
#define CLR_G(x)   (((x) & 0x00ff00) >>  8)
#define CLR_B(x)   (((x) & 0x0000ff) >>  0)
#define CLR_16(x)  ((double)(x) / 0xff)
#define CLR_GDK(x) (const GdkRGBA){ .red = CLR_16(CLR_R(x)), \
                                    .green = CLR_16(CLR_G(x)), \
                                    .blue = CLR_16(CLR_B(x)), \
                                    .alpha = 0 }

static gboolean on_title_changed();
static gboolean on_key_press();
       void     load_config_file();

GtkWidget *window, *terminal;

gboolean isfullscreen;

static gboolean on_title_changed(GtkWidget *terminal, gpointer user_data) {
    GtkWindow *window = user_data;
    gtk_window_set_title(window, vte_terminal_get_window_title(VTE_TERMINAL(terminal))?:"Terminal");
    return TRUE;
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    if (event->state & GDK_MOD1_MASK) {
        switch (event->keyval) {
            case GDK_KEY_Return:
                if (isfullscreen)
                    gtk_window_unfullscreen(GTK_WINDOW(window));
                else
                    gtk_window_fullscreen(GTK_WINDOW(window));
                isfullscreen = !isfullscreen;
                break;
        }
    }

    return FALSE;
}

void load_config_file() {
    config_t cfg;
    config_setting_t *setting;

    config_init(&cfg);

    if (!config_read_file(&cfg, "lterm.cfg")) {
        fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
        config_destroy(&cfg);
        return;
    }

    setting = config_lookup(&cfg, "colors");
    if (setting != NULL) {
        int c = config_setting_length(setting);
        GdkRGBA colors[c];

        for (int i = 0; i < c; i++) 
            colors[i] = CLR_GDK(config_setting_get_int_elem(setting, i));

        vte_terminal_set_colors(
            VTE_TERMINAL(terminal),
            &colors[c - 1],
            NULL,
            colors,
            c
        );
    }

    setting = config_lookup(&cfg, "font");
    if (setting != NULL) {
        const char *fontfamily;
        int fontsize;

        config_setting_lookup_string(setting, "family", &fontfamily);
        config_setting_lookup_int(setting, "size", &fontsize);

        PangoFontDescription *font = pango_font_description_new();
        pango_font_description_set_family(font, fontfamily);
        pango_font_description_set_size(font, PANGO_SCALE * fontsize);

        vte_terminal_set_font(VTE_TERMINAL(terminal), font);
    }

    setting = config_lookup(&cfg, "terminal");
    if (setting != NULL) {
        int rows, cols, scrollback;
        gboolean rewrap, autohide;

        config_setting_lookup_int(setting, "rows", &rows);
        config_setting_lookup_int(setting, "cols", &cols);
        config_setting_lookup_int(setting, "scrollback", &scrollback);
        config_setting_lookup_bool(setting, "rewrap_on_resize", &rewrap);
        config_setting_lookup_bool(setting, "autohide_mouse", &autohide);

        vte_terminal_set_size(VTE_TERMINAL(terminal), cols, rows);
        vte_terminal_set_scrollback_lines(VTE_TERMINAL(terminal), scrollback);
        vte_terminal_set_rewrap_on_resize(VTE_TERMINAL(terminal), rewrap);
        vte_terminal_set_mouse_autohide(VTE_TERMINAL(terminal), autohide);
    }

    setting = config_lookup(&cfg, "window");
    if (setting != NULL) {
        gboolean fullscreen;

        config_setting_lookup_bool(setting, "start_fullscreen", &fullscreen);
        
        isfullscreen = fullscreen;
        if (fullscreen) 
            gtk_window_fullscreen(GTK_WINDOW(window));
    }

    printf("Successfully loaded config file\n");
    config_destroy(&cfg);
    return;
}

int main(int argc, char *argv[]) {
    GtkWidget *box, *menubar, *fileMenu, *fileMi, *quitMi;

    gtk_init(&argc, &argv);
    terminal = vte_terminal_new();
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "lTerm");

    menubar = gtk_menu_bar_new();
    fileMenu = gtk_menu_new();

    fileMi = gtk_menu_item_new_with_label("File");
    quitMi = gtk_menu_item_new_with_label("Quit");

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(fileMi), fileMenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), quitMi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), fileMi);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), box);
    gtk_box_pack_start(GTK_BOX(box), menubar, FALSE, FALSE, 0);
    gtk_box_pack_end  (GTK_BOX(box), terminal, TRUE, TRUE, 0);

    load_config_file();

    gchar **envp = g_get_environ();
    gchar **command = (gchar *[]){g_strdup(g_environ_getenv(envp, "SHELL")), NULL };
    g_strfreev(envp);

    vte_terminal_spawn_sync(
        VTE_TERMINAL(terminal), // terminal
        VTE_PTY_DEFAULT,        // pty_flags
        NULL,                   // working_directory
        command,                // argv
        NULL,                   // envv
        0,                      // spawn_flags
        NULL,                   // child_setup
        NULL,                   // child_setup_data
        NULL,                   // child_pid
        NULL,                   // cancellable
        NULL                    // error
    );

    g_signal_connect(quitMi, "activate", gtk_main_quit, NULL);
    g_signal_connect(window, "delete-event", gtk_main_quit, NULL);
    g_signal_connect(terminal, "child-exited", gtk_main_quit, NULL);
    g_signal_connect(terminal, "window-title-changed", G_CALLBACK(on_title_changed), GTK_WINDOW(window));
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), NULL);

    gtk_widget_show_all(window);
    gtk_main();
}

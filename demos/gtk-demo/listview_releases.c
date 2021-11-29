/* Lists/Application launcher
 * #Keywords: GtkListItemFactory, GListModel,JSON
 *
 * This demo downloads GTK's latest release and displays them in a list.
 *
 * It shows how hard it still is to get JSON into lists.
 */

#include <gtk/gtk.h>

#include "gtk/json/gtkjsonparserprivate.h"

#define GTK_TYPE_RELEASE (gtk_release_get_type ())

G_DECLARE_FINAL_TYPE (GtkRelease, gtk_release, GTK, RELEASE, GObject)

struct _GtkRelease
{
  GObject parent_instance;

  char *name;
  GDateTime *timestamp;
};

struct _GtkReleaseClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (GtkRelease, gtk_release, G_TYPE_OBJECT)

static void
gtk_release_class_init (GtkReleaseClass *klass)
{
}

static void
gtk_release_init (GtkRelease *self)
{
}

static GtkRelease *
gtk_release_new (const char *name,
                 GDateTime  *timestamp)
{
  GtkRelease *result;

  result = g_object_new (GTK_TYPE_RELEASE, NULL);

  result->name = g_strdup (name);

  return result;
}

static void
setup_listitem_cb (GtkListItemFactory *factory,
                   GtkListItem        *list_item)
{
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_list_item_set_child (list_item, label);
}

static void
bind_listitem_cb (GtkListItemFactory *factory,
                  GtkListItem        *list_item)
{
  GtkWidget *label;
  GtkRelease *release;

  label = gtk_list_item_get_child (list_item);
  release = gtk_list_item_get_item (list_item);

  gtk_label_set_label (GTK_LABEL (label), release->name);
}

static void
loaded_some_releases_cb (GObject      *file,
                         GAsyncResult *res,
                         gpointer      userdata)
{
  GListStore *store = userdata;
  GBytes *bytes;
  GtkJsonParser *parser;
  GError *error = NULL;

  bytes = g_file_load_bytes_finish (G_FILE (file), res, NULL, &error);
  if (bytes == NULL)
    {
      g_printerr ("Error loading: %s\n", error->message);
      g_clear_error (&error);
      return;
    }

  parser = gtk_json_parser_new_for_bytes (bytes);
  g_bytes_unref (bytes);

  gtk_json_parser_start_array (parser);
  do
    {
      enum { NAME, COMMIT };
      static const char *options[] = { "name", "commit", NULL };
      char *name = NULL;

      gtk_json_parser_start_object (parser);
      do
        {
          switch (gtk_json_parser_select_member (parser, options))
            {
              case NAME:
                g_clear_pointer (&name, g_free);
                name = gtk_json_parser_get_string (parser);
                break;
              case COMMIT:
                break;
              default:
                break;
            }
        }
      while (gtk_json_parser_next (parser));
      gtk_json_parser_end (parser);

      if (name)
        {
          GtkRelease *release = gtk_release_new (name, NULL);
          g_list_store_append (store, release);
          g_object_unref (release);
        }
      g_clear_pointer (&name, g_free);
    }
  while (gtk_json_parser_next (parser));
  gtk_json_parser_end (parser);

  if (gtk_json_parser_get_error (parser))
    {
      const GError *json_error = gtk_json_parser_get_error (parser);

      g_printerr ("Error parsing: %s\n", json_error->message);
    }
  gtk_json_parser_free (parser);
}

static void
load_some_releases (GListStore *store)
{
  GFile *file = g_file_new_for_uri ("https://gitlab.gnome.org/api/v4/projects/665/repository/tags");

  g_file_load_bytes_async (file, NULL, loaded_some_releases_cb, store);
  g_object_unref (file);
}

static GtkWidget *window = NULL;

GtkWidget *
do_listview_releases (GtkWidget *do_widget)
{
  if (window == NULL)
    {
      GtkWidget *list, *sw;
      GListStore *store;
      GtkListItemFactory *factory;

      window = gtk_window_new ();
      gtk_window_set_default_size (GTK_WINDOW (window), 640, 320);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "GTK releases");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);

      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_listitem_cb), NULL);
      g_signal_connect (factory, "bind", G_CALLBACK (bind_listitem_cb), NULL);

      store = g_list_store_new (GTK_TYPE_RELEASE);

      list = gtk_list_view_new (GTK_SELECTION_MODEL (gtk_single_selection_new (G_LIST_MODEL (store))), factory);

      sw = gtk_scrolled_window_new ();
      gtk_window_set_child (GTK_WINDOW (window), sw);
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), list);

      load_some_releases (store);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}

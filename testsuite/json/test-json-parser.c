/*
 * Copyright (C) 2021 Red Hat Inc.
 *
 * Author:
 *      Benjamin Otte <otte@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gio/gio.h>
#include <locale.h>
#include <string.h>

#include "gtk/json/gtkjsonparserprivate.h"
#include "gtk/json/gtkjsonprinterprivate.h"
#include "testsuite/testutils.h"

#ifdef G_OS_WIN32
# include <io.h>
#endif

static char *
test_get_reference_file (GFile *json_file)
{
  char *path;
  GString *file = g_string_new (NULL);

  path = g_file_get_path (json_file);
  g_assert (path);

  if (g_str_has_suffix (path, ".json"))
    g_string_append_len (file, path, strlen (path) - 4);
  else
    g_string_append (file, path);
  
  g_string_append (file, ".ref.json");

  if (!g_file_test (file->str, G_FILE_TEST_EXISTS))
    {
      g_string_free (file, TRUE);
      return path;
    }
  g_free (path);

  return g_string_free (file, FALSE);
}

static void
parse_and_print (GtkJsonParser  *parser,
                 GtkJsonPrinter *printer)
{
  while (TRUE)
    {
      char *name = gtk_json_parser_get_member_name (parser);

      switch (gtk_json_parser_get_node (parser))
        {
        case GTK_JSON_NONE:
          if (gtk_json_printer_get_depth (printer) == 0)
            return;
          gtk_json_printer_end (printer);
          gtk_json_parser_end (parser);
          gtk_json_parser_next (parser);
          break;

        case GTK_JSON_NULL:
          gtk_json_printer_add_null (printer, name);
          gtk_json_parser_next (parser);
          break;

        case GTK_JSON_BOOLEAN:
          gtk_json_printer_add_boolean (printer,
                                        name,
                                        gtk_json_parser_get_boolean (parser));
          gtk_json_parser_next (parser);
          break;

        case GTK_JSON_NUMBER:
          gtk_json_printer_add_number (printer,
                                       name,
                                       gtk_json_parser_get_number (parser));
          gtk_json_parser_next (parser);
          break;

        case GTK_JSON_STRING:
          { 
            char *s = gtk_json_parser_get_string (parser);
            gtk_json_printer_add_string (printer, name, s);
            g_free (s);
          }
          gtk_json_parser_next (parser);
          break;

        case GTK_JSON_OBJECT:
          gtk_json_printer_start_object (printer, name);
          gtk_json_parser_start_object (parser);
          break;

        case GTK_JSON_ARRAY:
          gtk_json_printer_start_array (printer, name);
          gtk_json_parser_start_array (parser);
          break;

        default:
          g_assert_not_reached ();
          return;
        }

      g_free (name);
    }
}

static void
string_append_func (GtkJsonPrinter *printer,
                    const char     *s,
                    gpointer        data)
{
  g_string_append (data, s);
}

static void
parse_json_file (GFile *file, gboolean generate)
{
  GtkJsonPrinter *printer;
  GtkJsonParser *parser;
  char *reference_file;
  GBytes *bytes;
  char *diff;
  GString *string;
  GError *error = NULL;

  bytes = g_file_load_bytes (file, NULL, NULL, &error);
  g_assert_no_error (error);

  parser = gtk_json_parser_new_for_bytes (bytes);
  string = g_string_new (NULL);
  printer = gtk_json_printer_new (string_append_func,
                                  string,
                                  NULL);
  gtk_json_printer_set_flags (printer, GTK_JSON_PRINTER_PRETTY);
  parse_and_print (parser, printer);
  g_string_append (string, "\n");

  g_assert_no_error (gtk_json_parser_get_error (parser));

  gtk_json_printer_free (printer);
  gtk_json_parser_free (parser);

  reference_file = test_get_reference_file (file);

  diff = diff_with_file (reference_file, string->str, string->len, &error);
  g_assert_no_error (error);

  if (diff && diff[0])
    {
      g_test_message ("Resulting CSS doesn't match reference:\n%s", diff);
      g_test_fail ();
    }
  g_free (reference_file);
  g_free (diff);
  g_string_free (string, TRUE);
  g_bytes_unref (bytes);
}

static void
test_json_file (GFile *file)
{
  parse_json_file (file, FALSE);
}

static void
add_test_for_file (GFile *file)
{
  char *path;

  path = g_file_get_path (file);

  g_test_add_vtable (path,
                     0,
                     g_object_ref (file),
                     NULL,
                     (GTestFixtureFunc) test_json_file,
                     (GTestFixtureFunc) g_object_unref);

  g_free (path);
}

static int
compare_files (gconstpointer a, gconstpointer b)
{
  GFile *file1 = G_FILE (a);
  GFile *file2 = G_FILE (b);
  char *path1, *path2;
  int result;

  path1 = g_file_get_path (file1);
  path2 = g_file_get_path (file2);

  result = strcmp (path1, path2);

  g_free (path1);
  g_free (path2);

  return result;
}

static void
add_tests_for_files_in_directory (GFile *dir)
{
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GList *files;
  GError *error = NULL;

  enumerator = g_file_enumerate_children (dir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &error);
  g_assert_no_error (error);
  files = NULL;

  while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)))
    {
      const char *filename;

      filename = g_file_info_get_name (info);

      if (!g_str_has_suffix (filename, ".json") ||
          g_str_has_suffix (filename, ".out.json") ||
          g_str_has_suffix (filename, ".ref.json"))
        {
          g_object_unref (info);
          continue;
        }

      files = g_list_prepend (files, g_file_get_child (dir, filename));

      g_object_unref (info);
    }
  
  g_assert_no_error (error);
  g_object_unref (enumerator);

  files = g_list_sort (files, compare_files);
  g_list_foreach (files, (GFunc) add_test_for_file, NULL);
  g_list_free_full (files, g_object_unref);
}

int
main (int argc, char **argv)
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "");

  if (argc < 2)
    {
      const char *basedir;
      GFile *dir;

      basedir = g_test_get_dir (G_TEST_DIST);
      dir = g_file_new_for_path (basedir);
      add_tests_for_files_in_directory (dir);

      g_object_unref (dir);
    }
  else if (strcmp (argv[1], "--generate") == 0)
    {
      if (argc >= 3)
        {
          GFile *file = g_file_new_for_commandline_arg (argv[2]);

          parse_json_file (file, TRUE);

          g_object_unref (file);
        }
    }
  else
    {
      guint i;

      for (i = 1; i < argc; i++)
        {
          GFile *file = g_file_new_for_commandline_arg (argv[i]);

          add_test_for_file (file);

          g_object_unref (file);
        }
    }

  return g_test_run ();
}


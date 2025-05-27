#include <gtk/gtk.h>
#include <stdio.h>
#include "data_saver.h"

/**
 * @brief Saves a GdkPixbuf to a PNG file.
 *
 * @param pixbuf Pointer to the GdkPixbuf to be saved.
 * @param filename Path and name of the output PNG file.
 * @return TRUE on success, FALSE on error.
 */
gboolean save_pixbuf_to_png(GdkPixbuf *pixbuf, const char *filename) {
    GError *error = NULL;

    if (!pixbuf) {
        fprintf(stderr, "Error: Cannot save, pixbuf is NULL.\n");
        return FALSE;
    }

    g_print("Saving to file: %s\n", filename);

    // Save the pixbuf to a PNG file
    gboolean result = gdk_pixbuf_save(pixbuf, filename, "png", &error, NULL);

    if (!result) {
        fprintf(stderr, "Error saving PNG '%s': %s\n", filename, error->message);
        g_error_free(error);
        return result;
    }

    g_print("Successfully saved GdkPixbuf to file: %s\n", filename);
    return result;
}
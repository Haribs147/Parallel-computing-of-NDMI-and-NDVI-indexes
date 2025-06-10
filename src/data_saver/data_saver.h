#ifndef DATA_SAVER_H
#define DATA_SAVER_H

#include <gtk/gtk.h>

/**
 * @brief Saves a GdkPixbuf to a PNG file.
 *
 * @param pixbuf Pointer to the GdkPixbuf to be saved.
 * @param filename Path and name of the output PNG file.
 * @return TRUE on success, FALSE on error.
 */
gboolean save_pixbuf_to_png(GdkPixbuf* pixbuf, const char* filename);

#endif // DATA_SAVER_H

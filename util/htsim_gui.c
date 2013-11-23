/*
 * This program allows simulating a Heater device.
 */

#include "wbus_server.h"

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <glib.h>
#include <glib/gprintf.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>


static int shmid;
static heater_state_t *hs;

G_MODULE_EXPORT void
on_window1_delete_event(GtkWidget *widget,
                         GdkEvent  *event,
                         gpointer   user_data) 
{
    gtk_main_quit();
}

G_MODULE_EXPORT void
on_hscrollbar1_value_changed(GtkRange *range, gpointer  user_data)
{
  hs->volatile_data.sensor[0] = (gint)gtk_range_get_value(range);
}
G_MODULE_EXPORT void
on_hscrollbar2_value_changed(GtkRange *range, gpointer  user_data)
{
  hs->volatile_data.sensor[1] = (gint)gtk_range_get_value(range);
}
G_MODULE_EXPORT void
on_hscrollbar3_value_changed(GtkRange *range, gpointer  user_data)
{
  hs->volatile_data.sensor[2] = (gint)gtk_range_get_value(range);
}
G_MODULE_EXPORT void
on_hscrollbar4_value_changed(GtkRange *range, gpointer  user_data)
{
  hs->volatile_data.sensor[3] = (gint)gtk_range_get_value(range);
}
G_MODULE_EXPORT void
on_hscrollbar5_value_changed(GtkRange *range, gpointer  user_data)
{
  hs->volatile_data.sensor[4] = (gint)gtk_range_get_value(range);
}
G_MODULE_EXPORT void
on_hscrollbar6_value_changed(GtkRange *range, gpointer  user_data)
{
  hs->volatile_data.sensor[5] = (gint)gtk_range_get_value(range);
}
G_MODULE_EXPORT void
on_hscrollbar7_value_changed(GtkRange *range, gpointer  user_data)
{
  hs->volatile_data.sensor[6] = (gint)gtk_range_get_value(range);
}
G_MODULE_EXPORT void
on_hscrollbar8_value_changed(GtkRange *range, gpointer  user_data)
{
  hs->volatile_data.sensor[7] = (gint)gtk_range_get_value(range);
}
G_MODULE_EXPORT void
on_hscrollbar9_value_changed(GtkRange *range, gpointer  user_data)
{
  hs->volatile_data.sensor[8] = (gint)gtk_range_get_value(range);
}
G_MODULE_EXPORT void
on_hscrollbar10_value_changed(GtkRange *range, gpointer  user_data)
{
  hs->volatile_data.sensor[9] = (gint)gtk_range_get_value(range);
}


static
gboolean cbUpdateAct (gpointer data)
{
  GladeXML *xml = (GladeXML *)data;
  GtkLabel *label;
  gint i;
  gchar lname[64], val[16];

  for (i=1; i<=NUM_ACT; i++) {
    g_sprintf(lname, "label%d", i);
    label = GTK_LABEL(glade_xml_get_widget(xml, lname));
    g_sprintf(val, "%d", hs->volatile_data.act[i-1]);
    gtk_label_set_text(label, val);
  }

  return TRUE;
}

static
void updateSensor (GladeXML *xml)
{
  GtkRange *range;
  gint i;
  gchar lname[64];

  for (i=1; i<=NUM_SENSOR; i++) {
    g_sprintf(lname, "hscrollbar%d", i);
    range = GTK_RANGE(glade_xml_get_widget(xml, lname));
    gtk_range_set_value(range, (gdouble)hs->volatile_data.sensor[i]);
  }
}



int
main(int argc, char **argv)
{
    GladeXML *xml;
    GtkWidget *widget;

    gtk_init(&argc, &argv);
    xml = glade_xml_new("htsim.glade", NULL, NULL);

    widget = glade_xml_get_widget(xml, "window1");

    glade_xml_signal_autoconnect(xml);

    shmid = shmget(0xb0E1, 4096, 0666 | IPC_CREAT);
    if (shmid == -1) {
      g_message("shmget() failed");
      return -1;
    }
    hs = shmat(shmid, (void *)0, 0);
    if ((int)hs == -1) {
      g_message("shmat() failed");
      return -1;
    }

    updateSensor(xml);    

    g_timeout_add(500, cbUpdateAct, xml);

    gtk_widget_show (widget);

    gtk_main();

    return 0;
}

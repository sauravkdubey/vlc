/*****************************************************************************
 * callbacks.c : Callbacks for the Familiar Linux Gtk+ plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: callbacks.c,v 1.4 2002/08/14 21:50:01 jpsaman Exp $
 *
 * Authors: Jean-Paul Saman <jpsaman@wxs.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <sys/types.h>                                              /* off_t */
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>

#include <unistd.h>
#include <string.h>
#include <dirent.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "familiar.h"

/*#include "netutils.h"*/

static void MediaURLOpenChanged( GtkWidget *widget, gchar *psz_url );
static void PreferencesURLOpenChanged( GtkEditable *editable, gpointer user_data );

/*****************************************************************************
 * Useful function to retrieve p_intf
 ****************************************************************************/
void * __GtkGetIntf( GtkWidget * widget )
{
    void *p_data;

    if( GTK_IS_MENU_ITEM( widget ) )
    {
        /* Look for a GTK_MENU */
        while( widget->parent && !GTK_IS_MENU( widget ) )
        {
            widget = widget->parent;
        }

        /* Maybe this one has the data */
        p_data = gtk_object_get_data( GTK_OBJECT( widget ), "p_intf" );
        if( p_data )
        {
            return p_data;
        }

        /* Otherwise, the parent widget has it */
        widget = gtk_menu_get_attach_widget( GTK_MENU( widget ) );
    }

    /* We look for the top widget */
    widget = gtk_widget_get_toplevel( GTK_WIDGET( widget ) );

    p_data = gtk_object_get_data( GTK_OBJECT( widget ), "p_intf" );

    return p_data;
}

/*****************************************************************************
 * Helper functions for URL changes in Media and Preferences notebook pages.
 ****************************************************************************/
static void MediaURLOpenChanged( GtkWidget *widget, gchar *psz_url )
{
    intf_thread_t *p_intf = GtkGetIntf( widget );
    playlist_t *p_playlist;

    g_print( "%s\n",psz_url );

    // Add p_url to playlist .... but how ?
    if (p_intf)
    {
        p_playlist = (playlist_t *)
                 vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
        if( p_playlist )
        {
           playlist_Add( p_playlist, (char*)psz_url,
                         PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
           vlc_object_release( p_playlist );
        }
    }
}

static void PreferencesURLOpenChanged( GtkEditable *editable, gpointer user_data )
{
    gchar *       p_url;

    p_url = gtk_entry_get_text(GTK_ENTRY(editable) );
    g_print( "%s\n",p_url );
}

/*****************************************************************
 * Read directory helper function.
 ****************************************************************/
void ReadDirectory( GtkCList *clist, char *psz_dir)
{
    struct dirent **namelist;
    int n,i;

    if (psz_dir)
       n = scandir(psz_dir, &namelist, 0, NULL);
    else
       n = scandir(".", &namelist, 0, NULL);

    if (n<0)
        perror("scandir");
    else
    {
        gchar *ppsz_text[2];

        gtk_clist_freeze( clist );
        gtk_clist_clear( clist );

        for (i=0; i<n; i++)
        {
            /* This is a list of strings. */
            ppsz_text[0] = namelist[i]->d_name;
            if (namelist[i]->d_type)
                ppsz_text[1] = "dir";
            else
                ppsz_text[1] = "file";
            gtk_clist_insert( clist, i, ppsz_text );
            free(namelist[i]);
        }
        free(namelist);
        gtk_clist_thaw( clist );
    }
}


/*
 * Main interface callbacks
 */

gboolean GtkExit( GtkWidget       *widget,
                  gpointer         user_data )
{
    intf_thread_t *p_intf = GtkGetIntf( widget );

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_vlc->b_die = VLC_TRUE;
    vlc_mutex_unlock( &p_intf->change_lock );

    return TRUE;
}

gboolean
on_familiar_destroy_event              (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    GtkExit( GTK_WIDGET( widget ), user_data );
    return TRUE;
}


void
on_toolbar_open_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );
    GtkCList *clistmedia = NULL;

    if (p_intf)
    {
        /* Testing routine */
        clistmedia = GTK_CLIST( lookup_widget( p_intf->p_sys->p_window,
                                   "clistmedia") );
        if (GTK_CLIST(clistmedia))
        {
            ReadDirectory(clistmedia, ".");
        }
        gtk_widget_show( GTK_WIDGET(p_intf->p_sys->p_notebook) );
        gdk_window_raise( p_intf->p_sys->p_window->window );
    }
}


void
on_toolbar_preferences_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );
    if (p_intf) {
        gtk_widget_show( GTK_WIDGET(p_intf->p_sys->p_notebook) );
        gdk_window_raise( p_intf->p_sys->p_window->window );
    }
}


void
on_toolbar_rewind_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );

    if( p_intf )
    {
        if( p_intf->p_sys->p_input )
        {
            input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_SLOWER );
        }
    }
}


void
on_toolbar_pause_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );

    if( p_intf )
    {
        if( p_intf->p_sys->p_input )
        {
            input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PAUSE );
        }
    }
}


void
on_toolbar_play_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        if( p_intf )
        {
           gtk_widget_show( GTK_WIDGET(p_intf->p_sys->p_notebook) );
           gdk_window_raise( p_intf->p_sys->p_window->window );
        }
        /* Display open page */
    }

    /* If the playlist is empty, open a file requester instead */
    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_playlist->i_size )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        playlist_Play( p_playlist );
        vlc_object_release( p_playlist );
        gdk_window_lower( p_intf->p_sys->p_window->window );
    }
    else
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );
        /* Display open page */
    }
}


void
on_toolbar_stop_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist)
    {
        playlist_Stop( p_playlist );
        vlc_object_release( p_playlist );
        gdk_window_raise( p_intf->p_sys->p_window->window );
    }
}


void
on_toolbar_forward_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *  p_intf = GtkGetIntf( button );

    if( p_intf )
    {
        if( p_intf->p_sys->p_input )
        {
            input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_FASTER );
        }
    }
}


void
on_toolbar_about_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf = GtkGetIntf( button );
    if (p_intf)
    {// Toggle notebook
        if (p_intf->p_sys->p_notebook)
        {
/*        if ( gtk_get_data(  GTK_WIDGET(p_intf->p_sys->p_notebook), "visible" ) )
 *           gtk_widget_hide( GTK_WIDGET(p_intf->p_sys->p_notebook) );
 *        else
 */           gtk_widget_show( GTK_WIDGET(p_intf->p_sys->p_notebook) );
        }
        gdk_window_raise( p_intf->p_sys->p_window->window );
    }
}


void
on_comboURL_entry_changed              (GtkEditable     *editable,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf = GtkGetIntf( editable );
    gchar *       psz_url;

    if (p_intf)
    {
        psz_url = gtk_entry_get_text(GTK_ENTRY(editable));
        MediaURLOpenChanged( GTK_WIDGET(editable), psz_url );
    }
}


void
on_comboPrefs_entry_changed            (GtkEditable     *editable,
                                        gpointer         user_data)
{
    intf_thread_t * p_intf = GtkGetIntf( editable );

    if (p_intf)
    {
        PreferencesURLOpenChanged( editable, NULL );
    }
}


void
on_clistmedia_click_column             (GtkCList        *clist,
                                        gint             column,
                                        gpointer         user_data)
{
    static GtkSortType sort_type = GTK_SORT_ASCENDING;

    switch(sort_type)
    {
        case GTK_SORT_ASCENDING:
            sort_type = GTK_SORT_DESCENDING;
            break;
        case GTK_SORT_DESCENDING:
            sort_type = GTK_SORT_ASCENDING;
            break;
    }
    gtk_clist_freeze( clist );
    gtk_clist_set_sort_type( clist, sort_type );
    gtk_clist_sort( clist );
    gtk_clist_thaw( clist );
}


void
on_clistmedia_select_row               (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    gchar *text[2];
    gint ret;

    ret = gtk_clist_get_text (clist, row, 0, text);
    if (ret)
    {
        MediaURLOpenChanged( GTK_WIDGET(clist), text[0] );

//        /* DO NOT TRY THIS CODE IT SEGFAULTS */
//        g_print( "dir\n");
//        /* should be a gchar compare function */
//        if (strlen(text[1])>0)
//        {
//            g_print( "checking dir\n");
//            /* should be a gchar compare function */
//            if (strncmp(text[1],"dir",3)==0)
//            {
//                g_print( "dir: %s\n", text[0]);
//                ReadDirectory(clist, text[0]);
//            }
//            else
//            {
//                g_print( "playing file\n");
//                MediaURLOpenChanged( GTK_WIDGET(clist), text[0] );
//            }
//        }
//        else
//        {
//            g_print( "playing filer\n");
//            MediaURLOpenChanged( GTK_WIDGET(clist), text[0] );
//        }
    }
}


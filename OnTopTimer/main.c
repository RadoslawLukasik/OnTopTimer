#include <gtk/gtk.h>
#include <glib/gi18n.h>
#ifdef __linux__
    #define DATADIR "/usr/share/OnTopTimer/locale"
#elif _WIN32
    #define DATADIR ""
#endif

GtkWidget *win, *opthpos, *optvpos, *optfsize, *optfcol, *optbcol, *optafcol, *optabcol, *optbNoSec, *optbStoper, *optbClockAlarm, *optbStoperAlarm;
GtkWidget *optStoperAlarmH, *optStoperAlarmM, *optStoperAlarmS, *optClockAlarmH, *optClockAlarmM, *optClockAlarmS, *optAlarmLifeSpan;
char *pathopt;
cairo_t *tcr;
cairo_surface_t *surface;
cairo_text_extents_t extents;
gboolean bOptWin = FALSE;
gboolean ClockAlarmState = 0;
gint StoperState = 0; // 0 off, 1 on, 2 pause
gint64 StoperVal = 0;
gint64 StoperTime = 0;
_Atomic gint InDraw = 0;
GdkRectangle workarea;

gchar szTime[] = "00:00:00";

typedef struct Options {
    GdkRGBA fcol;// font color
    GdkRGBA bcol;// background color
    GdkRGBA afcol;// alarm font color
    GdkRGBA abcol;// alarm background color
    gdouble fsize;// font size
    gdouble hpos;// horizontal position on the screen
    gdouble vpos;// vertical position on the screen
    gint64 clockAlarmTime;// in us
    gint64 stoperAlarmTime;// in us
    gint64 alarmLifeSpan;// in us
    gboolean bNoSec;// hide seconds?
    gboolean bStoper;// 0 - clock, 1 - stopwatch
    gboolean bClockAlarm;// is an alarm turn on for the clock
    gboolean bStoperAlarm;// is an alarm turn on for the stopwatch
} Options;

gint w_w, w_h;

Options opt = {{0.0,1.0,0.0,1.0},{1.0,1.0,1.0,0.05},{0.0,1.0,1.0,1.0},{1.0,0.0,0.0,1.0},64.0,100.0,4.0,0,0,300000000,TRUE,FALSE,FALSE,FALSE};

void opt_save() {
    GFile *gf = g_file_new_for_path( pathopt );
    if ( gf ) {
        GFileOutputStream * gfos = g_file_replace( gf, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL );
        if ( gfos ) {
            g_output_stream_write( G_OUTPUT_STREAM(gfos), &opt, sizeof(opt), NULL, NULL );
            g_object_unref( gfos );
        }
        g_object_unref( gf );
    }
}

void opt_load() {
    gchar *home = g_build_filename( g_get_home_dir(), ".OnTopTimer", NULL );
    pathopt = g_build_filename( home, "options.dat", NULL );
    GError *error = NULL;
    GFile *gf = g_file_new_for_path( pathopt );
    if ( gf ) {
        GFileInputStream * gfis = g_file_read( gf, NULL, &error );
        if ( error ){
            if ( error->code == G_IO_ERROR_NOT_FOUND ) {
                g_clear_error( &error );
                GFile *dir = g_file_new_for_path( home );
                if ( g_file_make_directory_with_parents( dir, NULL, &error ) || error->code == G_IO_ERROR_EXISTS ) {
                    g_clear_error( &error );
                    opt_save();
                }
                g_clear_error( &error );
                g_object_unref( dir );
            } else {
                g_clear_error( &error );
            }
        } else {
            Options tmp;
            if(g_input_stream_read( G_INPUT_STREAM(gfis), &tmp, sizeof(opt), NULL, NULL ) == sizeof(opt))
                opt = tmp;// different size means another version of OnTopTimer
            g_object_unref( gfis );
        }
        g_object_unref( gf );
    }
    g_free( home );
}

void col_chg(GtkColorChooser *self, gpointer data) {
    gint i = data - NULL;
    switch (i) {
        case 0:
            gtk_color_chooser_get_rgba(self,&opt.fcol);
            break;
        case 1:
            gtk_color_chooser_get_rgba(self,&opt.bcol);
            break;
        case 2:
            gtk_color_chooser_get_rgba(self,&opt.afcol);
            break;
        case 3:
            gtk_color_chooser_get_rgba(self,&opt.abcol);
    }
}

void destroy( GtkWidget *self, gpointer data ) {
    cairo_destroy( tcr );
    cairo_surface_destroy( surface );
    gtk_main_quit();
}

void opt_destroy( GtkWidget *self, gpointer data ) {
    bOptWin = FALSE;
}

void opt_ok( GtkWidget *self, gpointer data ) {
    opt_save();
    gtk_window_close(GTK_WINDOW(data));
}

void opt_cancel( GtkWidget *self, gpointer data ) {
    gtk_window_close(GTK_WINDOW(data));
}

void move_window(){
    gtk_window_resize(GTK_WINDOW(win),extents.x_advance+8,2*opt.fsize-extents.height);
    w_w = extents.x_advance+8;
    w_h = 2*opt.fsize-extents.height;
    gtk_window_move(GTK_WINDOW(win),workarea.x+(gint)((workarea.width-w_w)*opt.hpos/100),workarea.y+(gint)((workarea.height-w_h)*opt.vpos/100));
}

void size_chg (GtkWidget *self, gpointer data){
    while(InDraw) g_usleep( 1000 );
    InDraw = 2;
    gint i = data - NULL;
    switch(i) {
    case 0:
        opt.fsize = gtk_spin_button_get_value( GTK_SPIN_BUTTON(self) );
        cairo_set_font_size (tcr, opt.fsize);
        cairo_text_extents (tcr, szTime, &extents);
        if(extents.x_advance + 8 > 0.25*workarea.width){
            opt.fsize *= ((0.25*workarea.width-8.0) / extents.x_advance);
            cairo_set_font_size (tcr, opt.fsize);
            cairo_text_extents (tcr, szTime, &extents);
        }
        break;
    case 1:
        opt.hpos = gtk_spin_button_get_value( GTK_SPIN_BUTTON(self) );
        break;
    case 2:
        opt.vpos = gtk_spin_button_get_value( GTK_SPIN_BUTTON(self) );
        break;
    case 3:
        opt.bNoSec = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(self) );
        if ( opt.bNoSec ) szTime[5] = 0;
        else szTime[5] = ':';
        cairo_text_extents( tcr, szTime, &extents );
        if(extents.x_advance + 8 > 0.25*workarea.width){
            opt.fsize *= ((0.25*workarea.width-8.0) / extents.x_advance);
            cairo_set_font_size (tcr, opt.fsize);
            cairo_text_extents (tcr, szTime, &extents);
        }
        break;
    case 4:
        opt.bStoper = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(self) );
        if ( opt.bStoper ) {
            StoperVal = 0;
            StoperState = 0;
        }
        break;
    case 5:
        opt.stoperAlarmTime = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(optStoperAlarmH) );
        opt.stoperAlarmTime *= 60;
        opt.stoperAlarmTime += gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(optStoperAlarmM) );
        opt.stoperAlarmTime *= 60;
        opt.stoperAlarmTime += gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(optStoperAlarmS) );
        opt.stoperAlarmTime *= 1000000;
        break;
    case 6:
        opt.clockAlarmTime = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(optClockAlarmH) );
        opt.clockAlarmTime *= 60;
        opt.clockAlarmTime += gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(optClockAlarmM) );
        opt.clockAlarmTime *= 60;
        opt.clockAlarmTime += gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(optClockAlarmS) );
        opt.clockAlarmTime *= 1000000;
        break;
    case 7:
        opt.bStoperAlarm = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(self) );
        break;
    case 8:
        opt.bClockAlarm = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(self) );
        break;
    case 9:
        opt.alarmLifeSpan = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(self) );
        opt.alarmLifeSpan *= 1000000;
    }
    if ( i < 4 ) move_window();
    InDraw = 0;
}

void create_options_window() {
    bOptWin = TRUE;
    GtkWidget *winopt = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_resizable(GTK_WINDOW(winopt),FALSE);
    gtk_window_set_title(GTK_WINDOW(winopt),_("Options"));
    g_signal_connect(winopt, "destroy", G_CALLBACK(opt_destroy), NULL );
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,1);
    gtk_container_add(GTK_CONTAINER(winopt),vbox);

    optbStoper = gtk_check_button_new_with_label(_("Stopwatch instead of clock"));
    gtk_box_pack_start(GTK_BOX(vbox),optbStoper,FALSE,FALSE,0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(optbStoper),opt.bStoper);
    g_signal_connect(optbStoper, "toggled", G_CALLBACK (size_chg), NULL+4 );

    optbNoSec = gtk_check_button_new_with_label(_("No seconds"));
    gtk_box_pack_start(GTK_BOX(vbox),optbNoSec,FALSE,FALSE,0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(optbNoSec),opt.bNoSec);
    g_signal_connect(optbNoSec, "toggled", G_CALLBACK (size_chg), NULL+3 );

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,1);
    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
    GtkWidget *lbl = gtk_label_new(_("Font size"));
    gtk_box_pack_start(GTK_BOX(hbox),lbl,FALSE,FALSE,0);
    optfsize = gtk_spin_button_new_with_range(8.0,128.0,1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(optfsize),opt.fsize);
    g_signal_connect(optfsize, "value-changed", G_CALLBACK (size_chg), NULL );
    gtk_box_pack_end(GTK_BOX(hbox),optfsize,FALSE,FALSE,0);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,1);
    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
    lbl = gtk_label_new(_("Horizontal position (%)"));
    gtk_box_pack_start(GTK_BOX(hbox),lbl,FALSE,FALSE,0);
    opthpos = gtk_spin_button_new_with_range(0.0,100.0,0.1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(opthpos),opt.hpos);
    g_signal_connect(opthpos, "value-changed", G_CALLBACK (size_chg), NULL+1 );
    gtk_box_pack_end(GTK_BOX(hbox),opthpos,FALSE,FALSE,0);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,1);
    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
    lbl = gtk_label_new(_("Vertical position (%)"));
    gtk_box_pack_start(GTK_BOX(hbox),lbl,FALSE,FALSE,0);
    optvpos = gtk_spin_button_new_with_range(0.0,100.0,0.1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(optvpos),opt.vpos);
    g_signal_connect(optvpos, "value-changed", G_CALLBACK (size_chg), NULL+2 );
    gtk_box_pack_end(GTK_BOX(hbox),optvpos,FALSE,FALSE,0);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,1);
    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
    lbl = gtk_label_new(_("Font color"));
    gtk_box_pack_start(GTK_BOX(hbox),lbl,FALSE,FALSE,0);
    optfcol = gtk_color_button_new_with_rgba(&opt.fcol);
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(optfcol),TRUE);
    g_object_set(G_OBJECT(optfcol),"show-editor",TRUE,NULL);
    g_signal_connect(optfcol, "color-set", G_CALLBACK (col_chg), NULL );
    gtk_box_pack_end(GTK_BOX(hbox),optfcol,FALSE,FALSE,0);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,1);
    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
    lbl = gtk_label_new(_("Background color"));
    gtk_box_pack_start(GTK_BOX(hbox),lbl,FALSE,FALSE,0);
    optbcol = gtk_color_button_new_with_rgba(&opt.bcol);
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(optbcol),TRUE);
    g_object_set(G_OBJECT(optbcol),"show-editor",TRUE,NULL);
    g_signal_connect(optbcol, "color-set", G_CALLBACK (col_chg), NULL+1 );
    gtk_box_pack_end(GTK_BOX(hbox),optbcol,FALSE,FALSE,0);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,1);
    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
    lbl = gtk_label_new(_("Alarm font color"));
    gtk_box_pack_start(GTK_BOX(hbox),lbl,FALSE,FALSE,0);
    optafcol = gtk_color_button_new_with_rgba(&opt.afcol);
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(optafcol),TRUE);
    g_object_set(G_OBJECT(optafcol),"show-editor",TRUE,NULL);
    g_signal_connect(optafcol, "color-set", G_CALLBACK (col_chg), NULL+2 );
    gtk_box_pack_end(GTK_BOX(hbox),optafcol,FALSE,FALSE,0);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,1);
    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
    lbl = gtk_label_new(_("Alarm background color"));
    gtk_box_pack_start(GTK_BOX(hbox),lbl,FALSE,FALSE,0);
    optabcol = gtk_color_button_new_with_rgba(&opt.abcol);
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(optabcol),TRUE);
    g_object_set(G_OBJECT(optabcol),"show-editor",TRUE,NULL);
    g_signal_connect(optabcol, "color-set", G_CALLBACK (col_chg), NULL+3 );
    gtk_box_pack_end(GTK_BOX(hbox),optabcol,FALSE,FALSE,0);

    optbStoperAlarm = gtk_check_button_new_with_label(_("Stopwatch alarm"));
    gtk_box_pack_start(GTK_BOX(vbox),optbStoperAlarm,FALSE,FALSE,0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(optbStoperAlarm),opt.bStoperAlarm);
    g_signal_connect(optbStoperAlarm, "toggled", G_CALLBACK (size_chg), NULL+7 );

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,1);
    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
    lbl = gtk_label_new(_("Stopwatch alarm time (H:M:S)"));
    gtk_box_pack_start(GTK_BOX(hbox),lbl,FALSE,FALSE,0);
    optStoperAlarmH = gtk_spin_button_new_with_range(0.0,99.0,1);
    optStoperAlarmM = gtk_spin_button_new_with_range(0.0,59.0,1);
    optStoperAlarmS = gtk_spin_button_new_with_range(0.0,59.0,1);
    guint64 val = opt.stoperAlarmTime / 1000000;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(optStoperAlarmS),val % 60);
    val /= 60;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(optStoperAlarmM),val % 60);
    val /= 60;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(optStoperAlarmH),val % 100);
    g_signal_connect(optStoperAlarmH, "value-changed", G_CALLBACK (size_chg), NULL+5 );
    g_signal_connect(optStoperAlarmM, "value-changed", G_CALLBACK (size_chg), NULL+5 );
    g_signal_connect(optStoperAlarmS, "value-changed", G_CALLBACK (size_chg), NULL+5 );
    gtk_spin_button_set_snap_to_ticks( GTK_SPIN_BUTTON(optStoperAlarmS), TRUE );
    gtk_spin_button_set_snap_to_ticks( GTK_SPIN_BUTTON(optStoperAlarmM), TRUE );
    gtk_spin_button_set_snap_to_ticks( GTK_SPIN_BUTTON(optStoperAlarmH), TRUE );
    gtk_orientable_set_orientation( GTK_ORIENTABLE(optStoperAlarmS), GTK_ORIENTATION_VERTICAL );
    gtk_orientable_set_orientation( GTK_ORIENTABLE(optStoperAlarmM), GTK_ORIENTATION_VERTICAL );
    gtk_orientable_set_orientation( GTK_ORIENTABLE(optStoperAlarmH), GTK_ORIENTATION_VERTICAL );
    gtk_box_pack_start(GTK_BOX(hbox),optStoperAlarmH,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(hbox),optStoperAlarmM,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(hbox),optStoperAlarmS,FALSE,FALSE,0);

    optbClockAlarm = gtk_check_button_new_with_label(_("Clock alarm"));
    gtk_box_pack_start(GTK_BOX(vbox),optbClockAlarm,FALSE,FALSE,0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(optbClockAlarm),opt.bClockAlarm);
    g_signal_connect(optbClockAlarm, "toggled", G_CALLBACK (size_chg), NULL+8 );

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,1);
    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
    lbl = gtk_label_new(_("Clock alarm time (H:M:S)"));
    gtk_box_pack_start(GTK_BOX(hbox),lbl,FALSE,FALSE,0);
    optClockAlarmH = gtk_spin_button_new_with_range(0.0,23.0,1);
    optClockAlarmM = gtk_spin_button_new_with_range(0.0,59.0,1);
    optClockAlarmS = gtk_spin_button_new_with_range(0.0,59.0,1);
    val = opt.clockAlarmTime / 1000000;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(optClockAlarmS),val % 60);
    val /= 60;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(optClockAlarmM),val % 60);
    val /= 60;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(optClockAlarmH),val % 24);
    g_signal_connect(optClockAlarmH, "value-changed", G_CALLBACK (size_chg), NULL+6 );
    g_signal_connect(optClockAlarmM, "value-changed", G_CALLBACK (size_chg), NULL+6 );
    g_signal_connect(optClockAlarmS, "value-changed", G_CALLBACK (size_chg), NULL+6 );
    gtk_spin_button_set_snap_to_ticks( GTK_SPIN_BUTTON(optClockAlarmS), TRUE );
    gtk_spin_button_set_snap_to_ticks( GTK_SPIN_BUTTON(optClockAlarmM), TRUE );
    gtk_spin_button_set_snap_to_ticks( GTK_SPIN_BUTTON(optClockAlarmH), TRUE );
    gtk_orientable_set_orientation( GTK_ORIENTABLE(optClockAlarmS), GTK_ORIENTATION_VERTICAL );
    gtk_orientable_set_orientation( GTK_ORIENTABLE(optClockAlarmM), GTK_ORIENTATION_VERTICAL );
    gtk_orientable_set_orientation( GTK_ORIENTABLE(optClockAlarmH), GTK_ORIENTATION_VERTICAL );
    gtk_box_pack_start(GTK_BOX(hbox),optClockAlarmH,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(hbox),optClockAlarmM,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(hbox),optClockAlarmS,FALSE,FALSE,0);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,1);
    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
    lbl = gtk_label_new(_("Clock alarm duration (in seconds)"));
    gtk_box_pack_start(GTK_BOX(hbox),lbl,FALSE,FALSE,0);
    optAlarmLifeSpan = gtk_spin_button_new_with_range(0.0,86400.0,1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(optAlarmLifeSpan),opt.alarmLifeSpan/1000000);
    g_signal_connect(optAlarmLifeSpan, "value-changed", G_CALLBACK (size_chg), NULL+9 );
    gtk_spin_button_set_snap_to_ticks( GTK_SPIN_BUTTON(optAlarmLifeSpan), TRUE );
    gtk_box_pack_end(GTK_BOX(hbox),optAlarmLifeSpan,FALSE,FALSE,0);

    lbl = gtk_label_new(_("RMB opens the options window\nLMB start/pause the stopwatch\nMMB resetting the stopwatch/hide the clock's alarm\nCtrl+RMB close the program"));
    gtk_box_pack_start(GTK_BOX(vbox),lbl,FALSE,FALSE,0);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,1);
    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
    GtkWidget *btn = gtk_button_new_with_label(_("Save"));
    gtk_box_pack_start(GTK_BOX(hbox),btn,FALSE,FALSE,0);
    g_signal_connect( btn, "clicked", G_CALLBACK (opt_ok), winopt );
    btn = gtk_button_new_with_label( _("Cancel") );
    gtk_box_pack_end( GTK_BOX(hbox), btn, FALSE, FALSE, 0 );
    g_signal_connect( btn, "clicked", G_CALLBACK (opt_cancel), winopt );
    gtk_widget_show_all(winopt);
    gtk_window_set_keep_above(GTK_WINDOW(winopt),TRUE);
}

gboolean button_press( GtkWidget *self, GdkEventButton *event, gpointer data ) {
    if (event->button == 3) { // RMB
        if ( event->state & GDK_CONTROL_MASK ) {// Ctrl+RMB
            gtk_window_close(GTK_WINDOW(self));
        } else if (!bOptWin) {
            create_options_window();
        }
    } else if (event->button == 2) { // MMB
        if (opt.bStoper) {
            StoperTime = g_get_monotonic_time();
            StoperVal = 0;
        } else {
            if(ClockAlarmState) ClockAlarmState = 2;
        }
    } else if(event->button == 1){ // LMB
        if ( opt.bStoper ){
            if ( StoperState & 1 ){
                StoperState = 2;
            } else {
                StoperTime = g_get_monotonic_time();
                StoperState = 1;
            }
        }
    }
    return TRUE;
}

gboolean time_refresh(gpointer data){
    if ( InDraw ) return TRUE;
    static guint st_alarm = 0;
    GdkRGBA *bcol = &opt.bcol;
    GdkRGBA *fcol = &opt.fcol;
    InDraw = 1;
    if ( opt.bStoper ){
        gint64 tr;
        if ( StoperState & 1 ) {
            tr = g_get_monotonic_time();
            StoperVal +=  tr - StoperTime;
            StoperTime = tr;
        }
        tr = StoperVal / 1000000;
        if( (opt.bStoperAlarm) && (StoperVal >= opt.stoperAlarmTime)) {
            if(st_alarm) {
                bcol = &opt.abcol;
                fcol = &opt.afcol;
            }
            st_alarm ^= 1;
        }
        szTime[1] = ((tr/3600)%10) +48;
        szTime[0] = ((tr/36000)%10) + 48;
        szTime[4] = ((tr/60)%10) +48;
        szTime[3] = ((tr/600)%6) + 48;
        szTime[7] = (tr%10) +48;
        szTime[6] = ((tr/10)%6) + 48;
    } else {
        GDateTime *dt = g_date_time_new_now_local();
        gint m = g_date_time_get_minute( dt );
        gint h = g_date_time_get_hour( dt );
        gint s = g_date_time_get_second( dt );
        gint64 tr = h * 60; tr += m; tr *= 60; tr += s; tr *= 1000000;
        g_date_time_unref( dt );
        if (opt.bClockAlarm) {
            if (tr >= opt.clockAlarmTime) {
                if (tr < opt.clockAlarmTime + opt.alarmLifeSpan) {
                    if (ClockAlarmState == 0)
                        ClockAlarmState = 1;
                } else
                    ClockAlarmState = 0;
            }
        }
        if (ClockAlarmState == 1) {
            if(st_alarm) {
                bcol = &opt.abcol;
                fcol = &opt.afcol;
            }
            st_alarm ^= 1;
        }
        szTime[1] = (h%10) +48;
        szTime[0] = ((h/10)%10) + 48;
        szTime[4] = (m%10) +48;
        szTime[3] = ((m/10)%10) + 48;
        szTime[7] = (s%10) +48;
        szTime[6] = ((s/10)%10) + 48;
    }
    gdk_cairo_set_source_rgba( tcr, bcol );
    cairo_set_operator( tcr, CAIRO_OPERATOR_SOURCE );
    cairo_paint( tcr );
    gdk_cairo_set_source_rgba( tcr, fcol );
    cairo_set_operator( tcr, CAIRO_OPERATOR_OVER );
    cairo_move_to ( tcr, 4.0, opt.fsize );
    cairo_show_text( tcr, szTime );
    InDraw = 0;
    gtk_widget_queue_draw_area( GTK_WIDGET(data), 0, 0, w_w, w_h );
    return TRUE;
}

gboolean draw( GtkWidget* self, cairo_t* cr, gpointer data ){
    cairo_set_source_surface( cr, surface, 0, 0 );
    cairo_paint( cr );
    return FALSE;
}

int main(int argc, char **argv){
    gtk_init(&argc,&argv);
    setlocale( LC_ALL, "" );
    bindtextdomain( "OnTopTimer", DATADIR );
    bind_textdomain_codeset( "OnTopTimer", "UTF-8" );
    textdomain( "OnTopTimer" );
    opt_load();
    if ( opt.bNoSec ) {
        szTime[5] = 0;
    }
    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *monitor = gdk_display_get_primary_monitor(display);
    gdk_monitor_get_workarea (monitor,&workarea);

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0.25*workarea.width, 192);
    tcr = cairo_create(surface);
    gdk_cairo_set_source_rgba(tcr,&opt.bcol);
    cairo_paint(tcr);
    cairo_select_font_face(tcr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size (tcr, opt.fsize);
    cairo_text_extents (tcr, szTime, &extents);
    if(extents.x_advance + 8 > 0.25*workarea.width){
        opt.fsize *= ((0.25*workarea.width-8.0) / extents.x_advance);
        cairo_set_font_size (tcr, opt.fsize);
        cairo_text_extents (tcr, szTime, &extents);
    }
    gdk_cairo_set_source_rgba(tcr,&opt.fcol);
    cairo_set_operator(tcr,CAIRO_OPERATOR_OVER);
    cairo_move_to (tcr, 4.0,opt.fsize);
    cairo_show_text(tcr, szTime);

    win = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_decorated (GTK_WINDOW(win), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(win),extents.x_advance+8,2*opt.fsize-extents.height);
    gtk_widget_set_app_paintable(win,TRUE);
    gtk_widget_add_events(win, GDK_BUTTON_PRESS_MASK);
    gtk_widget_set_visual(win,gdk_screen_get_rgba_visual(gtk_window_get_screen(GTK_WINDOW(win))));

    g_timeout_add( 500, G_SOURCE_FUNC(time_refresh), win );
    g_signal_connect(win, "destroy", G_CALLBACK(destroy), NULL );
    g_signal_connect(win, "draw", G_CALLBACK (draw), NULL );
    g_signal_connect(win, "button-release-event", G_CALLBACK (button_press), NULL );
    gtk_widget_show_all(win);
    //gtk_window_set_gravity(GTK_WINDOW(win),GDK_GRAVITY_NORTH_WEST);
    gtk_window_set_keep_above(GTK_WINDOW(win),TRUE);
    move_window();
    gtk_main();
}

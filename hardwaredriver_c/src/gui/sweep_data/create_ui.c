#include "create_ui.h"

void create_ui(Tcl_Interp* interp) {
    // Create main menu
    Tcl_Eval(interp, "menu .mainmenu -tearoff 0");
    Tcl_Eval(interp, "menu .mainmenu.mm_clear -command {on_mainmenu_action} -label {Clear}");
    Tcl_Eval(interp, "menu .mainmenu.mm_about -command {on_mainmenu_action} -label {About}");
    Tcl_Eval(interp, "menu .mainmenu.mm_quit -command {on_mainmenu_action} -label {Quit}");
    Tcl_Eval(interp, ". configure -menu .mainmenu");

    // Create main window
    Tcl_Eval(interp, "toplevel .mainwindow -height 200 -width 200");
    Tcl_Eval(interp, "wm title .mainwindow {Myapp}");
    Tcl_Eval(interp, "frame .mainwindow.mw_fcontainer -height 200 -width 200 -padding 2");
    Tcl_Eval(interp, "grid .mainwindow.mw_fcontainer -column 0 -row 0 -sticky nsew");

    // Recording toolbar frame
    Tcl_Eval(interp, "frame .mainwindow.mw_fcontainer.recording_toolbar_frame -borderwidth 2 -height 20 -width 200");
    Tcl_Eval(interp, "grid .mainwindow.mw_fcontainer.recording_toolbar_frame -column 0 -row 0 -sticky n");
    Tcl_Eval(interp, "button .mainwindow.mw_fcontainer.recording_toolbar_frame.start_record_button -text {Start Recording}");
    Tcl_Eval(interp, "grid .mainwindow.mw_fcontainer.recording_toolbar_frame.start_record_button -column 0 -row 0");
    Tcl_Eval(interp, "button .mainwindow.mw_fcontainer.recording_toolbar_frame.stop_record_button -text {Stop Recording} -state disabled");
    Tcl_Eval(interp, "grid .mainwindow.mw_fcontainer.recording_toolbar_frame.stop_record_button -column 1 -row 0");
    Tcl_Eval(interp, "button .mainwindow.mw_fcontainer.recording_toolbar_frame.save_record_button -text {Save} -state disabled");
    Tcl_Eval(interp, "grid .mainwindow.mw_fcontainer.recording_toolbar_frame.save_record_button -column 2 -row 0");

    // Playback toolbar frame
    Tcl_Eval(interp, "frame .mainwindow.mw_fcontainer.playback_toolbar_frame -borderwidth 2 -height 200 -width 200");
    Tcl_Eval(interp, "grid .mainwindow.mw_fcontainer.playback_toolbar_frame -column 0 -row 1 -sticky n");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.playback_toolbar_frame.label1 -text {Redraw}");
    Tcl_Eval(interp, "grid .mainwindow.mw_fcontainer.playback_toolbar_frame.label1 -column 0 -row 1");
    Tcl_Eval(interp, "button .mainwindow.mw_fcontainer.playback_toolbar_frame.play_button -text {Play} -state disabled");
    Tcl_Eval(interp, "grid .mainwindow.mw_fcontainer.playback_toolbar_frame.play_button -column 1 -row 1");
    Tcl_Eval(interp, "button .mainwindow.mw_fcontainer.playback_toolbar_frame.pause_button -text {Pause} -state disabled");
    Tcl_Eval(interp, "grid .mainwindow.mw_fcontainer.playback_toolbar_frame.pause_button -column 2 -row 1");
    Tcl_Eval(interp, "button .mainwindow.mw_fcontainer.playback_toolbar_frame.clear_button -text {Clear}");
    Tcl_Eval(interp, "grid .mainwindow.mw_fcontainer.playback_toolbar_frame.clear_button -column 3 -row 1");
    Tcl_Eval(interp, "button .mainwindow.mw_fcontainer.playback_toolbar_frame.instruction_button -text {Instructions}");
    Tcl_Eval(interp, "grid .mainwindow.mw_fcontainer.playback_toolbar_frame.instruction_button -column 4 -row 1");

    // Frame for current status
    Tcl_Eval(interp, "frame .mainwindow.mw_fcontainer.frame2 -height 20 -width 200");
    Tcl_Eval(interp, "grid .mainwindow.mw_fcontainer.frame2 -column 0 -row 2 -sticky n");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.frame2.current_status");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.frame2.current_status -side top");

    // Main container area
    Tcl_Eval(interp, "frame .mainwindow.mw_fcontainer.mw_fmain -height 1000 -width 1000 -relief flat");
    Tcl_Eval(interp, "grid .mainwindow.mw_fcontainer.mw_fmain -column 0 -row 3 -sticky new");
    Tcl_Eval(interp, "frame .mainwindow.mw_fcontainer.mw_fmain.value_type_label -height 200 -width 200");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fmain.value_type_label -side left");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.mw_fmain.value_type_label.ap_pitch_label -text {A/P Pitch} -foreground blue -justify center");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fmain.value_type_label.ap_pitch_label -side top");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.mw_fmain.value_type_label.lateral_roll_label -text {Lateral Roll} -foreground red -justify center");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fmain.value_type_label.lateral_roll_label -side top");

    // Graph canvas
    Tcl_Eval(interp, "frame .mainwindow.mw_fcontainer.mw_fmain.main_container_area -height 200 -width 200");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fmain.main_container_area -side left");
    Tcl_Eval(interp, "canvas .mainwindow.mw_fcontainer.mw_fmain.main_container_area.graph_canvas -background #ffffff");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fmain.main_container_area.graph_canvas -side top");

    // Value label type frame
    Tcl_Eval(interp, "frame .mainwindow.mw_fcontainer.mw_fmain.value_label_type_frame -borderwidth 2 -height 200 -width 100 -relief groove");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fmain.value_label_type_frame -side left");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.mw_fmain.value_label_type_frame.a_flex_image_label");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fmain.value_label_type_frame.a_flex_image_label -side top");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.mw_fmain.value_label_type_frame.a_flex_label -text {A Flex} -justify center");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fmain.value_label_type_frame.a_flex_label -side top");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.mw_fmain.value_label_type_frame.p_ext_label -text {P Ext} -justify center");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fmain.value_label_type_frame.p_ext_label -side top");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.mw_fmain.value_label_type_frame.p_ext_image_label");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fmain.value_label_type_frame.p_ext_image_label -side top");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.mw_fmain.value_label_type_frame.r_flex_image_label");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fmain.value_label_type_frame.r_flex_image_label -side top");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.mw_fmain.value_label_type_frame.r_flex_label -text {R Flex} -justify center");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fmain.value_label_type_frame.r_flex_label -side top");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.mw_fmain.value_label_type_frame.l_flex_label -text {L Flex} -justify center");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fmain.value_label_type_frame.l_flex_label -side top");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.mw_fmain.value_label_type_frame.l_flex_image_label");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fmain.value_label_type_frame.l_flex_image_label -side top");

    // Table data frame container
    Tcl_Eval(interp, "frame .mainwindow.mw_fcontainer.mw_fmain.table_data_frame_container -height 200 -width 500 -relief raised");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fmain.table_data_frame_container -side right -fill both");

    // Bottom frame
    Tcl_Eval(interp, "frame .mainwindow.mw_fcontainer.mw_fbottom -borderwidth 1 -height 200 -width 200 -relief raised");
    Tcl_Eval(interp, "grid .mainwindow.mw_fcontainer.mw_fbottom -column 0 -row 5 -sticky sew");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.mw_fbottom.label3 -text {Gain}");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.label3 -side left");
    Tcl_Eval(interp, "ttk::combobox .mainwindow.mw_fcontainer.mw_fbottom.gain_control_combo -width 10");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.gain_control_combo -side left");
    Tcl_Eval(interp, "ttk::separator .mainwindow.mw_fcontainer.mw_fbottom.separator1 -orient vertical");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.separator1 -side left -fill both -padx 5");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.mw_fbottom.label4 -text {Space bar to START/STOP}");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.label4 -side left");
    Tcl_Eval(interp, "ttk::separator .mainwindow.mw_fcontainer.mw_fbottom.separator2 -orient vertical");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.separator2 -side left -fill both -padx 5");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.mw_fbottom.label5 -text {C to Clear}");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.label5 -side left");
    Tcl_Eval(interp, "ttk::separator .mainwindow.mw_fcontainer.mw_fbottom.separator3 -orient vertical");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.separator3 -side left -fill both -padx 5");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.mw_fbottom.label6 -text {F5 to Save}");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.label6 -side left");
    Tcl_Eval(interp, "ttk::separator .mainwindow.mw_fcontainer.mw_fbottom.separator4 -orient vertical");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.separator4 -side left -fill both -padx 5");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.mw_fbottom.label7 -text {I for Instructions}");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.label7 -side left");
    Tcl_Eval(interp, "ttk::separator .mainwindow.mw_fcontainer.mw_fbottom.separator5 -orient vertical");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.separator5 -side left -fill both -padx 5");
    Tcl_Eval(interp, "ttk::combobox .mainwindow.mw_fcontainer.mw_fbottom.scan_type_combo -width 10");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.scan_type_combo -side left");
    Tcl_Eval(interp, "ttk::separator .mainwindow.mw_fcontainer.mw_fbottom.separator6 -orient vertical");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.separator6 -side left -fill both -padx 5");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.mw_fbottom.label8 -text {Speed}");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.label8 -side left");
    Tcl_Eval(interp, "ttk::combobox .mainwindow.mw_fcontainer.mw_fbottom.speed_combo -width 10");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.speed_combo -side left");
    Tcl_Eval(interp, "ttk::separator .mainwindow.mw_fcontainer.mw_fbottom.separator7 -orient vertical");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.separator7 -side left -fill both -padx 5");
    Tcl_Eval(interp, "label .mainwindow.mw_fcontainer.mw_fbottom.label9 -text {Filter}");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.label9 -side left");
    Tcl_Eval(interp, "ttk::separator .mainwindow.mw_fcontainer.mw_fbottom.separator8 -orient vertical");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.separator8 -side left -fill both -padx 5");
    Tcl_Eval(interp, "ttk::combobox .mainwindow.mw_fcontainer.mw_fbottom.filter_combo -width 10");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.filter_combo -side left");
    Tcl_Eval(interp, "ttk::separator .mainwindow.mw_fcontainer.mw_fbottom.separator9 -orient vertical");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.separator9 -side left -fill both -padx 5");
    Tcl_Eval(interp, "button .mainwindow.mw_fcontainer.mw_fbottom.graph_button -text {Graph}");
    Tcl_Eval(interp, "pack .mainwindow.mw_fcontainer.mw_fbottom.graph_button -side left");

    // Graph plot window
    Tcl_Eval(interp, "toplevel .graph_plot_window -height 200 -width 200");
    Tcl_Eval(interp, "frame .graph_plot_window.frame1 -height 200 -width 200");
    Tcl_Eval(interp, "grid .graph_plot_window.frame1 -column 0 -row 0 -sticky nsew");
    Tcl_Eval(interp, "frame .graph_plot_window.frame1.frame3 -height 10 -width 200 -relief raised");
    Tcl_Eval(interp, "pack .graph_plot_window.frame1.frame3 -side top -fill x");
    Tcl_Eval(interp, "button .graph_plot_window.frame1.frame3.print_button -text {Print}");
    Tcl_Eval(interp, "pack .graph_plot_window.frame1.frame3.print_button -side left");
    Tcl_Eval(interp, "canvas .graph_plot_window.frame1.graph_plot_canvas");
    Tcl_Eval(interp, "pack .graph_plot_window.frame1.graph_plot_canvas -side left");
    Tcl_Eval(interp, "frame .graph_plot_window.frame1.table_data_frame_container_2 -height 200 -width 200 -relief raised");
    Tcl_Eval(interp, "pack .graph_plot_window.frame1.table_data_frame_container_2 -side left -fill both");
}
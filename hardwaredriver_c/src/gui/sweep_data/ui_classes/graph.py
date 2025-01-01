import json
import pathlib
import pickle
from datetime import datetime, timedelta
from os import path
from threading import Lock
from tkinter import Toplevel, Label, Button
from uuid import uuid4

import matplotlib.pyplot as plt
import numpy as np
from matplotlib import animation
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from pygubu import Builder

from MyotronicsDriver.hardware.functions.sweep_data.graph_plot_window import GraphPlotWindow
from MyotronicsDriver.hardware.functions.sweep_data.namespace_options import EVENT_TOGGLE_RECORDING, \
    EVENT_USER_RECORD_SAVED, EVENT_CMS_RECORDING_PLAYBACK, EVENT_CMS_START_PLAYBACK, EVENT_MARK_REDRAW_TOOL, \
    NamespaceOptions
from MyotronicsDriver.hardware.functions.sweep_data.tilt_windows.picture_window_reusable import PictureWindowReusable
from MyotronicsDriver.hardware.functions.sweep_data.ui_classes.data_class import DataClass, RecordingModes
from MyotronicsDriver.utils.windows_api import make_k7_window_active

GAIN_VALUES = ('15', '30', '45', '90')
SCAN_TYPE_VALUES = (
    RecordingModes.A_P_PITCH.value, RecordingModes.LAT_ROLL.value,
    RecordingModes.OTHER.value)
SPEED_VALUES = ('1.0', '2.0', '4.0')

NONE_FILTER_VALUE = "None"
FILTER_COMBO_VALUES = [NONE_FILTER_VALUE] + list(SCAN_TYPE_VALUES)


class Graph:
    __fig = None
    ani = None

    __recording_time = 16
    __gain_label = 45
    __scan_type_label = SCAN_TYPE_VALUES[0]
    __speed_label = "1.0"
    __scan_filter_type = NONE_FILTER_VALUE
    __patient_path = None

    # playback variables
    __requested_playback_file_name = None

    INSTRUCTIONS_TEXT_FILE_PATH = "C:\\K7\\python\\sweep_instructions.txt"

    __frontal_window = None
    __sagittal_window = None
    __picture_windows_only: bool = False

    __running = False
    __running_mutex = Lock()

    __temp_file_fd = None

    __show_sweep_graph = None

    @property
    def gain_label(self):
        return self.__gain_label

    def __set_gain_label(self, value):
        self.__gain_label = int(value)

    @property
    def scan_type_label(self):
        return self.__scan_type_label

    def __set_scan_type_label(self, value):
        self.__scan_type_label = value
        self.__setup_graph__(speed_label=self.__speed_label)

    @property
    def speed_label(self):
        return self.__speed_label

    def __set_speed_label(self, value):
        self.__speed_label = value
        self.__setup_graph__(speed_label=value)

    @property
    def scan_filter_type(self):
        return self.__scan_filter_type

    def __set_scan_filter_type(self, value):
        self.__scan_filter_type = value
        self.__filter_table_callback(value)

    def __init__(
            self, master, main_window, data_class: DataClass, builder: Builder,
            filter_table_callback, patient_path, patient_name, cms_callback,
            namespace: NamespaceOptions
    ):
        self.__master = master
        self.__main_window = main_window
        self.__data_class = data_class
        self.__builder = builder
        self.__setup_combo_values()
        self.__setup_buttons()
        self.__setup_graph__(speed_label=self.__speed_label)
        self.__filter_table_callback = filter_table_callback
        self.__patient_path = patient_path
        self.__patient_name = patient_name
        self.__namespace = namespace

        self.__cms_callback = cms_callback

        self.__frontal_window = PictureWindowReusable(
            is_frontal=True, patient_name=patient_name, main_window=main_window,
            image_window_options=data_class.image_window_options
        )
        self.__frontal_window.hide_window()
        self.__sagittal_window = PictureWindowReusable(
            is_frontal=False, patient_name=patient_name, main_window=main_window,
            image_window_options=data_class.image_window_options
        )
        self.__sagittal_window.hide_window()
        self.__main_window.withdraw()

        if self.__picture_windows_only:
            make_k7_window_active()

        self.__main_window_hidden = True
        self.__running = False

        self.__temp_file_fd = None

    def update_patient_path(self, new_patient_path):
        self.__patient_path = new_patient_path

    def update_patient_name(self, new_patient_name):
        if self.__sagittal_window:
            self.__sagittal_window.update_patient_name(new_patient_name)
        if self.__frontal_window:
            self.__frontal_window.update_patient_name(new_patient_name)

    def start(self, picture_windows_only, tilt_enabled):
        self.__show_sweep_graph = None
        with self.__running_mutex:
            if self.__running:
                return

            self.__data_class.clear_all(clear_all=True)
            self.__picture_windows_only = picture_windows_only

            if (picture_windows_only and tilt_enabled) or not picture_windows_only:
                self.__frontal_window.start()
                self.__sagittal_window.start()

            if self.__picture_windows_only:
                self.__master.after(80, self.__animate_picture_window_only)
                make_k7_window_active()
                if not self.__main_window_hidden:
                    self.__main_window.withdraw()
            else:
                if self.__main_window_hidden:
                    self.__main_window.deiconify()
                    self.__main_window_hidden = False
                if self.ani is None:
                    self.ani = animation.FuncAnimation(
                        self.__fig, self.__animate, None, interval=80, blit=False,
                        cache_frame_data=False, save_count=0
                    )
                else:
                    self.ani.resume()

            # mark module initialized so that all the recording can start
            self.__namespace.app_ready = True
            self.__running = True

    def stop(self):
        with self.__running_mutex:
            if not self.__running:
                return

            self.__namespace.app_ready = False
            if self.ani:
                self.ani.pause()
            self.__frontal_window.stop()
            self.__sagittal_window.stop()
            if not self.__main_window_hidden:
                self.__main_window.withdraw()
                self.__main_window_hidden = True
            self.__running = False

    def __setup_combo_values(self):
        gain_combo = self.__builder.get_object('gain_control_combo')
        gain_combo.configure(values=GAIN_VALUES)
        gain_combo.current(GAIN_VALUES.index(str(self.__gain_label)))
        gain_combo.bind(
            "<<ComboboxSelected>>",
            lambda event: self.__set_gain_label(gain_combo.get()))

        scan_type_combo = self.__builder.get_object('scan_type_combo')
        scan_type_combo.configure(values=SCAN_TYPE_VALUES)
        scan_type_combo.current(SCAN_TYPE_VALUES.index(self.__scan_type_label))
        scan_type_combo.bind(
            "<<ComboboxSelected>>",
            lambda event: self.__set_scan_type_label(scan_type_combo.get()))

        speed_combo = self.__builder.get_object('speed_combo')
        speed_combo.configure(values=SPEED_VALUES)
        speed_combo.current(SPEED_VALUES.index(self.__speed_label))
        speed_combo.bind(
            "<<ComboboxSelected>>",
            lambda event: self.__set_speed_label(speed_combo.get()))

        filter_combo = self.__builder.get_object('filter_combo')
        filter_combo.configure(values=FILTER_COMBO_VALUES)
        filter_combo.current(FILTER_COMBO_VALUES.index(self.__scan_filter_type))
        filter_combo.bind(
            "<<ComboboxSelected>>",
            lambda event: self.__set_scan_filter_type(filter_combo.get()))

    def __setup_buttons(self):
        pause_button = self.__builder.get_object('pause_button')
        pause_button.configure(command=self.__data_class.pause_playback, takefocus=0)

        play_button = self.__builder.get_object('play_button')
        play_button.configure(command=self.__data_class.resume_playback, takefocus=0)

        clear_button = self.__builder.get_object('clear_button')
        clear_button.configure(command=self.__data_class.clear_all, takefocus=0)
        clear_button.unbind('<space>')

        start_record_button = self.__builder.get_object('start_record_button')
        start_record_button.configure(
            takefocus=0,
            command=lambda: self.__data_class.start_recording(
                scan_type_label=self.__scan_type_label))

        stop_record_button = self.__builder.get_object('stop_record_button')
        stop_record_button.configure(command=self.__data_class.stop_recording, takefocus=0)

        save_record_button = self.__builder.get_object('save_record_button')
        save_record_button.configure(command=lambda: self.__data_class.save_recording(
            patient_path=self.__patient_path), takefocus=0)

        instructions_button = self.__builder.get_object('instruction_button')
        instructions_button.configure(command=self.show_instructions, takefocus=0)

        graph_button = self.__builder.get_object('graph_button')
        graph_button.configure(command=self.__plot_graph_values, takefocus=0)

        if not self.__picture_windows_only:
            self.__master.bind_all('<KeyRelease>', self.__key_press_event)

    def __key_press_event(self, event):
        keycode = event.keycode
        if keycode == ord(' '):
            self.__data_class.toggle_recording(scan_type_label=self.__scan_type_label)
        elif keycode in [ord('c'), ord('C')]:
            self.__data_class.clear_all()
        elif keycode == 116:
            self.__data_class.save_recording(patient_path=self.__patient_path)
        elif keycode in [ord('i'), ord('I')]:
            self.show_instructions()

    def __plot_graph_values(self):
        graph_window = GraphPlotWindow(
            scan_filter_type=self.__scan_filter_type,
            patient_path=self.__patient_path, patient_name=self.__patient_name)
        graph_window.setup_toolbar()
        self.__frontal_window.window.withdraw()
        self.__sagittal_window.window.withdraw()
        self.__main_window.withdraw()
        self.__data_class.pause_data_capture()

        def exit_sub_window():
            self.__filter_table_callback(None)
            graph_window.window.destroy()

            self.__main_window.deiconify()
            self.__frontal_window.window.deiconify()
            self.__sagittal_window.window.deiconify()
            self.__data_class.resume_data_capture()

        graph_window.window.protocol("WM_DELETE_WINDOW", exit_sub_window)
        graph_window.window.mainloop()
        graph_window.window.quit()
        del graph_window

    @classmethod
    def show_instructions(cls):
        win = Toplevel()
        win.wm_title("Sweep Mode Instructions")

        try:
            instructions_text = open(cls.INSTRUCTIONS_TEXT_FILE_PATH).read()
        except FileNotFoundError:
            instructions_text = "Sweep instructions not found at {}".format(cls.INSTRUCTIONS_TEXT_FILE_PATH)

        instructions_label = Label(win, text=instructions_text)
        instructions_label.grid(row=0, column=0)

        b = Button(win, text="Close", command=win.destroy)
        b.grid(row=1, column=0)

    def __setup_graph__(self, speed_label):
        if self.__fig:
            ax = self.__ax
            ax.clear()
        else:
            self.__fig = fig = plt.Figure(figsize=(6, 6))
            fig.subplots_adjust(left=0.03, bottom=0.05, right=0.98, top=0.95, wspace=0, hspace=0)
            self.__ax = ax = self.__fig.add_subplot(111)
            self.__canvas = canvas = FigureCanvasTkAgg(fig, master=self.__master)
            canvas.get_tk_widget().grid(column=0, row=2, rowspan=20)

        ax.grid()
        ax.set_ylim([-90, 270])
        ax.get_yaxis().set_ticks(np.arange(-90, 271, 15))
        ax.set_yticklabels(" " * len(np.arange(-90, 271, 15)))

        if speed_label == "1.0":
            ax.get_xaxis().set_ticks(np.arange(0, 16, 1))
            self.__recording_time = 16
            self.X = np.arange(0, 16, 0.1)
        elif speed_label == "2.0":
            ax.get_xaxis().set_ticks(np.arange(0, 32, 2))
            self.__recording_time = 32
            self.X = np.arange(0, 32, 0.1)
        else:
            ax.get_xaxis().set_ticks(np.arange(0, 64, 4))
            self.__recording_time = 64
            self.X = np.arange(0, 64, 0.1)

        self.line_frontal, = self.__ax.plot(self.X, np.zeros(len(self.X)), color='red')
        self.line_sagittal, = self.__ax.plot(self.X, np.zeros(len(self.X)), color='blue')

    def __update_plot_data(self, y_points_frontal, y_points_sagittal):
        points_count = len(y_points_frontal[0])
        gain_label = self.__gain_label / 15
        x_point_count = len(self.X)

        draw_y_points_sagittal = list(map(
            lambda point: (point / gain_label), y_points_sagittal[1])) + ([0] * (x_point_count - points_count))
        draw_y_points_frontal = list(map(
            lambda point: (point / gain_label + 180), y_points_frontal[1])) + (
                                        [180] * (x_point_count - points_count))

        if draw_y_points_sagittal or draw_y_points_frontal:
            self.line_frontal.set_ydata(draw_y_points_sagittal)
            self.line_sagittal.set_ydata(draw_y_points_frontal)

    @staticmethod
    def get_mode_type():
        with open("C:\\K7\\current_mode_type") as fp:
            return json.load(fp)["show_sweep_graph"]

    def __animate(self, *_args):
        data_class = self.__data_class
        namespace = data_class.namespace

        try:
            exit_thread = namespace.exit_thread
        except (FileNotFoundError, EOFError):
            exit_thread = True

        if exit_thread:
            self.stop()
            return

        namespace_event, event_data = namespace.event
        if namespace_event:
            if namespace_event == EVENT_TOGGLE_RECORDING:
                if self.__temp_file_fd:
                    with open(self.__temp_file_fd, "rb") as fd:
                        __saved_data = pickle.load(fd)
                        self.__data_class._DataClass__y_points_sagittal = __saved_data['y_points_sagittal']
                        self.__data_class._DataClass__y_points_frontal = __saved_data['y_points_frontal']
                        self.__temp_file_fd = None

                self.__requested_playback_file_name = None
                data_class.toggle_recording(scan_type_label=RecordingModes.CMS_SCAN.value)
            elif namespace_event == EVENT_USER_RECORD_SAVED:
                if self.__temp_file_fd:
                    with open(self.__temp_file_fd, "rb") as fd:
                        saved_data_direct = pickle.load(fd)
                        data_class.save_recording(
                            patient_path=self.__patient_path, extra_filter=event_data,
                            saved_data=([saved_data_direct], RecordingModes.CMS_SCAN.value)
                        )
                    self.__temp_file_fd = None
                else:
                    data_class.save_recording(
                        patient_path=self.__patient_path, extra_filter=event_data
                    )
            elif namespace_event == EVENT_CMS_RECORDING_PLAYBACK:
                if not self.__data_class.is_recording_on:
                    self.__cms_callback(extra_filter=event_data, with_summary=True)
                self.__requested_playback_file_name = event_data
                namespace.requested_playback_file_name = event_data
            elif namespace_event == EVENT_CMS_START_PLAYBACK:
                if self.__requested_playback_file_name:
                    self.__cms_callback(
                        extra_filter=namespace.requested_playback_file_name, fast_replay=True,
                        with_summary=True)
            elif namespace_event == EVENT_MARK_REDRAW_TOOL:
                if self.__requested_playback_file_name:
                    self.__cms_callback(
                        extra_filter=namespace.requested_playback_file_name, fast_replay=True,
                        with_summary=False)
                else:
                    if self.__temp_file_fd:
                        with open(self.__temp_file_fd, "rb") as fd:
                            __saved_data = pickle.load(fd)
                    else:
                        temp_dir = path.join(self.__patient_path, "temp")
                        pathlib.Path(temp_dir).mkdir(parents=True, exist_ok=True)
                        temp_file = path.join(temp_dir, str(uuid4()))

                        __saved_data = {
                            'y_points_sagittal': self.__data_class.y_points_sagittal,
                            'y_points_frontal': self.__data_class.y_points_frontal
                        }

                        with open(temp_file, "wb") as fd:
                            fd.write(pickle.dumps(__saved_data))
                        self.__temp_file_fd = temp_file

                    self.__data_class.start_playback(
                        playback_data=__saved_data, fast_replay=True, with_summary=False
                    )

        if data_class.is_playback_on:
            return self.__animate_playback()

        if data_class.is_recording_paused:
            return

        data_queue = data_class.data_queue

        y_points = []
        append = y_points.append
        while data_queue.qsize() > 0:
            input_points = data_queue.get()
            if input_points is None:
                break
            if data_class.saved_data:
                continue
            datetime_now, frontal, sagittal = input_points
            append((datetime_now, sagittal, frontal))
        data_class.append_data(y_points=y_points, points_count=len(self.X))

        if self.__data_class.saved_data:
            return

        if data_class.is_recording_on:
            if self.__show_sweep_graph is None:
                self.__show_sweep_graph = self.get_mode_type()
            if self.__show_sweep_graph is True:
                if (datetime.now() - data_class.recording_started_at).total_seconds() >= self.__recording_time:
                    data_class.stop_recording()

        y_points_sagittal = data_class.y_points_sagittal
        y_points_frontal = data_class.y_points_frontal

        self.__update_plot_data(y_points_frontal=y_points_frontal, y_points_sagittal=y_points_sagittal)

    def __animate_picture_window_only(self, *_args):
        self.__animate()

        if not self.__data_class.namespace.exit_thread:
            self.__master.after(80, self.__animate_picture_window_only)

    def __animate_playback(self):
        data_class = self.__data_class

        if data_class.is_recording_paused:
            return

        playback_data = data_class.playback_data
        y_points_sagittal = data_class.y_points_sagittal
        y_points_frontal = data_class.y_points_frontal

        playback_speed = 100 * self.__data_class.playback_speed
        if not data_class.playback_last_run_time:
            data_class.playback_last_run_time = playback_data['y_points_frontal'][0][0]
        data_class.playback_last_run_time = data_class.playback_last_run_time + timedelta(milliseconds=playback_speed)
        playback_last_run_time = data_class.playback_last_run_time

        i = 0
        found = False
        image_window_options = self.__data_class.image_window_options
        for i, (timestamp, frontal, sagittal) in enumerate(zip(
                playback_data['y_points_frontal'][0], playback_data['y_points_frontal'][1],
                playback_data['y_points_sagittal'][1])):
            if timestamp <= playback_last_run_time:
                found = True
                y_points_sagittal[0].append(timestamp)
                y_points_sagittal[1].append(sagittal)
                y_points_frontal[0].append(timestamp)
                y_points_frontal[1].append(frontal)
                image_window_options.current_frontal = frontal
                image_window_options.current_sagittal = sagittal
            else:
                break

        self.__update_plot_data(y_points_frontal=y_points_frontal, y_points_sagittal=y_points_sagittal)

        if found:
            if i == 0:
                i += 1
            playback_data['y_points_frontal'] = [
                playback_data['y_points_frontal'][0][i:], playback_data['y_points_frontal'][1][i:]]
            playback_data['y_points_sagittal'] = [
                playback_data['y_points_sagittal'][0][i:], playback_data['y_points_sagittal'][1][i:]]

        if len(playback_data['y_points_sagittal'][1]) == 0:
            data_class.stop_playback()

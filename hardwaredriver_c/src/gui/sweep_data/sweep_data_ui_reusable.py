import os
import pathlib
import pickle
from multiprocessing import Queue
from os import path
from queue import Empty

import pkg_resources
import pygubu

from MyotronicsDriver.hardware.functions.sweep_data.data_table import Table
from MyotronicsDriver.hardware.functions.sweep_data.namespace_options import NamespaceOptions
from MyotronicsDriver.hardware.functions.sweep_data.ui_classes.data_class import DataClass, RecordingModes
from MyotronicsDriver.hardware.functions.sweep_data.ui_classes.graph import Graph
from MyotronicsDriver.hardware.functions.sweep_data.ui_classes.image_helpers import ImageHelpers
from MyotronicsDriver.hardware.functions.sweep_data.ui_classes.scrollable_frame import create_scrollable_frame


class PlotAppReusable(ImageHelpers, Graph):
    __table: Table
    __patient_path: str

    DATA_VERSION = "1.2"

    def __close_window(self):
        self.stop()

    def __change_patient_path__(self, new_path):
        with open(new_path, "rb") as fd:
            patient_name = pickle.load(fd)

        first_name, last_name = patient_name.split('+')

        namespace = self.__namespace
        self.__patient_path = patient_path = path.join(
            namespace.root_data_dir(), last_name, first_name, "sweep_data", self.DATA_VERSION
        )
        Graph.update_patient_path(self, new_patient_path=patient_path)
        Graph.update_patient_name(self, new_patient_name=patient_name)
        pathlib.Path(self.__patient_path).mkdir(parents=True, exist_ok=True)

    def __init__(self, data_queue, namespace: NamespaceOptions, command_queue: Queue):
        ui_file = pkg_resources.resource_filename(__name__, "sweep_mode.ui")

        self.builder = builder = pygubu.Builder()
        builder.add_from_file(ui_file)
        self.main_window = main_window = builder.get_object('mainwindow')
        main_window.protocol("WM_DELETE_WINDOW", self.__close_window)
        main_window.title("K7-Postural Range of Motion")

        first_name, last_name = namespace.patient_name.split('+')
        self.__patient_path = patient_path = path.join(
            namespace.root_data_dir(), last_name, first_name, "sweep_data", self.DATA_VERSION)
        pathlib.Path(self.__patient_path).mkdir(parents=True, exist_ok=True)

        self.__data_class = data_class = DataClass(
            builder=builder, namespace=namespace, data_queue=data_queue)
        Graph.__init__(
            self, master=builder.get_object('graph_canvas'), main_window=main_window,
            data_class=data_class, builder=builder,
            filter_table_callback=self.__table_filter_callback,
            patient_path=patient_path, patient_name=namespace.patient_name,
            cms_callback=self.playback_cms_window, namespace=namespace
        )

        self.__data_queue = data_queue
        self.__command_queue = command_queue
        self.__namespace = namespace

    def __watch_for_command__(self):
        try:
            command, picture_windows_only, tilt_enabled = self.__command_queue.get_nowait()
        except Empty:
            command, picture_windows_only, tilt_enabled = None, None, None

        if command == "start":
            self.start(picture_windows_only=picture_windows_only, tilt_enabled=tilt_enabled)
        elif command == "stop":
            self.stop()

        self.main_window.after(100, self.__watch_for_command__)

    def __table_filter_callback(self, scan_filter_type):
        self.__table.re_populate_data(scan_filter_type=scan_filter_type)

    def setup_table(self):
        scrollable_frame = create_scrollable_frame(
            builder=self.builder, container_name='table_data_frame_container')

        self.__table = Table(
            scrollable_frame, patient_path_data=self.__patient_path,
            playback_callback=self.__playback_callback)

    def setup_value_labels(self):
        a_flex_image_label = self.builder.get_object('a_flex_image_label', self.main_window)
        blue_arrow_image_path = pkg_resources.resource_filename(__name__, "images/blue_arrow.jpg")
        self.attach_image(
            label=a_flex_image_label, image_path=blue_arrow_image_path, orientation_angle=90)

        p_ext_image_label = self.builder.get_object('p_ext_image_label', self.main_window)
        self.attach_image(
            label=p_ext_image_label, image_path=blue_arrow_image_path, orientation_angle=-90)
        p_ext_image_label.pack(pady=(0, 75))

        red_arrow_image_path = pkg_resources.resource_filename(__name__, "images/red_arrow.jpg")
        r_flex_image_label = self.builder.get_object('r_flex_image_label')
        self.attach_image(
            label=r_flex_image_label, image_path=red_arrow_image_path, orientation_angle=90)
        r_flex_image_label.pack(pady=(45, 0))

        l_flex_image_label = self.builder.get_object('l_flex_image_label')
        self.attach_image(
            label=l_flex_image_label, image_path=red_arrow_image_path, orientation_angle=-90)

        ap_pitch_label = self.builder.get_object('ap_pitch_label')
        ap_pitch_label.configure(font=('Arial', 12, "bold"))

        lateral_roll_label = self.builder.get_object('lateral_roll_label')
        lateral_roll_label.configure(font=('Arial', 12, "bold"))
        lateral_roll_label.pack(pady=(230, 0))

    def run(self, *args, **kwargs):
        self.main_window.mainloop(*args, **kwargs)

    def __playback_callback(self, file_name):
        data_path = path.join(self.__patient_path, file_name)
        playback_data = pickle.load(open(data_path, "rb"))['saved_data'][0][0]

        self.__data_class.start_playback(playback_data=playback_data)

    def playback_cms_window(self, extra_filter, with_summary=False, fast_replay=False):
        for _file_name in os.listdir(self.__patient_path):
            if _file_name == "temp":
                continue
            file_path = os.path.join(self.__patient_path, _file_name)
            with open(file_path, "rb") as file:
                try:
                    pickled_data = pickle.load(file)
                except EOFError:
                    continue
                data = pickled_data['saved_data']
                try:
                    run_data_buckets, run_type = data
                    if run_type == RecordingModes.CMS_SCAN.value and pickled_data['extra_filter'] == extra_filter:
                        self.__data_class.start_playback(
                            playback_data=data[0][0], fast_replay=fast_replay, with_summary=with_summary)
                except TypeError:
                    pass

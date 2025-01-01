import math
from os import path
from tkinter import Tk

import pkg_resources
from PIL import Image, ImageDraw

from MyotronicsDriver.hardware.functions.sweep_data.namespace_options import NamespaceOptions
from MyotronicsDriver.utils.windows_api import load_placement_values, setup_watch_event


class PictureWindowFunctions:
    image = None
    window: Tk

    size = 100

    def __update_patient_name__(self, patient_name):
        first_name, last_name = patient_name.split('+')

        data_package_name = "MyotronicsDriver.data"
        if self.__is_frontal:
            front_path = __patient_path = path.join(NamespaceOptions.root_data_dir(), last_name, first_name,
                                                    "sagittal.JPG")
            if path.isfile(front_path):
                filename_face = front_path
            else:
                filename_face = pkg_resources.resource_filename(data_package_name, "figure8.jpg")
        else:
            face_path = __patient_path = path.join(NamespaceOptions.root_data_dir(), last_name, first_name,
                                                   "frontal.JPG")
            if path.isfile(face_path):
                filename_face = face_path
            else:
                filename_face = pkg_resources.resource_filename(data_package_name, "figure10.jpg")

        self.__filename_face = filename_face
        self.create_image_handler()

    def __init__(self, is_frontal, patient_name, window):
        self.window = window

        self.__is_frontal = is_frontal
        self.__update_patient_name__(patient_name=patient_name)

        setup_watch_event(self.place_window)
        window.overrideredirect(True)
        window.attributes('-topmost', True)

        self.__namespace = namespace = NamespaceOptions(reset_states=False)
        namespace.setup_watch()

        self.place_window()

    def create_image_handler(self):
        if self.__is_frontal:
            line_color = 'blue'
        else:
            line_color = 'red'

        size = self.size
        self.image = image = Image.open(self.__filename_face).resize((size, size))
        draw = ImageDraw.Draw(image)
        draw.line(xy=(size / 2, 0, size / 2, size), width=3, fill=line_color)
        del draw

    def place_window(self):
        if self.__is_frontal:
            x_pos, y_pos, size = load_placement_values(is_left=True)
        else:
            x_pos, y_pos, size = load_placement_values(is_left=False)
        self.size = size
        self.create_image_handler()
        self.window.geometry("{}x{}+{}+{}".format(size, size + 20, x_pos, y_pos))

    @staticmethod
    def draw_line_at_angle(draw, angle, size):
        if angle < 0:
            x = size
            angle *= -1
        else:
            x = 0

        y = size - (math.tan(math.radians(90 - angle)) * size / 2)
        draw.line(xy=(size / 2, size, x, y), width=3, fill='grey')

    @staticmethod
    def pad_values(angle):
        space_count = 0
        if angle > 0:
            space_count += 1
        if abs(angle) < 10:
            space_count += 1
        return "{}{}".format("  " * space_count, int(angle))

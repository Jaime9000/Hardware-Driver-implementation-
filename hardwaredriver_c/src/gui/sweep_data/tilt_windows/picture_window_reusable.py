from os import path
from shutil import copy
from threading import Lock
from tkinter import *
from tkinter import filedialog

from PIL import ImageDraw, ImageTk

from MyotronicsDriver.hardware.functions.sweep_data.namespace_options import NamespaceOptions
from .image_window_options import ImageWindowOptions
from .picture_window_functions import PictureWindowFunctions


class PictureWindowReusable(PictureWindowFunctions):
    __after_id_mutex = Lock()
    __running = False

    def update_patient_name(self, new_patient_name):
        self.__patient_name = new_patient_name
        PictureWindowFunctions.__update_patient_name__(self, patient_name=new_patient_name)

    def __init__(self, patient_name, main_window: Tk, is_frontal: bool, image_window_options: ImageWindowOptions):
        self.__window = window = Toplevel()
        window.title("Picture window: {}".format("Frontal" if is_frontal else "Sagittal"))

        PictureWindowFunctions.__init__(
            self, is_frontal=is_frontal, patient_name=patient_name, window=window
        )

        self.__image_window_options = image_window_options
        self.__patient_name = patient_name
        self.__is_frontal = is_frontal

        self.__namespace = namespace = NamespaceOptions(reset_states=False)
        namespace.setup_watch()

        self.__canvas = canvas = Canvas(window, width=100, height=100)
        canvas.pack(side=TOP)
        canvas.bind("<Button-1>", lambda event: self.__load_image(is_frontal=is_frontal))

        self.__frame = frame = Frame(master=window, bd=1, relief=SUNKEN)
        frame.pack(side=BOTTOM, fill=X)

        self.__max_value_label = max_value_label = Label(master=frame)
        max_value_label.pack(side=LEFT, anchor=W, fill=X)

        self.__current_value_label = current_value_label = Label(master=frame)
        current_value_label.pack(side=LEFT, anchor=CENTER, fill=X)

        self.__min_value_label = min_value_label = Label(master=frame)
        min_value_label.pack(side=RIGHT, anchor=E, fill=X)

        self.__window_hidden = False

    def start(self):
        with self.__after_id_mutex:
            if not self.__running:
                self.__window.after(100, self.draw)
                self.__running = True
                self.show_window()

    def stop(self):
        with self.__after_id_mutex:
            self.__running = False
            self.hide_window()

    def __load_image(self, is_frontal):
        filetypes = (
            ('Image Files', '*.jpg'),
            ('Image Files', '*.png')
        )

        filename = filedialog.askopenfilename(
            title='Open a file',
            initialdir='/',
            filetypes=filetypes)
        if filename:
            first_name, last_name = self.__patient_name.split('+')

            if is_frontal:
                image_path = path.join(NamespaceOptions.root_data_dir(), last_name, first_name, "sagittal.JPG")
            else:
                image_path = path.join(NamespaceOptions.root_data_dir(), last_name, first_name, "frontal.JPG")
            copy(filename, image_path)
            self.__filename_face = image_path
            self.create_image_handler()

    def hide_window(self):
        if not self.__window_hidden:
            self.__window.withdraw()
            self.__window_hidden = True

    def show_window(self):
        if self.__window_hidden:
            self.__window.deiconify()
            self.__window_hidden = False

    def __draw(self):
        if self.__namespace.options_display:
            self.show_window()
        else:
            self.hide_window()

        canvas = self.__canvas
        is_frontal = self.__is_frontal
        image_window_options = self.__image_window_options

        size = self.size

        if image_window_options.max_frontal is not None:
            im = self.image.copy()
            draw = ImageDraw.Draw(im)

            if is_frontal:
                min_angle = image_window_options.min_frontal
                max_angle = image_window_options.max_frontal
                max_label = "A Flex"
                min_label = "P Ext"
            else:
                min_angle = image_window_options.min_sagittal
                max_angle = image_window_options.max_sagittal
                max_label = "R Flex"
                min_label = "L Flex"
            self.draw_line_at_angle(
                draw=draw, angle=max_angle, size=size)
            self.draw_line_at_angle(
                draw=draw, angle=min_angle, size=size)

            max_text = int(max_angle)
            min_text = int(min_angle)
            if min_angle < 0:
                min_text = int(-1 * min_angle)
            if max_angle < 0:
                max_text = int(-1 * min_angle)

            self.__max_value_label.configure(text="{}{}{}".format(max_label, max_text, u"\u00b0"))
            self.__min_value_label.configure(text="{}{}{}".format(min_label, min_text, u"\u00b0"))
            self.__current_value_label.configure(text="")

            del draw
        else:
            im = self.image
            self.__max_value_label.configure(text="")
            self.__min_value_label.configure(text="")

            if is_frontal:
                angle = self.__image_window_options.current_frontal
            else:
                angle = self.__image_window_options.current_sagittal

            label_text = self.pad_values(angle)
            self.__current_value_label.configure(text="{}  {}{}".format("Angle", label_text, u"\u00b0"))
            im = im.rotate(angle)

        tkimage = ImageTk.PhotoImage(im, master=canvas)
        canvas.create_image(size / 2, size / 2, image=tkimage)
        canvas.config(bg="white", width=size, height=size)
        canvas.image = tkimage

        self.__window.after(100, self.draw)

    def draw(self):
        with self.__after_id_mutex:
            if not self.__running:
                return
            else:
                self.__draw()

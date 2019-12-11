import logging

import serial


class USBControl:
    RTS_ON_USB = 0x01
    RTS_OFF_USB = 0x02
    DTR_ON_USB = 0x03
    DTR_OFF_USB = 0x04

    SLOW_BAUD_RATE = 115200
    FAST_BAUD_RATE = 230400

    HANDSHAKE_STRING = b'K7-MYO6'  # last digit '6' can be replaced with '5', need to talk to scott.

    @property
    def ser(self) -> serial.Serial:
        return self.__ser__

    def __init__(self):
        self.__ser__ = self.get_serial_handler()

    @classmethod
    def get_serial_handler(cls, use_slow_baud=False) -> serial.Serial:
        if use_slow_baud:
            baudrate = cls.SLOW_BAUD_RATE
        else:
            baudrate = cls.FAST_BAUD_RATE
        return serial.Serial('/dev/ttyUSB0', baudrate=baudrate, timeout=0.1)

    def usb_control_on(self):
        logging.info("Sending control signal on to hardware.")
        logging.debug("Sending signal, %d" % self.RTS_ON_USB)
        status = self.ser.write(self.RTS_ON_USB)
        assert status == self.RTS_ON_USB, "Failed to send control signal."
        return status

    def usb_control_off(self):
        logging.info("Sending control signal off to hardware.")
        logging.debug("Sending signal, %d" % self.RTS_OFF_USB)
        status = self.ser.write(self.RTS_OFF_USB)
        assert status == self.RTS_OFF_USB
        return status

    def usb_data_on(self):
        logging.info("Sending data signal on to hardware.")
        logging.debug("Sending signal, %d" % self.DTR_ON_USB)
        status = self.ser.write(self.DTR_ON_USB)
        assert status == self.DTR_ON_USB

    def usb_data_off(self):
        logging.info("Sending data signal off to hardware.")
        logging.debug("Sending signal, %d" % self.DTR_OFF_USB)
        status = self.ser.write(self.DTR_OFF_USB)
        assert status == self.DTR_OFF_USB

    def handshake(self):
        logging.info("Initiating handshake.")

        self.usb_control_off()
        self.usb_data_off()

        self.usb_control_on()
        self.ser.write(b'K7-MYO6')
        self.usb_control_off()

        self.usb_data_on()
        line = self.ser.readline()
        assert line == b'K7-MYO Ver 2.0\r', "Handshake failed"

        self.usb_data_off()
        logging.info("Handshake successful.")

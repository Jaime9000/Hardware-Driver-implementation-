from sys import argv

from MyotronicsDriver.config import Config
from MyotronicsDriver.hardware import USBControl


def main():
    Config(argv=argv[1:])

    usb_control = USBControl()
    usb_control.handshake()


if __name__ == "__main__":
    main()

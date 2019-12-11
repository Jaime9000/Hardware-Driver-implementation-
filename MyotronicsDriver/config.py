import logging
from argparse import ArgumentParser


class Config:
    def __init__(self, argv):
        parser = ArgumentParser("Myotronics hardware-service.")
        parser.add_argument("--debug", help="Turn on debug messages.", action='store_true')
        parser.add_argument("--info", help="Turn on info messages.", action='store_true')

        args = parser.parse_args(argv)
        if args.debug:
            logging.basicConfig(level=logging.DEBUG)
        elif args.info:
            logging.basicConfig(level=logging.INFO)

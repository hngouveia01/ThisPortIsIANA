import urllib.request
from collections import defaultdict
from urllib import request

IANA_PORT_URL = "http://www.iana.org/assignments/"\
      "service-names-port-numbers/service-names-port-numbers.csv"

IANA_CSV_FILE = "ports.csv"
IANA_TXT_FILE = "ports.txt"
C_HEADER_FILE = "./iana_ports.h"


def main():
    protocols = set()
    port_ranges = list()
    port_protocol = defaultdict(list)
    urllib.request.urlretrieve(IANA_PORT_URL, IANA_CSV_FILE)
    #parse_iana_file
    #write_data_to_file
    #write_c_language_header

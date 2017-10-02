from collections import defaultdict
from urllib import request
from csv import DictReader
import sys

IANA_PORT_URL = "http://www.iana.org/assignments/"\
      "service-names-port-numbers/service-names-port-numbers.csv"

IANA_CSV_FILE = "ports.csv"
IANA_TXT_FILE = "ports.txt"
C_HEADER_FILE = "./tpi_iana.h"


def main():
    protocols = set()
    port_ranges = list()
    port_protocol = defaultdict(list)
    request.urlretrieve(IANA_PORT_URL, IANA_CSV_FILE)
    parse_iana_csv_file(port_protocol, protocols, port_ranges)
    write_to_txt_file(port_protocol, protocols, port_ranges)
    write_c_header(port_ranges, protocols)


def parse_iana_csv_file(port_protocol, protocols, port_ranges):
    with open(IANA_CSV_FILE, 'r', encoding='utf-8') as f_csv:
        content = DictReader(f_csv)

        for row in content:
            if bool(row['Service Name']) and bool(row['Port Number'] and bool(row['Transport Protocol'])):
                if '-' in row['Port Number']:
                    port_ranges.append(row['Port Number'].upper() +
                                       "/" +
                                       row['Transport Protocol'].upper() +
                                       ">" +
                                       row['Service Name'].upper())
                else:
                    port_protocol[row['Port Number'].upper() +
                                  "/" +
                                  row['Transport Protocol'].upper()].append([row['Service Name'].upper()])
                    protocols.add(row['Transport Protocol'].upper())
    f_csv.close()


def write_to_txt_file(port_protocol, protocols, port_ranges):
    protocols = sorted(protocols)

    with open(IANA_TXT_FILE, 'w', encoding='utf-8') as f_txt:
        sys.stdout = f_txt
        print(end='#')
        print(end='|')

        for protocol in protocols:
            print(protocol, end='|')

        print(end='#')
        print()
        print('$')

        for port_range in port_ranges:
            print(port_range)

        print('$')

        for key in port_protocol.keys():
            print(key, end='>')
            print(port_protocol[key][0][0])
    f_txt.close()
    sys.stdout = sys.__stdout__


def write_c_header(port_ranges, protocols):
    with open(C_HEADER_FILE, 'w', encoding='utf-8') as f_header:
        sys.stdout = f_header
        write_definitions(protocols, port_ranges)

        write_definitions_port_range(port_ranges)
    f_header.close()
    sys.stdout = sys.__stdout__


def write_definitions_port_range(port_ranges):
    for port_range in port_ranges:
        print('\t\t\t{ ', end='')
        # 6000[0]-6063/TCP>X11[1]
        min_port = port_range.split('-')
        print(min_port[0] + ', ', end='')
        # 6063[0]/TCP>X11[1]
        max_port = min_port[1].split('/')
        print(max_port[0] + ', ', end='')
        # TCP[0]>X11[1]
        protocol = max_port[1].split('>')
        print('TPI_IANA_PORTOCOL_' + protocol[0] + ', ', end='')
        # X11[0]'\n'[1]
        service_name = protocol[1].split('\n')
        print('\"' + service_name[0] + '\"', end='')
        print(' }, \\')
    print('\t\t\t{ -1. -1. INT_MAX, NULL } \\')
    print('\t\t}')
    print('#endif')


def write_definitions(protocols, port_ranges):
    counter = 1
    print('#ifndef TPI_IANA_HEADER_H_')
    print('#define TPI_IANA_HEADER_H_')
    print()
    print('#include <stddef.h>')
    print('#include <limits.h>')
    for protocol in protocols:
        print('#define TPI_IANA_PROTOCOL_' + protocol.upper() + ' 0x' + str(counter))
        counter += 1
    print()

    print("#define TPI_NUMBER_OF_PROTOCOLS " +
          str(len(protocols) + 1))
    print()

    print("#define TPI_NUMBER_OF_PORT_INTERVAL " +
          str(len(port_ranges) + 1))

    print('#define TPI_IANA_TRANSPORT_PROTOCOL_INIT { ', end='')
    for protocol in protocols:
        print('\"' + protocol.upper() + "\", ", end='')
    print(' NULL }')
    print()
    print('#define TPI_IANA_TRANSPORT_PROTOCOL_NAME_INIT { \\')
    for protocol in protocols:
        print('\t\t\t{ TPI_IANA_PROTOCOL_' + protocol.upper() + ', ' + '\"' + protocol.upper() + '\" }, \\')
    print('\t\t\t{ INT_MAX, NULL } \\')
    print('\t\t}')
    print()
    print('#define TPI_IANA_PORT_RANGES_INIT ', end='')
    print('{ \\')


if __name__ == '__main__':
    main()

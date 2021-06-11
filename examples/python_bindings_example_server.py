#!/usr/bin/python3

# Build required code:
# $ ./examples/buildall.py
#
# Start zmqproxy (only one instance)
# $ ./build/zmqproxy
#
# Run server, default enabling ZMQ interface:
# $ LD_LIBRARY_PATH=build PYTHONPATH=build python3 examples/python_bindings_example_server.py
#

import os
import time
import sys
import threading

import csp


def csp_server():
    sock = csp.socket()
    csp.bind(sock, csp.CSP_ANY)
    csp.listen(sock, 5)
    while True:
        # wait for incoming connection
        conn = csp.accept(sock, csp.CSP_MAX_TIMEOUT)
        if not conn:
            continue

        print ("connection: source=%i:%i, dest=%i:%i" % (csp.conn_src(conn),
                                                         csp.conn_sport(conn),
                                                         csp.conn_dst(conn),
                                                         csp.conn_dport(conn)))

        while True:
            # Read all packets on the connection
            packet = csp.read(conn, 100)
            if packet is None:
                break

            if csp.conn_dport(conn) == 10:
                # print request
                data = bytearray(csp.packet_get_data(packet))
                length = csp.packet_get_length(packet)
                print ("got packet, len=" + str(length) + ", data=" + ''.join('{:02x}'.format(x) for x in data))
                # send reply
                data[0] = data[0] + 1
                reply = csp.buffer_get(1)
                csp.packet_set_data(reply, data)
                csp.sendto_reply(packet, reply, csp.CSP_O_NONE)

            else:
                # pass request on to service handler
                csp.service_handler(conn, packet)


if __name__ == "__main__":

    # init csp
    csp.init(27, "test_service", "bindings", "1.2.3", 10, 300)
    csp.zmqhub_init(27, "localhost")
    csp.rtable_set(0, 0, "ZMQHUB")
    csp.route_start_task()

    print("Hostname: %s" % csp.get_hostname())
    print("Model:    %s" % csp.get_model())
    print("Revision: %s" % csp.get_revision())

    print("Routes:")
    csp.print_routes()

    # start CSP server
    threading.Thread(target=csp_server).start()

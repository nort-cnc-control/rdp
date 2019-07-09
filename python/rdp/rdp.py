#-*- encoding: utf-8 -*-

import rdp.wrapper

class RDP(object):
    def __init__(self, connected_cb=None,
                       closed_cb=None,
                       dgram_send_cb=None,
                       data_received_cb=None,
                       data_transmitted_cb=None):
        self.__conn = rdp.wrapper.create_connection()
    
    def set_connected_cb(self, cb):
        rdp.wrapper.set_connected_cb(self.__conn, cb)

    def set_closed_cb(self, cb):
        rdp.wrapper.set_closed_cb(self.__conn, cb)

    def set_dgram_send_cb(self, cb):
        rdp.wrapper.set_dgram_send_cb(self.__conn, cb)

    def set_data_received_cb(self, cb):
        rdp.wrapper.set_data_received_cb(self.__conn, cb)

    def set_data_transmitted_cb(self, cb):
        rdp.wrapper.set_data_transmitted_cb(self.__conn, cb)

    def reset(self):
        rdp.wrapper.reset(self.__conn)
    
    def listen(self, port):
        return rdp.wrapper.listen(self.__conn, port)
    
    def connect(self, source_port, target_port):
        return rdp.wrapper.connect(self.__conn, source_port, target_port)
    
    def close(self):
        return rdp.wrapper.close(self.__conn)
    
    def send(self, data):
        return rdp.wrapper.send(self.__conn, data)

    def get_state(self):
        return rdp.wrapper.state(self.__conn)

    def dgram_receive(self, dgram):
        return rdp.wrapper.datagram_receive(self.__conn, dgram)

    def tick(self, dt):
        return rdp.wrapper.tick(self.__conn, dt)

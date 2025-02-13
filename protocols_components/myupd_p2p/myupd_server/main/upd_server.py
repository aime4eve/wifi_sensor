'''
@module:upd_server
@author:[伍志勇]
@date:2025-02-13
@version:v1.0.0

@brief:实现udp_server服务器的测试
1.测试udp_server服务器的ipv4
2.测试udp_server服务器的ipv6
3.测试udp_server服务器的ipv4和ipv6
4.接收udp_client发送的数据
6.向udp_client发送接收成功数据
5.关闭socket
'''

import socket
import sys
import logging


class Udp_Server:
    def __init__(self, ip_type, ip, port):
        self.ip_type = ip_type
        self.ip = ip
        self.port = port
        self.sock = None

    def socket_bind(self):
        '''
        @brief:socket bind
        @param:none
        @return:none
        '''
        try:
            if self.ip_type == 'ipv4':
                self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            elif self.ip_type == 'ipv6':
                self.sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
            elif self.ip_type == 'ipv4_ipv6':
                self.sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
                self.sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
            else:
                print("ip_type error")
                sys.exit()
        except socket.error as msg:
            print('Failed to create socket. Error code: ' + str(msg[0]) + ' , Error message : ' + msg[1])
            sys.exit()

        try:
            self.sock.bind((self.ip, self.port))
        except socket.error as msg:
            print('Bind failed. Error code: ' + str(msg[0]) + ' , Error message : ' + msg[1])
            sys.exit()

        print('Socket bind complete')

    def recv_data(self):
        '''
        @brief:recv data
        @param:none
        @return:none
        '''
        try:
            while True:
                try:
                    data, addr = self.sock.recvfrom(1024)
                    print('Received message from ' + addr[0] + ':' + str(addr[1]))
                    print('Data:' + data.decode('utf-8'))
                    # 发送接收成功数据
                    self.send_data('Data received successfully', addr)
                except socket.error as msg:
                    print('Recv failed. Error code: ' + str(msg[0]) + ' , Error message : ' + msg[1])
                    sys.exit()
        except KeyboardInterrupt:
            print('Keyboard interrupt received. Closing socket and exiting.')
            self.close()
            sys.exit()

    def send_data(self, data, addr):
        '''
        @brief:send data
        @param:data: data to send
        @param:addr: address of the client
        @return:none
        '''
        try:
            self.sock.sendto(data.encode('utf-8'), addr)
            print('Sent message to ' + addr[0] + ':' + str(addr[1]))
        except socket.error as msg:
            print('Send failed. Error code: ' + str(msg[0]) + ' , Error message : ' + msg[1])
            sys.exit()

    def close(self):
        '''
        @brief:close socket
        @param:none
        @return:none
        '''
        self.sock.close()

if __name__ == '__main__':
    try:
        udp_server = Udp_Server('ipv4', '192.168.99.60', 3333)
        udp_server.socket_bind()
        udp_server.recv_data()
    except KeyboardInterrupt:
        print('Keyboard interrupt received. Closing socket and exiting.')
        udp_server.close()
        sys.exit()
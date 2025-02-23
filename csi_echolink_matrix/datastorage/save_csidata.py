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
import os
import time
from config import *
from tools import *

class Udp_Server:
    def __init__(self, ip_type, ip, port):
        self.ip_type = ip_type
        self.ip = ip
        self.port = port
        self.sock = None
        self.csi_data_file_name = ""
        self.file_size_limit = 1 * 1024 * 1024  # 1MB
        
        self.recv_csi_raw_data = ""
        self.g_r_count = 0

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
            print(f'Failed to create socket. Error Info: {msg}')
            sys.exit()

        try:
            self.sock.bind((self.ip, self.port))
            # 设置非阻塞模式
            # self.sock.setblocking(False)

        except socket.error as msg:
            print(f'Bind failed. Error Info: {msg}')
            sys.exit()

        print('Socket bind complete')

    def recv_data(self, action='file'):
        '''
        @brief:recv data
        @param:none
        @return:none
        '''
        try:
            self.g_r_count = 0
            while True:
                # time.sleep(1)
                try:
                    data, addr = self.sock.recvfrom(1024)
                    self.g_r_count = self.g_r_count + 1
                    print(f'Received {self.g_r_count} message from {addr[0]} : {addr[1]}')
                    # print('Data:' + data.decode('utf-8'))
                    # 发送接收成功数据
                    # self.send_data('Data received successfully', addr)
                    if action == "file":
                        # 保存csi数据
                        self.save_csi_data(data)
                    else:
                        self.recv_csi_raw_data = data.decode('utf-8')    
 
                        csi_data_dict = parse_csi_data(self.recv_csi_raw_data)
                        if len(csi_data_dict) >0 :
                            print(f'rssi={csi_data_dict['rssi']}')
                            print(f'csi={csi_data_dict['data']}')
                except socket.error as msg:
                    print(f'Recv failed. Error Info: {msg}')
                    # sys.exit()
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

    def create_csi_data_file(self):
        '''
        @brief: create csi data file
        @param:none
        @return:none
        '''
        try:
            if self.csi_data_file_name == "":          
                timestamp = int(time.time() * 1000)  # 精确到毫秒的时间戳
                filename = f'csi_data_{timestamp}.txt'
            else:
                filename = self.csi_data_file_name

            if os.path.exists(filename):
                file_size = os.path.getsize(filename)
                if file_size > self.file_size_limit:
                    # 文件超过大小限制，创建新文件
                    timestamp = int(time.time() * 1000)  # 精确到毫秒的时间戳
                    filename = f'csi_data_{timestamp}.txt'
                    self.g_r_count = 0

            f = open(filename, 'a', encoding='utf-8')  # 创建文件
            f.write('')
            
            self.csi_data_file_name = filename
            return f

        except Exception as e:
            print(f"Error creating csi data file: {e}")
    
    def save_csi_data(self, data):
        '''
        @brief:save data
        @param:data: data to save
        @return:none
        '''
        
        # 检查文件是否存在以及大小
        try:
            csi_file = self.create_csi_data_file()
            csi_file.write(data.decode('utf-8') + '\n')  # 写入数据，添加换行符
            csi_file.close()
        except Exception as e:
            print(f"Error saving data: {e}")

if __name__ == '__main__':
    try:
        udp_server = Udp_Server('ipv4', UDP_Server_IP, UDP_Server_Port)
        udp_server.socket_bind()
        udp_server.recv_data('file')       
    except KeyboardInterrupt:
        print('Keyboard interrupt received. Closing socket and exiting.')
        udp_server.close()
        sys.exit()
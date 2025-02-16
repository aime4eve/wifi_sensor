'''
@module:upd_client
@author:[伍志勇]
@date:2025-02-13
@version:V1.0.0

@brief:发送数据到upd_server服务器
1.创建upd客户端
2.发送数据到upd服务器
3.关闭upd客户端

'''

import socket

class UdpClient:
    def __init__(self, server_ip, server_port):
        self.server_ip = server_ip
        self.server_port = server_port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def send_data(self, message):
        """Sends data to the UDP server."""
        try:
            self.sock.sendto(message.encode('utf-8'), (self.server_ip, self.server_port))
            print(f"Sent {message} to {self.server_ip}:{self.server_port}")
        except Exception as e:
            print(f"Error sending data: {e}")

    def close(self):
        """Closes the UDP client socket."""
        self.sock.close()
        print("UDP client closed")

if __name__ == '__main__':
    # Example usage:
    UDP_IP = "192.168.99.63"  # Replace with the server IP address
    UDP_PORT = 3333  # Replace with the server port

    client = UdpClient(UDP_IP, UDP_PORT)

    try:
        while True:
            message = input("Enter message to send (or 'exit' to quit): ")
            if message.lower() == 'exit':
                break
            client.send_data(message)
    except KeyboardInterrupt:
        print("Exiting...")
    finally:
        client.close()
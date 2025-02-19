import socket
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import numpy as np
from tools import parse_csi_data

# 全局变量
csirawdata = []
vizrawdata = []
vizdata = []

# UDP客户端设置
UDPIP = "192.168.99.55"
UDPPORT = 4444

# 创建UDP套接字
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDPIP, UDPPORT))

# 初始化折线图
fig, ax = plt.subplots()
line, = ax.plot([], [], 'r-')
ax.set_xlim(0, 100)
ax.set_ylim(-20, 20)

# 解析数据并更新折线图的函数
def parseandupdate(data):
    global vizrawdata
    vizrawdata = [float(data[i]) + 1j * float(data[i+1]) for i in range(0, len(data), 2)]
    return vizrawdata

# 更新折线图的函数
def updateline(newdata):
    vizdata = np.abs(newdata)
    line.set_data(range(len(vizdata)), vizdata)
    return line,

# 接收数据的生成器函数
def receivedata():
    while True:
        data, _ = sock.recvfrom(1024)
        # data = data.decode('utf-8').split(',')
        csi_raw_data = parse_csi_data(data.decode('utf-8'))
        newdata = parseandupdate(csi_raw_data['data'])
        yield newdata

# 创建动画
ani = animation.FuncAnimation(fig, updateline, receivedata, blit=True, interval=100)

# 显示折线图窗口
plt.show()

'''
写一段python代码，实现：
1、连接UDP服务器，服务器IP地址为 "192.168.99.55"，通信端口为4444；
2、将接收到的数据保存到全局变量csi_raw_data中；
3、解析csi_raw_data，获得需要用折线图动态显示的数据集viz_raw_data；
4、viz_raw_data数据结构定义为：[i,r,..]，其中i为虚部，r为实部。i和r成对出现;
5、将viz_raw_data中每对i,r，计算出对应的模值，保存到viz_data变量中；
6、将viz_data变量用折线图的方式动态展示出来；
7、要能根据从udp服务器获得的csi_raw_data来动态刷新这个折线图。
'''
import socket
import threading
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import math

# UDP 配置
# UDP_IP = "192.168.99.55"
# UDP_PORT = 4444
from config import *

# 全局变量
csi_raw_data = []
viz_raw_data = []
viz_data = []
lock = threading.Lock()

# --------------------------------------------------
# 增强型 UDP 接收
# --------------------------------------------------
def udp_receiver():
    global csi_raw_data
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_Server_IP, UDP_Server_Port))
    sock.settimeout(0.1)  # 添加超时控制
    
    while True:
        try:
            data, addr = sock.recvfrom(2048)
            # if addr[0] != UDP_IP:
            #     continue
                
            # 增强数据清洗
            str_data = data.decode().strip('" \n\r')  # 去除首尾特殊字符
            # print(str_data)
            clean_data = []
            for s in str_data.split(','):
                s = s.strip(' []"\'')  # 去除可能存在的格式字符
                if s.replace('-', '', 1).isdigit():  # 支持负数检查
                    clean_data.append(float(s))
                    
            with lock:
                csi_raw_data = clean_data
                
        except socket.timeout:
            pass
        except Exception as e:
            print(f"接收错误: {str(e)}")

# --------------------------------------------------
# 智能数据解析
# --------------------------------------------------
def parse_data():
    global viz_raw_data, viz_data
    while True:
        # 数据同步
        with lock:
            local_data = csi_raw_data.copy()
        
        # 数据对齐
        aligned_len = len(local_data) // 2 * 2
        aligned_data = local_data[:aligned_len]
        
        # 虚部/实部配对
        pairs = []
        for i in range(0, aligned_len, 2):
            try:
                imag = float(aligned_data[i])
                real = float(aligned_data[i+1])
                pairs.append((imag, real))
            except (IndexError, ValueError) as e:
                print(f"解析错误: {str(e)}")
                continue
        
        # 计算模值
        magnitudes = []
        for imag, real in pairs:
            try:
                mag = math.sqrt(imag**2 + real**2)
                magnitudes.append(mag)
            except TypeError:
                magnitudes.append(0.0)
        
        # 更新全局变量
        with lock:
            viz_raw_data = pairs
            viz_data = magnitudes
        
        threading.Event().wait(0.01)

# --------------------------------------------------
# 动态可视化
# --------------------------------------------------
def init_plot():
    fig, ax = plt.subplots(figsize=(12, 6))
    line, = ax.plot([], [], 'b-', lw=1.5)
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 15)
    ax.set_xlabel("Subcarrier Index", fontsize=12)
    ax.set_ylabel("Amplitude (dB)", fontsize=12)
    ax.set_title("Real-time CSI Amplitude Visualization", fontsize=14)
    ax.grid(True, linestyle='--', alpha=0.7)
    return fig, ax, line

def update(frame):
    with lock:
        data = viz_data.copy()
    
    if data:
        ax.set_xlim(0, len(data))
        y_min = max(0, min(data)*0.9)
        y_max = min(20, max(data)*1.1)
        ax.set_ylim(y_min, y_max)
        line.set_data(range(len(data)), data)
    return line,

# --------------------------------------------------
# 主程序
# --------------------------------------------------
if __name__ == "__main__":
    # 配置线程
    threads = [
        threading.Thread(target=udp_receiver, daemon=True),
        threading.Thread(target=parse_data, daemon=True)
    ]
    
    # 启动线程
    for t in threads:
        t.start()
    
    # 初始化图表
    fig, ax, line = init_plot()
    
    # 动画配置
    ani = animation.FuncAnimation(
        fig, update,
        # init_func=lambda: line.set_data([], []),
        interval=50,
        blit=True,
        cache_frame_data=False
    )
    
    # 显示界面
    plt.tight_layout()
    plt.show()
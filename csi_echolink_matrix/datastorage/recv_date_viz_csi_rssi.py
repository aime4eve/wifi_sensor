import socket
import threading
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import math
import ast
from collections import deque
from config import CSI_DATA_COLUMNS_NAMES

# 配置参数
UDP_IP = "192.168.99.55"
UDP_PORT = 4444
MAX_POINTS = 64  # 最大显示点数


# 全局数据结构
global_data = {
    'raw_packet': None,
    'csi_magnitude': deque(maxlen=MAX_POINTS),
    'rssi_values': deque(maxlen=MAX_POINTS),
    'lock': threading.Lock()
}

# --------------------------------------------------
# 增强型UDP接收器
# --------------------------------------------------
def udp_receiver():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", UDP_PORT))
    sock.settimeout(0.1)

    while True:
        try:
            data, addr = sock.recvfrom(4096)
            
            # print(addr[0])
            # if addr[0] != UDP_IP:
            #     continue
                
            # 数据预处理
            decoded = data.decode().strip().replace('"','')
            if not decoded.startswith("CSI_DATA"):
                continue

            # 结构化解析
            parts = decoded.split(',', len(CSI_DATA_COLUMNS_NAMES)-1)
            if len(parts) != len(CSI_DATA_COLUMNS_NAMES):
                continue

            # 提取关键字段
            packet = dict(zip(CSI_DATA_COLUMNS_NAMES, parts))
            try:
                # 解析CSI数据
                csi_str = packet['data'].strip('[]')
                csi_values = [float(x) for x in csi_str.split(',')]
                # 提取RSSI
                rssi = float(packet['rssi'])
                
                with global_data['lock']:
                    global_data['raw_packet'] = {
                        'csi': csi_values,
                        'rssi': rssi
                    }
                    
            except (ValueError, KeyError) as e:
                print(f"解析错误: {str(e)}")

        except socket.timeout:
            pass
        except Exception as e:
            print(f"接收错误: {str(e)}")

# --------------------------------------------------
# 数据处理流水线
# --------------------------------------------------
def data_processor():
    while True:
        with global_data['lock']:
            raw = global_data['raw_packet']
        
        if raw:
            # CSI数据处理
            csi_pairs = []
            csi_data = raw['csi']
            for i in range(0, len(csi_data)-1, 2):
                try:
                    imag = float(csi_data[i])
                    real = float(csi_data[i+1])
                    csi_pairs.append(math.sqrt(imag**2 + real**2))
                except (IndexError, ValueError):
                    continue
            
            # 更新全局数据
            with global_data['lock']:
                if csi_pairs:
                    global_data['csi_magnitude'].extend(csi_pairs)
                global_data['rssi_values'].append(raw['rssi'])
                
            global_data['raw_packet'] = None
            
        threading.Event().wait(0.01)

# --------------------------------------------------
# 可视化系统
# --------------------------------------------------
def init_plots():
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))
    
    # CSI幅度图
    csi_line, = ax1.plot([], [], 'b-', lw=1)
    ax1.set_xlim(0, MAX_POINTS)
    ax1.set_ylim(0, 20)
    ax1.set_title("CSI Amplitude")
    ax1.grid(True)
    
    # RSSI实时图
    rssi_line, = ax2.plot([], [], 'r-', lw=1.5)
    ax2.set_xlim(0, MAX_POINTS)
    ax2.set_ylim(-100, -30)
    ax2.set_title("RSSI Variation")
    ax2.grid(True)
    
    plt.tight_layout()
    return fig, ax1, ax2, csi_line, rssi_line

def update_plots(frame):
    # 获取最新数据
    with global_data['lock']:
        csi_data = list(global_data['csi_magnitude'])
        rssi_data = list(global_data['rssi_values'])
    
    # 更新CSI幅度图
    csi_len = len(csi_data)
    if csi_len > 0:
        ax1.set_xlim(0, csi_len)
        ax1.set_ylim(0, max(csi_data)*1.2 if csi_data else 20)
        csi_line.set_data(range(csi_len), csi_data)
    
    # 更新RSSI图
    rssi_len = len(rssi_data)
    if rssi_len > 0:
        ax2.set_xlim(0, rssi_len)
        ax2.set_ylim(min(rssi_data)-5 if rssi_data else -100, 
                    max(rssi_data)+5 if rssi_data else -30)
        rssi_line.set_data(range(rssi_len), rssi_data)
    
    return csi_line, rssi_line

# --------------------------------------------------
# 主程序
# --------------------------------------------------
if __name__ == "__main__":

    # 启动数据接收线程
    threading.Thread(target=udp_receiver, daemon=True).start()
    
    # 启动数据处理线程
    threading.Thread(target=data_processor, daemon=True).start()
    
    # 初始化可视化
    fig, ax1, ax2, csi_line, rssi_line = init_plots()
    
    # 启动动画系统
    ani = animation.FuncAnimation(
        fig, update_plots,
        # init_func=lambda: (csi_line.set_data([], []), 
        interval=50,
        blit=True
    )
    
    # 显示界面
    plt.show()
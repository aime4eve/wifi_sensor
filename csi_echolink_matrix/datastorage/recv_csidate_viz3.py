import socket
import threading
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import matplotlib.gridspec as gridspec
import math
import numpy as np
from collections import deque
from config import CSI_DATA_COLUMNS_NAMES

import pandas as pd
from hampel import hampel
from scipy.signal import savgol_filter


# 配置参数
UDP_IP = "192.168.43.6"#"192.168.99.55"
UDP_PORT = 4444
CSI_DATA_LEN = 128

MAX_CACHE_FRAME = 1
FRONT_INVAILD = 12
END_INVAILD = 10
MAX_POINTS = int((CSI_DATA_LEN - FRONT_INVAILD - END_INVAILD)/2) # 最大显示点数
# MAX_POINTS = int(CSI_DATA_LEN/2)


# 全局数据结构
global_data = {
    'raw_packet': None,
    'csi_magnitude': deque(maxlen=MAX_POINTS*MAX_CACHE_FRAME),
    'imag_values': deque(maxlen=MAX_POINTS*MAX_CACHE_FRAME),
    'real_values': deque(maxlen=MAX_POINTS*MAX_CACHE_FRAME),
    'rssi_values': deque(maxlen=MAX_POINTS*MAX_CACHE_FRAME),
    'lock': threading.Lock()
}

def csidata_noise_filter(data):
    # 参数说明
    #data 为1个包的csi数据
    # print(data)
    amplitude_df = pd.DataFrame(data)
    amp_np = amplitude_df.to_numpy().T
    
    # Hampel滤波
    result = hampel(amp_np[0], window_size=7, n_sigma=5.0)
    filtered_data = result.filtered_data

    # Savitzky - Golay滤波
    savgol_filtered = savgol_filter(filtered_data, window_length=5, polyorder=3)
    # return filtered_data,savgol_filtered
    return savgol_filtered

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

            print(data.decode())
            
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
                csi_values = csi_values[FRONT_INVAILD:-1*END_INVAILD]
                # csi_values[:-1] = csi_values[1:]
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
            imag_list = []
            real_list = []            
            csi_data = raw['csi']
            # print(csi_data)
            # Rotate data to the left
            # csi_data[:-1] = csi_data[1:]
            for i in range(0, len(csi_data)-1, 2):
                try:
                    imag = float(csi_data[i])
                    real = float(csi_data[i+1])
                    csi_pairs.append(math.sqrt(imag**2 + real**2))
                    # if math.sqrt(imag**2 + real**2) == 0:
                    #     print(f'csi_pairs index:{i},{i+1}')
                    #     print(csi_data)
                    imag_list.append(imag)
                    real_list.append(real)                    
                except (IndexError, ValueError):
                    continue
            
            # 更新全局数据
            with global_data['lock']:
                if csi_pairs:
                    global_data['csi_magnitude'].extend(csidata_noise_filter(csi_pairs))
                    global_data['imag_values'].extend(imag_list)
                    global_data['real_values'].extend(real_list)
                global_data['rssi_values'].append(raw['rssi'])
                
            global_data['raw_packet'] = None
            
        threading.Event().wait(0.01)

# --------------------------------------------------
# 可视化系统
# --------------------------------------------------
def init_plots():
    # fig, (ax1, ax2, ax3, ax4) = plt.subplots(4, 1, figsize=(12, 16))
    # 创建一个图形对象
    fig = plt.figure(figsize=(18, 8))

    # 使用GridSpec进行布局，将图形划分为2行，3列
    gs = gridspec.GridSpec(2, 3, figure=fig)
    
    # CSI幅度图
    ax1 = fig.add_subplot(gs[0, 0])
    csi_line, = ax1.plot([], [], 'b-', lw=1)
    ax1.set_xlim(0, MAX_POINTS*MAX_CACHE_FRAME)
    ax1.set_ylim(0, 20)
    ax1.set_title("CSI Amplitude")
    ax1.grid(True)

    # 热力图 - Imag
    ax2 = fig.add_subplot(gs[0, 1])
    imag_heatmap = ax2.imshow(np.zeros((1, MAX_POINTS*MAX_CACHE_FRAME)), cmap='hot', aspect='auto', vmin=0, vmax=20)
    ax2.set_title("Phase Heatmap")
    plt.colorbar(imag_heatmap, ax=ax2)
    
    # 热力图 - Real
    ax3 = fig.add_subplot(gs[0, 2])
    real_heatmap = ax3.imshow(np.zeros((1, MAX_POINTS*MAX_CACHE_FRAME)), cmap='hot', aspect='auto', vmin=0, vmax=20)
    ax3.set_title("Amplitude Heatmap")
    plt.colorbar(real_heatmap, ax=ax3)    
    
    # RSSI实时图
    ax4 = fig.add_subplot(gs[1, :])
    rssi_line, = ax4.plot([], [], 'r-', lw=1.5)
    ax4.set_xlim(0, MAX_POINTS)
    ax4.set_ylim(-100, -30)
    ax4.set_title("RSSI Variation")
    ax4.grid(True)


    
    plt.tight_layout()
    return fig, ax1, ax2, ax3, ax4, csi_line, rssi_line, imag_heatmap, real_heatmap

def update_plots(frame):
    # 获取最新数据
    with global_data['lock']:
        csi_data = list(global_data['csi_magnitude'])
        imag_data = list(global_data['imag_values'])
        real_data = list(global_data['real_values'])
        rssi_data = list(global_data['rssi_values'])
    
    # 更新CSI幅度图
    csi_len = len(csi_data)
    if csi_len > 0:
        ax1.set_xlim(0, csi_len)
        ax1.set_ylim(0, max(csi_data)*1.2 if csi_data else 20)
        csi_line.set_data(range(csi_len), csi_data)
       
    
    # 更新Imag热力图
    if imag_data:
        imag_2d = np.array(imag_data).reshape(1, -1)
        # print("Imag 2D data:", imag_2d)  # 调试：打印二维数据
        imag_heatmap.set_data(imag_2d)
        imag_heatmap.set_clim(vmin=np.min(imag_2d), vmax=np.max(imag_2d))  # 动态调整范围
        imag_heatmap.autoscale()
    
    # 更新Real热力图
    if real_data:
        real_2d = np.array(real_data).reshape(1, -1)
        # print("Real 2D data:", real_2d)  # 调试：打印二维数据
        real_heatmap.set_data(real_2d)
        real_heatmap.set_clim(vmin=np.min(real_2d), vmax=np.max(real_2d))  # 动态调整范围
        real_heatmap.autoscale()

    # 更新RSSI图
    rssi_len = len(rssi_data)
    if rssi_len > 0:
        ax4.set_xlim(0, rssi_len)
        ax4.set_ylim(min(rssi_data)-5 if rssi_data else -100, 
                    max(rssi_data)+5 if rssi_data else -30)
        rssi_line.set_data(range(rssi_len), rssi_data)    

    return csi_line, rssi_line, imag_heatmap, real_heatmap

# --------------------------------------------------
# 主程序
# --------------------------------------------------
if __name__ == "__main__":

    # 启动数据接收线程
    threading.Thread(target=udp_receiver, daemon=True).start()
    
    # 启动数据处理线程
    threading.Thread(target=data_processor, daemon=True).start()
    
    # 初始化可视化
    fig, ax1, ax2, ax3, ax4, csi_line, rssi_line, imag_heatmap, real_heatmap = init_plots()
    
    # 启动动画系统
    ani = animation.FuncAnimation(
        fig, update_plots,
        # init_func=lambda: (csi_line.set_data([], []), 
        interval=100,
        save_count=MAX_CACHE_FRAME*10,
        blit=True
    )
    
    # 显示界面
    plt.show()
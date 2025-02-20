'''
写一段python代码，实现：
一、数据获取功能
1、连接UDP服务器，服务器IP地址为 "192.168.99.55"，通信端口为4444；
2、将接收到的数据保存到全局变量csi_raw_data中；
二、数据定义说明
1、csi_raw_data的数据格式定义为：
CSI_DATA_COLUMNS_NAMES = ["type", "id", "mac", "rssi", "rate", "sig_mode", "mcs", "bandwidth", "smoothing", "not_sounding", "aggregation", "stbc", "fec_coding",
                      "sgi", "noise_floor", "ampdu_cnt", "channel", "secondary_channel", "local_timestamp", "ant", "sig_len", "rx_state", "len", "first_word", "data"]
2、csi_raw_data的样例数据为：
csidata_sample = 'CSI_DATA,19003,24:ec:4a:04:7a:0d,-49,11,1,7,1,1,1,0,0,0,1,-95,0,5,2,211923676,0,83,0,128,0,"[0,0,0,0,0,0,0,0,0,0,0,0,9,-1,10,-1,9,-1,9,-1,9,-1,9,-1,8,-1,8,-1,8,-2,8,-2,7,-2,7,-2,6,-3,6,-3,6,-3,6,-4,6,-4,5,-5,5,-5,5,-5,5,-6,5,-6,5,-7,5,-7,6,-7,0,0,6,-8,6,-8,6,-8,7,-8,7,-7,7,-7,7,-7,7,-7,7,-7,8,-7,7,-6,7,-6,7,-6,7,-6,7,-5,7,-5,7,-5,7,-5,7,-5,7,-4,7,-4,7,-4,7,-4,7,-3,7,-3,7,-3,7,-2,0,0,0,0,0,0,0,0,0]"'
3、样例数据中，如下部分为csi_data数据。
"[0,0,0,0,0,0,0,0,0,0,0,0,9,-1,10,-1,9,-1,9,-1,9,-1,9,-1,8,-1,8,-1,8,-2,8,-2,7,-2,7,-2,6,-3,6,-3,6,-3,6,-4,6,-4,5,-5,5,-5,5,-5,5,-6,5,-6,5,-7,5,-7,6,-7,0,0,6,-8,6,-8,6,-8,7,-8,7,-7,7,-7,7,-7,7,-7,7,-7,8,-7,7,-6,7,-6,7,-6,7,-6,7,-5,7,-5,7,-5,7,-5,7,-5,7,-4,7,-4,7,-4,7,-4,7,-3,7,-3,7,-3,7,-2,0,0,0,0,0,0,0,0,0]"                      
4、csi_data数据结构定义为：[i,r,..]，其中i为虚部，r为实部。i和r成对出现;
三、数据展示功能
1、从csi_raw_data中获取csi_data数据，计算出对应的模值，保存到viz_cis_data变量中，并用折线图动态刷新展示；
2、要能根据从udp服务器获得的csi_raw_data来动态刷新这个折线图。

5、将viz_raw_data中每对i,r，计算出对应的模值，保存到viz_data变量中；
6、将viz_data变量用折线图的方式动态展示出来；
7、要能根据从udp服务器获得的csi_raw_data来动态刷新这个折线图。


'''
import socket
import threading
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.gridspec import GridSpec
import math
from collections import deque

# ========================
# 配置参数
# ========================
UDP_IP = "192.168.99.55"
UDP_PORT = 4444
MAX_POINTS = 64               # 最大显示数据点数, 64组载波
REFRESH_INTERVAL = 100         # 界面刷新间隔(ms)
TABLE_COLS = ['local_timestamp', 'mac', 'rate', 'mcs', 'channel', 'rssi', 'noise_floor', 'bandwidth']  # 表格显示列
CSI_COLUMNS = [                # CSI数据结构定义
    "type", "id", "mac", "rssi", "rate", "sig_mode", "mcs", "bandwidth",
    "smoothing", "not_sounding", "aggregation", "stbc", "fec_coding",
    "sgi", "noise_floor", "ampdu_cnt", "channel", "secondary_channel",
    "local_timestamp", "ant", "sig_len", "rx_state", "len", "first_word", "data"
]

# ========================
# 全局数据结构
# ========================
global_data = {
    'csi_mag': deque(maxlen=MAX_POINTS),
    'rssi': deque(maxlen=MAX_POINTS),
    'params': {col: 'N/A' for col in TABLE_COLS},
    'lock': threading.Lock(),
    'table_initialized': False,
    'table_cells': {}
}

# ========================
# UDP数据接收线程
# ========================
def udp_receiver():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", UDP_PORT))
    sock.settimeout(0.1)

    while True:
        try:
            data, addr = sock.recvfrom(1024)
            # if addr[0] != UDP_IP:
            #     continue
            # print(data)

            # 数据预处理
            decoded = data.decode().strip().replace('"', '')
            if not decoded.startswith("CSI_DATA"):
                continue

            # 结构化解析
            parts = decoded.split(',', len(CSI_COLUMNS)-1)
            if len(parts) != len(CSI_COLUMNS):
                continue

            packet = dict(zip(CSI_COLUMNS, parts))
            
            try:
                # 解析CSI数据
                csi_str = packet['data'].strip('[]')
                csi_pairs = [float(x) for x in csi_str.split(',')]
                
                # 计算幅度
                magnitudes = []
                for i in range(0, len(csi_pairs)-1, 2):
                    try:
                        imag = csi_pairs[i]
                        real = csi_pairs[i+1]
                        mag = math.sqrt(imag**2 + real**2)
                        # if real < 0:
                        #     mag = mag * -1
                        magnitudes.append(mag)
                    except IndexError:
                        continue
                
                # 提取参数
                params = {
                    'local_timestamp': packet['local_timestamp'],
                    'mac': packet['mac'][:17],  # MAC地址截断
                    'rate': f"{packet['rate']} Mbps",
                    'mcs': packet['mcs'],
                    'channel': packet['channel'],
                    'rssi': f"{packet['rssi']} dBm",
                    'noise_floor': f"{packet['noise_floor']} dBm",
                    'bandwidth': f"{packet['bandwidth']} MHz"
                }
                

                # 更新全局数据
                with global_data['lock']:
                    if magnitudes:
                        global_data['csi_mag'].extend(magnitudes)
                    global_data['rssi'].append(float(packet['rssi']))
                    global_data['params'] = params
                    # print(params)

            except (ValueError, KeyError) as e:
                print(f"数据解析错误: {str(e)}")

        except socket.timeout:
            pass
        except Exception as e:
            print(f"网络错误: {str(e)}")

# ========================
# 可视化系统
# ========================
def init_plots():
    fig = plt.figure(figsize=(16, 9), dpi=100)
    gs = GridSpec(3, 1, figure=fig, height_ratios=[2, 2, 1.5])
    
    # CSI幅度图
    ax1 = fig.add_subplot(gs[0])
    csi_line, = ax1.plot([], [], 'deepskyblue', lw=1)
    ax1.set_title("CSI Amplitude", fontsize=12, pad=10)
    ax1.grid(True, linestyle=':', alpha=0.7)
    ax1.set_ylabel("Amplitude (dB)", fontsize=10)
    
    # RSSI曲线图
    ax2 = fig.add_subplot(gs[1])
    rssi_line, = ax2.plot([], [], 'tomato', lw=1.2)
    ax2.set_title("RSSI Variation", fontsize=12, pad=10)
    ax2.grid(True, linestyle=':', alpha=0.7)
    ax2.set_ylabel("RSSI (dBm)", fontsize=10)
    
     # 初始化表格结构
    ax3 = fig.add_subplot(gs[2])
    ax3.axis('off')
    table = ax3.table(
        cellText=[[global_data['params'][col] for col in TABLE_COLS]],
        colLabels=TABLE_COLS,
        colColours=['#F0F0F0']*len(TABLE_COLS),
        cellLoc='center',
        loc='center'
    )
    table.auto_set_font_size(False)
    table.set_fontsize(11)
    table.scale(1, 2.2)
    
    # 存储表格对象到全局变量
    # 存储单元格引用
    for (row, col), cell in table.get_celld().items():
        if row == 0:  # 标题行
            cell.set_text_props(weight='bold', color='black')
            cell.set_facecolor('#E0E0E0')
        else:  # 数据行
            global_data['table_cells'][col] = cell
            # print(col)
    
    global_data['table_initialized'] = True
    
    plt.subplots_adjust(hspace=0.35, left=0.05, right=0.95)
    return fig, ax1, ax2, ax3, csi_line, rssi_line, table

# ========================
# 可视化更新
# ========================
def update_plots(frame):
    with global_data['lock']:
        csi_data = list(global_data['csi_mag'])
        rssi_data = list(global_data['rssi'])
        params = global_data['params'].copy()
        # print(params)
    
    # 更新CSI幅度图
    if csi_data:
        ax1.set_xlim(0, len(csi_data))
        ax1.set_ylim(min(csi_data)*1.2, max(csi_data)*1.2)
        csi_line.set_data(range(len(csi_data)), csi_data)
    
    # 更新RSSI图
    if rssi_data:
        ax2.set_xlim(0, len(rssi_data))
        ax2.set_ylim(min(rssi_data)-5, max(rssi_data)+5)
        rssi_line.set_data(range(len(rssi_data)), rssi_data)
    
    # 更新参数表格
    # print(params)
    # table = ax3.tables[0]
    # for col_idx, col in enumerate(TABLE_COLS):
    # #     table.get_celld()[(0, col_idx)].get_text().set_text(col)
    #     table.get_celld()[(1, col_idx)].get_text().set_text(params[col])
    if global_data['table_initialized']:
        plt.ion()
        # print(global_data['table_cells'].get(0).get_text())
        for col in range(len(TABLE_COLS)):
            # cell = global_data['table_cells'].get(col)
            cell = global_data['table_cells'][col]
            # print(cell)
            if cell:
                cell.get_text().set_text(params.get(col, 'N/A'))
                # print(params.get(col, 'N/A'))
        plt.ioff()
                
    # if global_data['table']:
    #     try:
    #         # 清除旧表格
    #         global_data['table'].remove()
    #     except ValueError:
    #         pass
        
    #     # 创建新表格
    #     print(params)
    #     table = ax3.table(
    #         cellText=[[params[col] for col in TABLE_COLS]],
    #         # cellText=table_data,
    #         colLabels=TABLE_COLS,
    #         colColours=['#F0F0F0']*len(TABLE_COLS),
    #         cellLoc='center',
    #         loc='center'
    #     )
    #     table.auto_set_font_size(False)
    #     table.set_fontsize(11)
    #     table.scale(1, 2.2)

        
        # # 更新全局表格引用
        # global_data['table'] = table

    
    return csi_line, rssi_line

# ========================
# 主程序
# ========================
if __name__ == "__main__":
    # 启动UDP接收线程
    recv_thread = threading.Thread(target=udp_receiver, daemon=True)
    recv_thread.start()
    
    # 初始化可视化
    fig, ax1, ax2, ax3, csi_line, rssi_line, table = init_plots()
    
    # 启动动画
    ani = animation.FuncAnimation(
        fig, update_plots,
        # init_func=lambda: (csi_line.set_data([], []), rssi_line.set_data([], [])),
        interval=REFRESH_INTERVAL,
        blit=True,
        cache_frame_data=False
    )
    
    # 显示界面
    plt.show()
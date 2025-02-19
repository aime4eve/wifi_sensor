from config import *
import numpy as np
import threading
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque




# CSI数据样例
csidata_sample = 'CSIDATA,19003,24:ec:4a:04:7a:0d,-49,11,1,7,1,1,1,0,0,0,1,-95,0,5,2,211923676,0,83,0,128,0,"[0,0,0,0,0,0,0,0,0,0,0,0,9,-1,10,-1,9,-1,9,-1,9,-1,9,-1,8,-1,8,-1,8,-2,8,-2,7,-2,7,-2,6,-3,6,-3,6,-3,6,-4,6,-4,5,-5,5,-5,5,-5,5,-6,5,-6,5,-7,5,-7,6,-7,0,0,6,-8,6,-8,6,-8,7,-8,7,-7,7,-7,7,-7,7,-7,7,-7,8,-7,7,-6,7,-6,7,-6,7,-6,7,-5,7,-5,7,-5,7,-5,7,-5,7,-4,7,-4,7,-4,7,-4,7,-3,7,-3,7,-3,7,-2,0,0,0,0,0,0,0,0,0]"'

def parse_csi_data(csi_data):
    """
    解析 CSI 数据
    :param csi_data_tile: 数据格式定义
    :param csi_data: 接收到的 CSI 数据
    :return: 解析后的字典
    """
    # 去除换行符并分割数据
    csi_data = csi_data.strip()
    csi_values = csi_data.split(',')

    # 分割数据格式定义
    fields = CSI_DATA_FIELD.split(',')

    # 解析数据
    parsed_data = {}
    for i, field in enumerate(fields):
        if i < len(csi_values):
            # 处理 data 字段（字符串转换为列表）
            if field == 'data':
                value = csi_values[i:][1:-1]
            else:
                value = csi_values[i]
            # 将值存储到字典中
            parsed_data[field] = value

    return parsed_data


# # 解析 CSI 数据
# parsed_csi = parse_csi_data(csidata_sample)

# # 打印解析结果
# print("解析后的 CSI 数据:")
# for key, value in parsed_csi.items():
#     print(f"{key}: {value}")

# csi_data_str = str(parsed_csi['data'])
# csi_data_str = csi_data_str.strip('[')
# csi_data_str = csi_data_str.strip(']')
# csi_data_str = csi_data_str.split(',')
# l = list(csi_data_str)
# print(l)




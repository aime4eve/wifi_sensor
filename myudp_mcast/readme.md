1、ESP32S3芯片A：实现WiFi STA功能（相当于网络路由器）和UDP Multicast功能

2、ESP32S3芯片B：实现WiFi STA功能（相当于网络路由器）和UDP Multicast功能

3、无线路由器C（例如：MyRouter）：实现芯片A和芯片B组网

![image](https://github.com/user-attachments/assets/bc18532c-e6e2-4dc8-9f8a-f5cadfaea502)


具备A和B间通过UDP Multicast相互通信的能力。用UDP Multicast协议可以大大简化芯片的组网配置工作，但要求无线路由器C要能做好网络覆盖。

# BT Speaker App (M1-Android)

Android 控制 App：通过 USB OTG 串口连接 ESP32 蓝牙音箱（CH340/CP210x，115200）。

## 构建

用 Android Studio 打开 `sperker-APP/app-android`，Sync 后运行到手机（minSdk 26）。

## 使用

1. 用 OTG 线连接手机与音箱（音箱可电池供电，USB 口仅串口）。
2. 首次连接会请求 USB 权限，允许后 App 自动枚举 CH340/CP210x 设备。
3. 点击「连接」→ 进入调试中心。
4. 没有真机时点「模拟连接」→ 以模拟音箱进入已连接状态，所有控件可交互（固件未实现的功能会标注）。

## 安装 APK

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

（或在 Android Studio 里直接 Run；也可把 APK 拷到手机安装，需允许未知来源。）

## 协议层单测（无需 Android SDK）

```bash
cd sperker-## 验证清单

- [ ] 无真机：点「模拟连接」进入已连接，滑块可拖、松手提交、回弹正常
- [ ] OTG 连接后弹出 USB 授权，允许后 App 识别 CH340/CP210x
- [ ] 点「连接」进入已连接，状态区显示 ready 固件版本
- [ ] 主音量 / 左右声道增益 / 平衡滑块与旋钮双向联动（事件刷新）
- [ ] 自定义 EQ 5 段（60/250/1k/4k/12k Hz）±12 dB 生效
- [ ] TF 卡信息显示（挂载 / 容量 / 字体 / 动画帧）
- [ ] 拔掉 OTG 后状态断开，重新插上可重连
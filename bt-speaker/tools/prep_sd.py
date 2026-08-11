#!/usr/bin/env python3
# ============================================================
# 准备 TF 卡内容：中文字体（HZK16 + HZK12）+ 演示动画帧。
# 用法:  python tools/prep_sd.py <TF卡盘符>
# 例:    python tools/prep_sd.py E:
#
# 特点:
#   - 完全离线：用 Pillow 从 Windows 系统字体(黑体/宋体)生成点阵字体
#   - 要求目标为 FAT32（ESP32 FATFS 不支持 exFAT/NTFS）
#   - 幂等：重复运行会覆盖同名文件
# ============================================================
import argparse
import ctypes
import os
import sys
import time

from PIL import Image, ImageDraw, ImageFont

# ---------------- 配置 ----------------
GRID = 94 * 94                      # GB2312 区×位 网格
CELL16, SLOT16 = 16, 32             # HZK16: 16×16，每字 32 字节（16 行 × 2B）
CELL12, SLOT12 = 12, 24             # HZK12: 12×12，每字 24 字节（12 行 × 2B，行内 16 位左对齐）

FONT_CANDIDATES = [
    "C:/Windows/Fonts/simhei.ttf",       # 黑体（首选）
    "C:/Windows/Fonts/simsun.ttc",       # 宋体（TTC 集合，index=0）
    "C:/Windows/Fonts/msyh.ttc",         # 微软雅黑
]

ANIM_FRAMES = 3                     # 演示动画帧数

README = """ESP32 蓝牙音箱 — TF 卡资源目录约定
====================================
本卡按以下约定存放音箱资源，播放/渲染逻辑须与生成脚本保持一致。

/fonts/hzk16.bin    GB2312 点阵字体 16x16（HZK16 格式）
  尺寸 : 94x94x32 = 282752 字节
  索引 : slot = ((区码-1)*94 + (位码-1))，区/位码为 GB2312 区码/位码
  每字 : 32 字节 = 16 行 x 2 字节，行内 MSB 最左（bit15 = 第 0 列）
  字形 : 在 16x16 格内居中（由 prep_sd.py 用系统字体生成）

/fonts/hzk12.bin    GB2312 点阵字体 12x12（HZK12 格式）
  尺寸 : 94x94x24 = 212064 字节
  每字 : 24 字节 = 12 行 x 2 字节（每行 12 位左对齐，低 4 位为 0）

/anim/*.bin         演示动画帧（128x64 SSD1306 页格式）
  每帧 : 128x8 = 1024 字节，8 页 x 128 列，字节内 bit0 = 该页最上像素
  命名 : frame_000.bin 起按序号排列

生成方式 : python tools/prep_sd.py <盘符>
要求     : FAT32 文件系统（>32GB 卡需重新格式化）
"""


# ---------------- 工具函数 ----------------
def get_fs_type(drive_root):
    """返回卷文件系统名（如 'FAT32' / 'NTFS' / 'exFAT'）。"""
    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    func = kernel32.GetVolumeInformationW
    func.argtypes = [
        ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_uint32,
        ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint32),
        ctypes.POINTER(ctypes.c_uint32), ctypes.c_wchar_p, ctypes.c_uint32,
    ]
    func.restype = ctypes.c_int
    fs = ctypes.create_unicode_buffer(64)
    ok = func(drive_root, None, 0, None, None, None, fs, 64)
    return fs.value if ok else None


def load_font(size):
    """载入一个系统 CJK 字体；返回 (font, 名称)，失败返回 None。"""
    for path in FONT_CANDIDATES:
        if not os.path.exists(path):
            continue
        try:
            font = ImageFont.truetype(path, size, index=0)
            return font, os.path.basename(path)
        except Exception:
            continue
    return None


def render_rows(ch, cell, font):
    """把单个汉字渲染为 cell 行、每行一个 16 位值（MSB 最左）。"""
    img = Image.new('L', (cell, cell), 0)
    d = ImageDraw.Draw(img)
    bbox = d.textbbox((0, 0), ch, font=font)
    iw = bbox[2] - bbox[0]
    ih = bbox[3] - bbox[1]
    x = (cell - iw) // 2 - bbox[0]
    y = (cell - ih) // 2 - bbox[1]
    d.text((x, y), ch, font=font, fill=255)
    rows = []
    for yy in range(cell):
        v = 0
        for xx in range(cell):
            if img.getpixel((xx, yy)) >= 128:
                v |= 0x8000 >> xx
        rows.append(v)
    return rows


def put_glyph(buf, slot, rows, slot_bytes, cell):
    idx = slot * slot_bytes
    for row in range(cell):
        v = rows[row]
        buf[idx + row * 2]     = (v >> 8) & 0xFF
        buf[idx + row * 2 + 1] = v & 0xFF


def generate_fonts(font16, font12):
    """生成 HZK16 与 HZK12 两个完整 94x94 网格。返回 (bytes16, bytes12)。"""
    buf16 = bytearray(GRID * SLOT16)
    buf12 = bytearray(GRID * SLOT12)
    t0 = time.time()
    for qu in range(1, 95):
        for wei in range(1, 95):
            hi = 0xA0 + qu
            lo = 0xA0 + wei
            try:
                ch = bytes([hi, lo]).decode('gb2312')
            except UnicodeDecodeError:
                continue                      # 保留区 → 零填充
            slot = (qu - 1) * 94 + (wei - 1)
            put_glyph(buf16, slot, render_rows(ch, CELL16, font16), SLOT16, CELL16)
            put_glyph(buf12, slot, render_rows(ch, CELL12, font12), SLOT12, CELL12)
    print(f"  [font] 生成完成 用时 {time.time()-t0:.1f}s")
    return bytes(buf16), bytes(buf12)


def generate_anim_frames(count):
    """生成 count 帧 128x64 SSD1306 页格式：全高竖条从右向左扫过。"""
    frames = []
    for i in range(count):
        buf = bytearray(128 * 8)
        bar_x = 20 + i * ((128 - 20 - 8) // max(1, count - 1))
        for x in range(bar_x, min(128, bar_x + 8)):
            for page in range(8):
                buf[page * 128 + x] = 0xFF
        frames.append(bytes(buf))
    return frames


def check_font(font_bytes, slot_bytes, cell, ch):
    """读回某汉字槽，确认非空（自检 MSB/打包方向）。"""
    try:
        gb = ch.encode('gb2312')
        hi, lo = gb[0], gb[1]      # 已是字节整数
    except Exception:
        return False
    slot = ((hi - 0xA1) * 94 + (lo - 0xA1)) * slot_bytes
    data = font_bytes[slot:slot + slot_bytes]
    return any(b != 0 for b in data)


# ---------------- 主流程 ----------------
def main():
    ap = argparse.ArgumentParser(description='准备 TF 卡资源内容')
    ap.add_argument('target', help='TF 卡挂载目录/盘符，如 E: 或 E:\\\\')
    args = ap.parse_args()

    root = os.path.normpath(args.target)
    if not os.path.isdir(root):
        sys.exit(f"[错误] 目录不存在: {root}")

    # 1. FAT32 校验
    fs = get_fs_type(root.rstrip('\\') + '\\')
    ok_fs = {None, 'FAT', 'FAT12', 'FAT16', 'FAT32'}
    if fs and fs.upper() not in ok_fs:
        sys.exit(f"[错误] {root} 是 {fs}，不是 FAT 文件系统。"
                 f"请用 ≤32GB 的卡并格式化为 FAT32 后重试。")
    print(f"[1/5] 目标 {root}   文件系统: {fs or '未知'}")

    # 2. 载入系统字体
    font16 = load_font(CELL16)
    font12 = load_font(CELL12)
    if not font16 or not font12:
        sys.exit("[错误] 未找到可用的系统 CJK 字体（simhei/simsun/msyh）")
    print(f"[2/5] 字体: {font16[1]} / {font12[1]}")

    # 3. 建目录
    fonts_dir = os.path.join(root, 'fonts')
    anim_dir = os.path.join(root, 'anim')
    os.makedirs(fonts_dir, exist_ok=True)
    os.makedirs(anim_dir, exist_ok=True)
    with open(os.path.join(root, 'README.txt'), 'w', encoding='utf-8') as f:
        f.write(README)
    print("[3/5] 目录已就绪: /fonts /anim /README.txt")

    # 4. 生成字体 + 动画
    data16, data12 = generate_fonts(font16[0], font12[0])
    frames = generate_anim_frames(ANIM_FRAMES)
    with open(os.path.join(fonts_dir, 'hzk16.bin'), 'wb') as f:
        f.write(data16)
    with open(os.path.join(fonts_dir, 'hzk12.bin'), 'wb') as f:
        f.write(data12)
    for i, fr in enumerate(frames):
        with open(os.path.join(anim_dir, f'frame_{i:03d}.bin'), 'wb') as f:
            f.write(fr)
    print("[4/5] 文件已写入")

    # 5. 自检 + 摘要
    ok16 = check_font(data16, SLOT16, CELL16, '中')
    ok12 = check_font(data12, SLOT12, CELL12, '中')
    print("[5/5] 自检: HZK16=%s HZK12=%s" % ("OK" if ok16 else "FAIL",
                                              "OK" if ok12 else "FAIL"))
    if not (ok16 and ok12):
        sys.exit("[错误] 字体自检失败，请检查字体渲染/打包逻辑。")
    print()
    print("完成。资源清单：")
    print(f"  fonts/hzk16.bin   = {os.path.getsize(os.path.join(fonts_dir, 'hzk16.bin'))} 字节")
    print(f"  fonts/hzk12.bin   = {os.path.getsize(os.path.join(fonts_dir, 'hzk12.bin'))} 字节")
    print(f"  anim/             = {len(frames)} 帧演示动画")
    print(f"  README.txt        = 格式约定")


if __name__ == '__main__':
    main()

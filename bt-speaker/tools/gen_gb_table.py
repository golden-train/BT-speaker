#!/usr/bin/env python3
# ============================================================
# 生成 UTF-8 → GB2312(区位码) 查询表 → src/ui/gb_table.h
# 遍历 GB2312 94×94 网格，解码每个区位得 Unicode，按 Unicode 升序输出。
# 固件用二分查找把 UTF-8 码点映射到 HZK16 偏移。勿手改生成文件。
# ============================================================
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'ui', 'gb_table.h')

entries = []
for qu in range(94):
    for wei in range(94):
        b = bytes([0xA1 + qu, 0xA1 + wei])
        try:
            ch = b.decode('gb2312')
        except UnicodeDecodeError:
            continue
        entries.append((ord(ch), qu, wei))

# 去重（同一 Unicode 可能落在多个区位）
uniq, seen = [], set()
for u, qu, wei in sorted(entries):
    if u not in seen:
        seen.add(u)
        uniq.append((u, qu, wei))

with open(OUT, 'w', encoding='utf-8') as f:
    f.write('// 自动生成：UTF-8 → GB2312 区位码查询表（按 Unicode 升序，二分查找）。\n')
    f.write('// 生成脚本 tools/gen_gb_table.py，勿手改。\n')
    f.write('#pragma once\n#include <stdint.h>\n\n')
    f.write('// quwei 高字节=区(0..93)、低字节=位(0..93)，HZK16 偏移 = (qu*94+wei)*32\n')
    f.write('struct GbEntry { uint16_t uni; uint16_t quwei; };\n\n')
    f.write('constexpr GbEntry kGbTable[] = {\n')
    for u, qu, wei in uniq:
        f.write('  {0x%04X, 0x%04X},\n' % (u, (qu << 8) | wei))
    f.write('};\n')
    f.write('constexpr int kGbTableSize = %d;\n' % len(uniq))

print('生成 %s : %d 条' % (OUT, len(uniq)))

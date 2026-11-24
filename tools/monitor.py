#!/usr/bin/env python3
"""ESP32 串口日志工具 —— 带时间戳、支持自动复位抓启动日志。

用法:
    ./.venv/bin/python tools/monitor.py                  # 自动找板子，复位后持续打印
    ./.venv/bin/python tools/monitor.py --no-reset        # 只看不断电输出
    ./.venv/bin/python tools/monitor.py --seconds 10      # 抓 10 秒后自动退出
    ./.venv/bin/python tools/monitor.py --save boot.log   # 同时存文件（排查崩溃用）

退出: Ctrl+C
"""

import argparse
import glob
import re
import select
import sys
import time

import serial

USB_PORT_PATTERNS = [
    r"/dev/cu\.usbserial.*",
    r"/dev/cu\.wchusbserial.*",
    r"/dev/cu\.usbmodem.*",
    r"/dev/cu\.SLAB_USBtoUART.*",
]


def find_port() -> str | None:
    for pat in USB_PORT_PATTERNS:
        hits = sorted(glob.glob(pat))
        if hits:
            return hits[0]
    return None


def hard_reset(ser: serial.Serial) -> None:
    """ESP32 自动复位电路: 拉低 EN 再释放, 芯片从 flash 重新启动。"""
    ser.dtr = False
    ser.rts = True
    time.sleep(0.12)
    ser.rts = False
    time.sleep(0.05)


def main() -> int:
    ap = argparse.ArgumentParser(description="ESP32 串口日志")
    ap.add_argument("-p", "--port", help="串口设备, 默认自动探测")
    ap.add_argument("-b", "--baud", type=int, default=115200)
    ap.add_argument("--no-reset", action="store_true", help="不复位, 只读取当前输出")
    ap.add_argument("--seconds", type=float, default=0, help="抓取时长(秒), 0=直到 Ctrl+C")
    ap.add_argument("--save", help="把原始日志追加写入该文件")
    ap.add_argument("-q", "--quiet-input", action="store_true",
                    help="不转发键盘输入到板子（默认在终端里会自动转发）")
    ap.add_argument("--skip", type=float, default=0.8,
                    help="复位后先丢弃多少秒的数据（默认 0.8s，避开 boot ROM 的 74880 波特率噪音）")
    args = ap.parse_args()

    port = args.port or find_port()
    if not port:
        print("❌ 没找到 USB 串口。检查数据线/驱动，或用 -p 指定。", file=sys.stderr)
        return 1

    try:
        ser = serial.Serial(port, args.baud, timeout=0.2)
    except serial.SerialException as e:
        print(f"❌ 打开 {port} 失败: {e}", file=sys.stderr)
        print("   常见原因: 被 Arduino IDE / 其它终端占用; 或需要串口权限。", file=sys.stderr)
        return 1

    log = None
    if args.save:
        log = open(args.save, "a", encoding="utf-8", buffering=1)

    if not args.no_reset:
        hard_reset(ser)
        # boot ROM 用 74880 波特率打印，在 115200 下全是乱码；驱动还可能吐出
        # 上一次会话的残留缓冲。先丢掉这一小段，再开始正常输出。
        time.sleep(args.skip)
        ser.reset_input_buffer()

    # 只有在真实终端里才转发键盘输入：敲一行回车就发给板子
    interactive = sys.stdin.isatty() and not args.quiet_input
    banner = "输入命令后回车发送" if interactive else "只读模式"
    print(f"── {port} @ {args.baud} ── {banner}, Ctrl+C 退出", file=sys.stderr)
    t0 = time.monotonic()
    buf = b""
    try:
        while True:
            if args.seconds and time.monotonic() - t0 >= args.seconds:
                break

            # 键盘输入 → 串口
            if interactive:
                rlist, _, _ = select.select([sys.stdin], [], [], 0)
                if rlist:
                    typed = sys.stdin.readline()
                    if typed:
                        ser.write(typed.encode())
                        ser.flush()

            chunk = ser.read(4096)
            if not chunk:
                continue
            buf += chunk
            # 按行拆分; 保留不完整的一行等下次
            *lines, buf = re.split(rb"(?<=\n)", buf)
            for raw in lines:
                text = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                if not text:
                    continue
                ts = time.monotonic() - t0
                out = f"[{ts:7.2f}s] {text}"
                print(out, flush=True)
                if log:
                    log.write(text + "\n")
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        if log:
            log.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())

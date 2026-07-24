#!/usr/bin/env python3
# Keke OS System Info - Python example
import os, time

def get_uptime():
    try:
        with open('/proc/uptime') as f:
            uptime_sec = float(f.read().split()[0])
            days = int(uptime_sec // 86400)
            hours = int((uptime_sec % 86400) // 3600)
            mins = int((uptime_sec % 3600) // 60)
            secs = int(uptime_sec % 60)
            return f"{days}d {hours}h {mins}m {secs}s"
    except:
        return "N/A"

def get_memory():
    try:
        with open('/proc/meminfo') as f:
            lines = f.readlines()
            total = int(lines[0].split()[1]) // 1024
            free = int(lines[1].split()[1]) // 1024
            return f"{free}MB / {total}MB"
    except:
        return "N/A"

def main():
    print("\n=== Keke OS System Info (Python) ===")
    print(f"Uptime:  {get_uptime()}")
    print(f"Memory:  {get_memory()}")
    print(f"Host:    Keke OS v2.7.5")
    print(f"Python:  {os.sys.version}")
    print(f"Date:    {time.strftime('%Y-%m-%d %H:%M:%S')}")
    print("===================================\n")

if __name__ == '__main__':
    main()

#!/bin/sh
# Keke OS Shell Script Example
echo "============================================"
echo "  Keke OS Shell Script Demo"
echo "  Running on: $(uname -o 2>/dev/null || echo 'Keke OS')"
echo "  Date: $(date 2>/dev/null || echo 'N/A')"
echo "============================================"
echo ""
echo "Files in /mnt:"
ls -la /mnt/ 2>/dev/null || echo "  (no /mnt mounted)"
echo ""
echo "Files in /mnt/scripts:"
ls -la /mnt/scripts/ 2>/dev/null || echo "  (no scripts directory)"
echo ""
echo "System info via /proc:"
for f in version uptime loadavg; do
    echo -n "  /proc/$f: "
    cat /proc/$f 2>/dev/null || echo "N/A"
done
echo ""
echo "Goodbye from Keke OS Shell!"
echo "============================================"

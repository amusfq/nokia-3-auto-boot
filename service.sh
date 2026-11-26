#!/system/bin/sh
cd "${0%/*}" || exit 1
./poweron >/dev/null 2>&1 &

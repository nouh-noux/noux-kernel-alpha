#!/bin/bash
# هذا السكربت يضمن توافق تشغيل ملفات tyb على أنظمة لينكس ويونيكس

echo "[Gatek-OS Compatibility Layer] Launching .tyb program..."
# ترجمة ملف النواة وتحديثه
gcc noux.c -o noux

# تشغيل البرنامج المتوافق
./noux

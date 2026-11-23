#!/usr/bin/env bash
# Usage: bash Tools/run_build.sh <log-number>   (run from anywhere; use "cmd //c" because Git Bash rewrites "/c")
LOG="/c/Users/user/Desktop/wizardEPIC/.sdd/scratch/build_${1:-x}.log"
cmd.exe //c "C:\Users\user\Desktop\wizardEPIC\Tools\Build_Editor.bat" > "$LOG" 2>&1
CODE=$?
echo "exit=$CODE" >> "$LOG"
tail -6 "$LOG" | cut -c1-240
exit $CODE

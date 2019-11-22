#!/usr/bin/env bash
function loop_execute() {
  local cmd="$1"
  # interval(seconds)
  local interval="$2"
  while true; do
    eval $cmd
    if [ $interval -gt 0 ]; then
      sleep $interval
    fi
  done
}

if [ $# -eq 0 ]; then
  echo "please select from [mem, cpu, disk, network]"
else
  case $1 in
    "mem")
      CMD="free | head -n 2 | tail -n 1 | awk -v date=\"\$(date -Iseconds)\" -F \" \" '{print date,\$2,\$3,\$4,\$5,\$6,\$7}'"
      loop_execute "$CMD" $2
      ;;
    "cpu")
      CMD="mpstat $2 1"
      loop_execute "$CMD" 0
      ;;
  esac
fi

#!/bin/bash
LANG=lt_LT.utf8
# LANG=ru_RU.utf8
cal -3
./build/waybar -l debug --config test.config --style test.style.css

TASK
####

• Connect matrix keypad to BBB (directly or using breadboard)
• Implement driver for detecting pressed buttons
• Report detected buttons to kernel log (dmesg)
• Use work queue for scanning columns
• Obtain scan interval from device tree definition
• Perform reading rows on interrupt
• Configure debouncing for rows lines
• Use platform driver and device tree

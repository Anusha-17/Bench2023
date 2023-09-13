cmd_/home/anusha/ANUSHA/LDD/Udev/udev.mod := printf '%s\n'   udev.o | awk '!x[$$0]++ { print("/home/anusha/ANUSHA/LDD/Udev/"$$0) }' > /home/anusha/ANUSHA/LDD/Udev/udev.mod

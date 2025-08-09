savedcmd_serdev_echo.mod := printf '%s\n'   serdev_echo.o | awk '!x[$$0]++ { print("./"$$0) }' > serdev_echo.mod

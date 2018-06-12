@echo off
@echo.
@echo.
@cd %~dp0
@echo cd tools folder
@del rtthread.bin
@del rtthread.mki
@del rtthread-with-spl.bin
@del rtthread-with-tf.img
@echo mkimage...
@copy ..\rtthread.bin rtthread.bin
@mkimage.exe -A arm -T firmware -C none -O u-boot -a 0x43000000 -e 0 -n "RT-Thread for sunxi board" -d rtthread.bin rtthread.mki
@echo mksplbin...
@copy sunxi-spl.bin /b + rtthread.mki /b rtthread-with-spl.bin
@echo mktfimg...
@copy sunxi-tf.bin rtthread-with-tf.img
@dd.exe if=rtthread-with-spl.bin of=rtthread-with-tf.img bs=1k seek=8
@echo mk.cmd complete
@echo.
@echo.

import os

# toolchains options
ARCH        ='arm'
CPU         ='local'
CROSS_TOOL  ='gcc'

if os.getenv('RTT_CC'):
    CROSS_TOOL = os.getenv('RTT_CC')

if  CROSS_TOOL == 'gcc':
    PLATFORM    = 'gcc'
    EXEC_PATH   = '/opt/arm-2014.05/bin'
else:
    print 'Please make sure your toolchains is GNU GCC!'
    exit(0)

if os.getenv('RTT_EXEC_PATH'):
    EXEC_PATH = os.getenv('RTT_EXEC_PATH')

BUILD = 'debug'

if PLATFORM == 'gcc':
    # toolchains
    PREFIX  = 'arm-none-eabi-'
    CC      = PREFIX + 'gcc'
    CXX     = PREFIX + 'g++'
    AS      = PREFIX + 'gcc'
    AR      = PREFIX + 'ar'
    LINK    = PREFIX + 'gcc'
    TARGET_EXT = 'elf'
    SIZE    = PREFIX + 'size'
    OBJDUMP = PREFIX + 'objdump'
    OBJCPY  = PREFIX + 'objcopy'

    DEVICE  = ' -march=armv7-a -mtune=cortex-a7 -ftree-vectorize -ffast-math -mfloat-abi=softfp -msoft-float'
    DEVICE += ' -ffunction-sections -fdata-sections -fno-common -mno-unaligned-access -DCONFIG_USE_STDINT'
    DEVICE += ' -Iinclude -D__KERNEL__ -D__UBOOT__ -D__ARM__ -D__LINUX_ARM_ARCH__=7 -include include/linux/kconfig.h'
    CFLAGS  = DEVICE + ' -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable' 
    AFLAGS  = ' -c' + DEVICE + ' -x assembler-with-cpp'
    AFLAGS += ' -D__ASSEMBLY__ -DCONFIG_ARM_ASM_UNIFIED'
    LFLAGS  = DEVICE + ' -Wl,--gc-sections,-Map=rtthread.map,-cref,-u,system_vectors -T link.lds'
    CPATH   = ''
    LPATH   = ''

    if BUILD == 'debug':
        CFLAGS += ' -O0 -gdwarf-2'
        AFLAGS += ' -gdwarf-2'
    else:
        CFLAGS += ' -Os'

    CXXFLAGS = CFLAGS

POST_ACTION  = OBJCPY + ' -O binary $TARGET rtthread.bin \n'
POST_ACTION += SIZE + ' $TARGET \n' 
POST_ACTION += 'tools\\mk.cmd $TARGET \n'

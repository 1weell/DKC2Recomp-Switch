# Nintendo Switch toolchain for the DKC2Recomp SDL2 host.
# Configure from a devkitPro shell with DEVKITPRO and DEVKITA64 set.

set(CMAKE_SYSTEM_NAME NintendoSwitch)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "DEVKITPRO is not set; run this from a devkitPro shell")
endif()
if(NOT DEFINED ENV{DEVKITA64})
    message(FATAL_ERROR "DEVKITA64 is not set; install devkitA64 first")
endif()

set(DEVKITPRO "$ENV{DEVKITPRO}" CACHE PATH "devkitPro root")
set(DEVKITA64 "$ENV{DEVKITA64}" CACHE PATH "devkitA64 root")

set(CMAKE_C_COMPILER "${DEVKITA64}/bin/aarch64-none-elf-gcc")
set(CMAKE_ASM_COMPILER "${DEVKITA64}/bin/aarch64-none-elf-gcc")
set(CMAKE_AR "${DEVKITA64}/bin/aarch64-none-elf-ar")
set(CMAKE_RANLIB "${DEVKITA64}/bin/aarch64-none-elf-ranlib")
set(CMAKE_OBJCOPY "${DEVKITA64}/bin/aarch64-none-elf-objcopy")

set(CMAKE_C_FLAGS_INIT
    "-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -ftls-model=local-exec -ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-fPIE -specs=${DEVKITPRO}/libnx/switch.specs")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_FIND_ROOT_PATH
    "${DEVKITPRO}/portlibs/switch"
    "${DEVKITPRO}/libnx"
    "${DEVKITA64}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

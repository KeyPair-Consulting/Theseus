#ec2 gcc is now gcc-11
CC=gcc
RDRND=-mrdrnd

#gcc now explains how strncpy works. I know how it works.
MUTE=-Wno-stringop-truncation -Wno-format-nonliteral
CFLAGS=-std=gnu11 -O2 -Wall -m64 -march=native -Wextra -fstack-protector-all -Wstack-protector --param ssp-buffer-size=4 -Wsign-conversion -Wformat-security -Werror -pie -fPIE -ftrapv -fstrict-overflow -fwrapv -fno-delete-null-pointer-checks -Wshadow -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wdeclaration-after-statement -Wformat=2 -Wswitch-default -Wswitch-enum -Wcast-align -Wpointer-arith -Wstrict-overflow=5 -Wundef -Wnested-externs -Wunreachable-code -Wlogical-op -Wfloat-equal -Wstrict-aliasing -Wredundant-decls -Wold-style-definition -Wtrampolines -ggdb3 -fno-omit-frame-pointer -ffloat-store -fno-common -fPIC -fPIE -fstrict-aliasing -mbmi2 -mbmi -msse4.2 $(RDRND) -mfpmath=sse -msse2 $(MUTE) -DUADDL_OVERFLOW

include Makefile.rules

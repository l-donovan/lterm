OBJS = lterm.c
cc = gcc

COMPILER_FLAGS = -O3 -Wall

PKG_CONFIG_FLAGS = vte-2.91 libconfig

OBJ_NAME = lterm

all : $(OBJS)
	$(CC) $(OBJS) $(COMPILER_FLAGS) `pkg-config --libs --cflags $(PKG_CONFIG_FLAGS)` -o $(OBJ_NAME)

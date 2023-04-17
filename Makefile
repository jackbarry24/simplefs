# Configuration

CC		= gcc
LD		= gcc
AR		= ar
CFLAGS		= -g -std=gnu99 -Wall -Iinclude -fPIC
LDFLAGS		= -Llib
LIBS		= -lm
ARFLAGS		= rcs

# Variables

SFS_LIB_HDRS	= $(wildcard include/sfs/*.h)
SFS_LIB_SRCS	= src/disk.c src/fs.c
SFS_LIB_OBJS	= $(SFS_LIB_SRCS:.c=.o)
SFS_LIBRARY	= lib/libsfs.a

SFS_SHL_SRCS	= src/sfssh.c
SFS_SHL_OBJS	= $(SFS_SHL_SRCS:.c=.o)
SFS_SHELL	= bin/sfssh

all:		$(SFS_LIBRARY) $(SFS_SHELL)

%.o:		%.c $(SFS_LIB_HDRS)
	@echo "Compiling $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(SFS_LIBRARY):	$(SFS_LIB_OBJS)
	@echo "Linking   $@"
	@$(AR) $(ARFLAGS) $@ $^

$(SFS_SHELL):	$(SFS_SHL_OBJS) $(SFS_LIBRARY)
	@echo "Linking   $@"
	@$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)


clean:
	@echo "Removing  objects"
	@rm -f $(SFS_LIB_OBJS) $(SFS_SHL_OBJS) $(SFS_TEST_OBJS)

	@echo "Removing  libraries"
	@rm -f $(SFS_LIBRARY)

	@echo "Removing  programs"
	@rm -f $(SFS_SHELL)

.PRECIOUS: %.o
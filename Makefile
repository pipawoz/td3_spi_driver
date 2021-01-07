# CROSS COMPILE FLAGS
ARCH := arm
CROSS_COMPILE := arm-linux-gnueabihf-
CROSS_GCC := arm-linux-gnueabihf-gcc


# C COMPILE FLAGS
GCC := gcc
PROJECT_CFLAGS := -std=c99
PROJECT_CFLAGS += -g
PROJECT_CFLAGS += -lpthread
PROJECT_CFLAGS += -Wpedantic
PROJECT_CFLAGS += -Wall
PROJECT_CFLAGS += -Wstrict-overflow 
PROJECT_CFLAGS += -fno-strict-aliasing 
PROJECT_CFLAGS += -Wshadow
PROJECT_CFLAGS += -Wno-unused-variable
PROJECT_CFLAGS += -Wno-unused-but-set-variable


# VALGRIND FLAGS
VFLAGS  = --quiet
VFLAGS += --tool=memcheck
VFLAGS += --leak-check=full
VFLAGS += --error-exitcode=1


# EXTRA
LIBS := -lpthread
RMALL := rm -fR

PROJECT = webserver
CLANG_FORMAT := clang-format-6.0


# GOALS
#.DEFAULT_GOAL := help
.PHONY: help clean all driver format valgrind debug ps commit 


#SOURCES
vpath %.c ./src/
vpath %.h ./inc/

obj-m += ./src/driver/spi_driver.o

KERNEL_HOST := /usr/src/linux-headers-$(shell uname -r)/
KERNEL_BBB := /home/wozniak/Documents/TD3/imagen_bbb/bb-kernel/KERNEL

SRC := ./src/$(PROJECT)/
BIN := ./bin/

SRCS := $(SRC)bmp_280.c $(SRC)plot_handler.c $(SRC)functions.c $(SRC)driver_handler.c $(SRC)webserver.c
OBJS := $(subst .c,.o,$(SRCS))


# RULES

# Launch webserver with sudo permissons
run: clean $(PROJECT)
	@sudo ./bin/webserver


all: $(PROJECT) driver

# Make driver with HOST kernel
driver_host:
	@$(MAKE) -C $(KERNEL_HOST) M=$(PWD) modules


# Make driver with TARGET kernel
driver:
	@$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNEL_BBB) M=$(PWD) modules


# Send module to TARGET
copy: driver $(PROJECT)
	arm-linux-gnueabihf-gcc ./sup/test.c -o test -g
	sshpass -p "temppwd" scp src/driver/spi_driver.ko ubuntu@192.168.6.2:/home/ubuntu/
	sshpass -p "temppwd" scp test ubuntu@192.168.6.2:/home/ubuntu/
	sshpass -p "temppwd" scp bin/webserver ubuntu@192.168.6.2:/home/ubuntu/
	sshpass -p "temppwd" scp sup/file.cfg ubuntu@192.168.6.2:/home/ubuntu/sup/


# General Rule
%.o: %.c
	@$(GCC) -c $(PROJECT_CFLAGS) $< -o $@

# Build Project
$(PROJECT): $(SRCS)
	@$(CROSS_GCC) -o $(BIN)$@ $^ -lpthread 

# Show all processes
ps:
	ps -elf | grep --color=auto $(PROJECT)

# Kill all processes
kill:
	pkill $(PROJECT)

# Launch cgdb on the webserver
debug: $(PROJECT)
	cgdb $(BIN)$(PROJECT)

# Launch CLI client with GET method
get:
	curl -i -X GET localhost

# Launch CLI client with POST method
post:
	curl -i -X POST localhost

# Send SIGUSR1
sigusr:
	sudo kill -SIGUSR1 `pgrep webserver | awk 'NR==1'`

# Launch valgrind
valgrind: $(PROJECT)
	@valgrind $(VFLAGS) $(BIN)*
	@echo "memory check passed"

# Format all the files
format:
	$(CLANG_FORMAT) -i -style=file ./src/driver/*.c ./src/$(PROJECT)/*.c ./inc/*.h

# Commit all the files
commit: all clean format
	git add --all
	git commit -m "$m"
	git push

# Clean
clean:
	-@$(RMALL) $(BIN)* $(SRC)*.o *.o *.txt *.ko *.mod .*.cmd
	-@make -C $(KERNEL_HOST) M=$(PWD) clean
	-@make -C $(KERNEL_BBB) M=$(PWD) clean

# Help function
help:
	$(info Usage: make [OPTION] )
	$(info )
	$(info Available Options: )
	$(info  * all:        Generate cross compile binary )
	$(info  * $(PROJECT):  Generate $(PROJECT) binary )
	$(info  * driver:     Generate driver binary)
	$(info  * copy:       Send files to target )
	$(info  * post:       Generate POST request to server )
	$(info  * get:        Generate GET request to server )
	$(info  * format:     Format files )
	$(info  * valgrind:   Memory check )
	$(info  * debug:      Launch cgdb on project )
	$(info  * commit:     Add files and commit to repository )
	$(info  * clean:      Remove compile files )
	$(info  * help:       Display this messages )
	@echo Executing make without parameters is equivalent to executing 'make help'
	$(info )

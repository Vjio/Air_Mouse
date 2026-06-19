#include <iostream>
#include <string>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <linux/uinput.h>
#include <cstring>
#include <cstdlib>

#define PORT "/dev/ttyACM0"

void emit(int fd, int type, int code, int val) {
	struct input_event ie{};
	ie.type = type;
	ie.code = code;
	ie.value = val;
	write(fd, &ie, sizeof(ie));
}

int setup_uinput() {
	// open file
	int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if (fd < 0) {
		perror("open /dev/uinput"); 
		exit(1); 
	}

	ioctl(fd, UI_SET_EVBIT,  EV_REL);
	ioctl(fd, UI_SET_RELBIT, REL_X);
	ioctl(fd, UI_SET_RELBIT, REL_Y);

	// make virtual device
	struct uinput_setup usetup{};
	usetup.id.bustype = BUS_USB;
	usetup.id.vendor  = 0x1234;
	usetup.id.product = 0x5678;
	strcpy(usetup.name, "MPU6500 Mouse");

	// register it
	ioctl(fd, UI_DEV_SETUP, &usetup);
	ioctl(fd, UI_DEV_CREATE);
	sleep(1); // let the kernel register the device
	return fd;
}

int setup_serial(const char* port) {
	int fd = open(port, O_RDONLY | O_NOCTTY);
	if (fd < 0) {
		perror("open serial"); 
		exit(1);
	}

	struct termios tty{};
	tcgetattr(fd, &tty);
	cfsetspeed(&tty, B9600);
	tty.c_cflag = CS8 | CREAD | CLOCAL;
	tty.c_iflag = 0;
	tty.c_oflag = 0;
	tty.c_lflag = 0;
	tcsetattr(fd, TCSANOW, &tty);
	return fd;
}

int main(int argc, char* argv[]) {
	// let user input their own port
	const char* port;
	if (argc > 1)
		port = argc[1];
	else {
		port = PORT;
		std::cout << "Using default ttyACM0 port for microcontroller!\n";
	}

	int ufd = setup_uinput();
	int sfd = setup_serial(port);

	std::cout << "mouse running\n";

	std::string line;
	char c;
	while (true) {
		if (read(sfd, &c, 1) <= 0) 
			continue;
		if (c == '\n') {
			int x = 0, y = 0;
			if (sscanf(line.c_str(), "%d,%d", &x, &y) == 2) {
				if (x != 0 || y != 0) {
					std::cout << "data:" << x << " " << y << "\n";
					emit(ufd, EV_REL, REL_X, x);
					emit(ufd, EV_REL, REL_Y, y);
					emit(ufd, EV_SYN, SYN_REPORT, 0);
				}
			}
			line.clear();
		} else if (c != '\r') {
			line += c;
		}
	}

	ioctl(ufd, UI_DEV_DESTROY);
	close(ufd);
	close(sfd);
}
// g++ -o mpumouse mpumouse.cpp

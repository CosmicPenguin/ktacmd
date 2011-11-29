/*
 * Copyright (C) 2011 Jordan Crouse <jordan@cosmicpenguin.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

/* Send a command to the controller */
static int sendcmd(int fd, const char *cmd, int val)
{
	char buffer[20];
	fd_set rfd;
	struct timeval tv;
	int index = 0;
	int size, ret;

	FD_ZERO(&rfd);
	FD_SET(fd, &rfd);

	memset(buffer, 0, sizeof(buffer));

	/* Create the command string */
	/* FIXME: Allow specific adressing */
	size = snprintf(buffer, sizeof(buffer) - 1, "@00 %s %d\r\n", cmd, val);

	ret = write(fd, buffer, size);
	if (ret != size) {
		perror("E: write()");
		return -1;
	}

	/* Get the response */
	memset(buffer, 0, sizeof(buffer));

	/* Wait for up to half a second for a response */
	tv.tv_sec = 5;
	tv.tv_usec = 0;

	ret = select(fd + 1, &rfd, NULL, NULL, &tv);
	if (ret < 0) {
		perror("E: select()");
		return -1;
	}

	/* Timeout means that no response was given */
	if (ret == 0) {
		fprintf(stderr, "E: No response from controller\n");
		return -1;
	}

	/* Read it until we get a \n */
	/* FIXME: Read until an actual CRLF? */

	while(index < sizeof(buffer)) {
		ret = read(fd, &buffer[index], 1);
		if (ret <= 0) {
			perror("E: read()");
			return -1;
		}

		if (buffer[index] == '\n')
			break;
		
		index++;
	}

	/* Zap the last two chars */
	buffer[index--] = 0;
	buffer[index--] = 0;

	ret = 0;

	/* If the buffer len is more than 3 then a response was given */

	if (strlen(buffer) > 3) {
		if (!strncmp(buffer, "#00 ", 4))
			ret = atoi(buffer + 4);
		else
			ret = -1;
	}
	
	return ret;	
}		

/* 'on' command handler */
int on_handler(int fd, int argc, char **argv)
{
	int relay;

	if (argc < 1) {
		fprintf(stderr, "E: on: must specify a relay number [1-8]\n");
		return -1;
	}

	relay = atoi(argv[0]);
	if (relay <= 0 || relay > 8) {
		fprintf(stderr, "E: on: invalid relay %d\n", relay);
		return -1;
	}

	return sendcmd(fd, "ON", relay);
}

/* 'off' command handler */
int off_handler(int fd, int argc, char **argv)
{
	int relay = 0;

	if (argc > 0 && strncmp(argv[0], "all", 3))
		relay = atoi(argv[0]);

	if (relay < 0 || relay > 8) {
		fprintf(stderr, "E: off: invalid relay %d\n", relay);
		return -1;
	}

	return sendcmd(fd, "OF", relay);
}

/* 'status' command handler */
int status_handler(int fd, int argc, char **argv)
{
	int relay = 0;
	int ret, i;

	if (argc > 0 && strncmp(argv[0], "all", 3))
		relay = atoi(argv[0]);

	if (relay < 0 || relay > 8) {
		fprintf(stderr, "E: status: invalid relay %d\n", relay);
		return -1;
	}

	ret = sendcmd(fd, "RS", relay);

	if (ret < 0)
		return -1;

	if (relay > 0) 
		printf(" [%d] %s\n", relay, ret ? "ON" : "OFF");
	else {
		for(i = 0; i < 8; i++) 
			printf(" [%d] %s\n", i + 1,
				ret & (1 << i) ? "ON" : "OFF");
	}

	return 0;
}

/* List of available command and handlers */

static struct {
	const char *cmd;
	int (*handler)(int, int, char **);
} commands[] = {
	{ "status", status_handler },
	{ "on", on_handler },
	{ "off", off_handler },
	{ "", NULL },
};
 	
int main(int argc, char **argv)
{
	int devfd, ret, i;
	struct termios term;

	if (argc < 2) {
		fprintf(stderr, "E: You must specify a command\n");
		return -1;
	}

	/* Check for the device */
	/* FIXME:  Specify serial device on cmdline */
	/* FIXME:  Specify serial device in config file */

	devfd = open("/dev/ttyUSB0", O_RDWR);
	if (devfd < 0) {
		perror("E: open()");
		return -1;
	}

	/* Get the terminal settings */
	ret = tcgetattr(devfd, &term);
	if (ret) {
		perror("E: tcgetattr()");
		goto exit;
	}

	/* Set the params: 9600 8N1 */
	cfsetospeed(&term, B9600);

	ret = tcsetattr(devfd, TCSANOW, &term);
	if (ret) {
		perror("E: tcsetattr()");
		goto exit;
	}

	/* Process the commands */
	for(i = 0; commands[i].handler != NULL; i++) {
		if (!strncmp(argv[1], commands[i].cmd, 
			strlen(commands[i].cmd))) {
			ret = commands[i].handler(devfd, argc - 2, &argv[2]);
			break;
		}
	}

	if (commands[i].handler == NULL) {
		fprintf(stderr, "E: Invalid command %s\n", argv[1]);
		return - 1;
	}

exit:
	close(devfd);
	return ret;
}	

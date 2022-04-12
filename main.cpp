/*
 * Set camera See3Cam_CU20 to HW trigger.
 * https://github.com/econsysqtcam/qtcam/blob/master/src/see3cam_cu20.cpp
 * Author: Martin Sakin
 * Date: 2022-03-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/hidraw.h>
#include <libudev.h>

#include <errno.h>
#include <linux/videodev2.h>

#include <iostream>

#define SUCCESS				0
#define FAILURE 			-1
#define TRUE				1
#define FALSE				0

#define CAMERA_CONTROL_CU20		 0x86
#define GET_CAMERA_MODE_CU20		0x03
#define SET_CAMERA_MODE_CU20		0x04

#define SET_FAIL			0x00
#define SET_SUCCESS			0x01
#define GET_FAIL			0x00
#define GET_SUCCESS			0x01
#define BUFFER_LENGTH		65

#define MAX_NUM_OF_CAMERAS 20
#define MAX_LEN_SERIAL_NUM 16


const char* ECON_VID = "2560";
const char* CAMERA_PID = "c120";	// PID of SEE3CAM_CU20

char *hid_device;
unsigned char g_out_packet_buf[BUFFER_LENGTH];
unsigned char g_in_packet_buf[BUFFER_LENGTH];
unsigned int econdev_count = 0;
unsigned int dev_node[10];
char	dev_name[64];

// Config file
int option = 1;
int stream_mode = 0;
int take_frame = 0;
int stillformatId = 1;
int stillresolutionId = 3;
int cameraMode = 0;
// One camera device is shown twice, so by serial number eliminate duplicity
char serial_nums[MAX_NUM_OF_CAMERAS][MAX_LEN_SERIAL_NUM];


using namespace std;


void loadConfig(char * filename) {
	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	FILE *fp = fopen(filename, "r");

	if (fp == NULL) {
		cout << "Error! Could not open file: " << filename << endl;
		exit(2);
	}

	while ((read = getline(&line, &len, fp)) != -1) {
		if (line[0] == ';' || read <= 1) { // ignore comment
			continue;
		}
		char *token = strtok(line, "=");
		char *value = strtok(NULL, "=");

		/*if (strcmp(token, "input") == 0) {
			strncpy(input, value, strlen(value)-1);
		}*/
		if (strcmp(token, "option") == 0) {
			option = strtol(value, NULL, 10);
		}
		else if (strcmp(token, "stream_mode") == 0) {
			stream_mode = strtol(value, NULL, 10);
		}
		else if (strcmp(token, "stillformatId") == 0) {
			stillformatId = strtol(value, NULL, 10);
		}
		else if (strcmp(token, "stillresolutionId") == 0) {
			stillresolutionId = strtol(value, NULL, 10);
		}
	}
	fclose(fp);
	cout << "Config file loaded: " << filename << endl << flush;
}


void help(){
	printf("HELP:\n"
		" --help\t\t\t print this help\n"
		" --config filename.ini\t load setting from config file\n"
		" on\t\t\t turn on wh trigger\n"
		" off\t\t\t turn off wh trigger\n"
		" get\t\t\t get info about hw trigger\n"
	);
	exit(0);
}


void parseAgrs(int argc, char *argv[]) {
	if (strcmp(argv[1], "--config") == 0 || strcmp(argv[1], "-c") == 0) {
		loadConfig(argv[2]);
	}
	else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
		help();
	}
	else if (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "ON") == 0) {
		cameraMode = 2;
	}
	else if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "OFF") == 0) {
		cameraMode = 1;
	}
	else if (strcmp(argv[1], "get") == 0 || strcmp(argv[1], "GET") == 0) {
		cameraMode = 0;
	}
	else {
		help();
	}
}
//==============================================================================

void close_hid(int hid_fd) {
	if(hid_fd) {
		close(hid_fd);
	}
}


int is_in_list(const char *serial_num) {
	for (unsigned i = 0; i < econdev_count; i ++) {
		if (strcmp(serial_nums[i], serial_num) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}


/* brief description on initExtensionUnit -Enumerating devices and getting hidnode 
 * param parameter - string parameter which contains hid type
 * return success/failure
 */
int initExtensionUnit(char * parameter) {
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;
	struct udev_device *dev,*pdev;
	int	hid_fd;

	// Create the udev object
	udev = udev_new();
	if (!udev) {
		return FAILURE;
	}

	// Create a list of the devices in the 'video4linux/hidraw' subsystem.
	enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, parameter);
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);

	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *path;
		path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev, path);
		pdev = udev_device_get_parent_with_subsystem_devtype(
									dev,
									"usb",
									"usb_device");
		if (!pdev) {
			continue;
		}

		if (strcmp(udev_device_get_sysattr_value(pdev,"idVendor"), ECON_VID) == 0 &&
			strcmp(udev_device_get_sysattr_value(pdev,"idProduct"), CAMERA_PID) == 0)
		{
			/*printf("hid_device = %s\n", udev_device_get_devnode(dev));				   // /dev/hidraw3
			printf("productName = %s\n", udev_device_get_sysattr_value(pdev,"product")); // FSCAM_CU135
			printf("serialNumber = %s\n", udev_device_get_sysattr_value(pdev,"serial")); // 28280B0E
			printf("vidValue = %s\n", udev_device_get_sysattr_value(pdev,"idVendor"));   // 2560
			printf("pidValue = %s\n", udev_device_get_sysattr_value(pdev,"idProduct"));  // c1d4
			*/
			const char *hid_dev = udev_device_get_devnode(dev);
			hid_fd = open(hid_dev, O_RDWR|O_NONBLOCK);

			if(hid_fd < 0) {
				cout << "Unable to open device, try sudo" << endl;
				return FAILURE;
			}

			const char *serial_num = udev_device_get_sysattr_value(pdev,"serial");
			if(!is_in_list(serial_num)) {
				dev_node[econdev_count] = hid_fd;
				strncpy(serial_nums[econdev_count], serial_num, MAX_LEN_SERIAL_NUM);
				econdev_count++;
			}
		}
		udev_device_unref(dev);
	}
	// Free the enumerator object
	udev_enumerate_unref(enumerate);
	udev_unref(udev);
	return SUCCESS;
}


/**
 * brief sendHidCmd - Sending hid command and get reply back
 * param outBuf - Buffer that fills to send into camera
 * param inBuf  - Buffer to get reply back
 * param len	- Buffer length
 * return success/failure
 * */
int sendHidCmd(unsigned char *outBuf, unsigned char *inBuf, int len, int hid_fd) {
	// Write data into camera
	cout << "sendHidCmd() write" << endl;
	int ret = write(hid_fd, outBuf, len);
	if (ret < 0) {
		perror("write");
		return FAILURE;
	}
	cout << "sendHidCmd() FD" << endl;
	struct timeval tv;
	fd_set rfds;

	FD_ZERO(&rfds);
	FD_SET(hid_fd, &rfds);

	cout << "sendHidCmd() tv" << endl;
	// Wait up to 5 seconds.
	tv.tv_sec = 3;
	tv.tv_usec = 0;

	cout << "sendHidCmd() select" << endl;
	// Monitor read file descriptor for 5 secs
	if(0 > select(1, &rfds, NULL, NULL, &tv)){
		perror("select");
		return FAILURE;
	}

	cout << "sendHidCmd() read" << endl;
	// Read data from camera
	int retval = read(hid_fd, inBuf, len);
	if (retval < 0) {
		cout << "sendHidCmd() read FAILURE" << endl;
		perror("read");
		return FAILURE;
	}
	else{
		cout << "sendHidCmd() read SUCCESS" << endl;
		return SUCCESS;
	}
}

/* The same like sendHidCmd() but without check, just write command and leave */
int sendHidCmdFast(unsigned char *outBuf, unsigned char *inBuf, int len, int hid_fd) {
	// Write data into camera
	int ret = write(hid_fd, outBuf, len);
	if (ret < 0) {
		perror("write");
		return FAILURE;
	}
	return SUCCESS;
}


/* Initialize input and output buffers */
void initializeBuffers() {
	memset(g_out_packet_buf, 0x00, sizeof(g_out_packet_buf));
	memset(g_in_packet_buf, 0x00, sizeof(g_in_packet_buf));
}



/**
 * @brief See3CAM_CU20::setCameraMode - getting camera mode
 * @return true/false
 */
bool getCameraMode(int hid_fd) {
	if(hid_fd < 0) {
		return false;
	}
	initializeBuffers();

	// fill buffer values
	g_out_packet_buf[1] = CAMERA_CONTROL_CU20; /* camera id */
	g_out_packet_buf[2] = GET_CAMERA_MODE_CU20; /* get camera command  */

	// send request and get reply from camera
	if(sendHidCmd(g_out_packet_buf, g_in_packet_buf, BUFFER_LENGTH, hid_fd)){
		if (g_in_packet_buf[6]==GET_FAIL) {
			return false;
		} else if(g_in_packet_buf[0] == CAMERA_CONTROL_CU20 &&
			g_in_packet_buf[1]==GET_CAMERA_MODE_CU20 &&
			g_in_packet_buf[6]==GET_SUCCESS) {\
			return true;
		}
	}
	return false;
}



/**
 * @brief See3CAM_CU20::setCameraMode - setting camera mode
 * @param cameraMode - master/slave
 * @return true/false
 */
bool setCameraMode(uint cameraMode, int hid_fd) {
	if(hid_fd < 0) {
		return false;
	}
	initializeBuffers();

	// fill buffer values
	g_out_packet_buf[1] = CAMERA_CONTROL_CU20; /* camera id */
	g_out_packet_buf[2] = SET_CAMERA_MODE_CU20; /* set camera mode command  */
	g_out_packet_buf[3] = cameraMode; /* pass camera mode value */

	// send request and get reply from camera
	if(sendHidCmd(g_out_packet_buf, g_in_packet_buf, BUFFER_LENGTH, hid_fd)){
		if (g_in_packet_buf[6]==SET_FAIL) {
			return false;
		} else if(g_in_packet_buf[0] == CAMERA_CONTROL_CU20 &&
			g_in_packet_buf[1] == SET_CAMERA_MODE_CU20 &&
			g_in_packet_buf[6] == SET_SUCCESS) {
				return true;
			}
	}
	return false;
}




//==============================================================================
//==============================================================================
//==============================================================================
int main(int argc, char *argv[]) {
	int result = 0;

	if (argc >= 2) {
		parseAgrs(argc, argv);
	} else {
		cout << "Missing arguments!!" << endl;
		help();
		exit(1);
	}

	// Get list of cameras (dev_node[n]) and init them
	char unit[] = "hidraw";
	if (initExtensionUnit(unit) < 0) {
		cout << "Device not found!" << endl;
		exit(4);
	}

	if (econdev_count == 0) {
		cout << "No cameras" << endl << flush;
		exit(6);
	} else {
		cout << "Number of cameras: " << econdev_count << endl << flush;
	}

	// Set all cameras to HW trigger
	for (unsigned i = 0; i < econdev_count; i++) {
		cout << "=== Processing dev_node " << dev_node[i] << " | /dev/video" << dev_node[i]+1 << endl;
		if(cameraMode == 0) {
			cout << "getCameraMode()" << endl;
			result = getCameraMode(i);
			cout << "result = " << result << endl;
		} else {
			cout << "setCameraMode(" << cameraMode << ")" << endl;
			result = setCameraMode(cameraMode, i);
			cout << "result = " << result << endl;
		}
	}
	sleep(2);

	// Close all cameras
	for (unsigned i = 0; i < econdev_count; i++) {
		close_hid(dev_node[i]);
	}
	return 0;
}

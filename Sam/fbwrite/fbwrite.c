/* Author: Sam Solondz
*  C file to copy an image to the framebuffer
*  2.2" pi tft screen resolution
*  W = 320, H = 240
*  6-bit color
* Code referenced from 
* https://stackoverflow.com/questions/2693631/read-ppm-file-and-store-it-in-an-array-coded-with-c 
* and
* http://betteros.org/tut/graphics1.php 
 */
#include <linux/fb.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>


struct PPMpixel{
	uint8_t red;
	uint8_t green;
	uint8_t blue;
};

struct PPMimage{
	int x, y;
};


#define RES 76800	
struct PPMpixel pixArr[RES];

#define RGB_COMPONENT 255

/*Read in a PPM file. Currently, only 320x240 resolutions are supported.
*
*@param fp file pointer passed from main().
*/
void readPPM(char * filename)
{
	int err = 0;
	struct PPMimage * img = malloc(sizeof(struct PPMimage));

	FILE *fp = fopen(filename, "r");
	if(!fp)
	{
		err = errno;
		perror("Could not open PPM file");
		syslog(LOG_ERR, "Could not open file %s, errno: %s", filename, strerror(err));
		exit(1);
	}

	char buf[16];
	/*fgets teminates at newline*/
	if(!fgets(buf, sizeof(buf), fp)) //P6
	{
		err = errno;
		perror("Could not open PPM file");
		syslog(LOG_ERR, "Could not fget from file %s, errno: %s", filename, strerror(err));
		exit(1);
	}
	//syslog(LOG_INFO, "buf = %s", buf);

	/*Scan through comments*/
	char c = getc(fp);
	while(c == '#')
	{
		while(getc(fp) != '\n');
		c = getc(fp);
	}
	ungetc(c,fp);


    	if (fscanf(fp, "%d %d", &img->x, &img->y) != 2) 
	{
		syslog(LOG_ERR, "Invalid image size (error loading '%s')\n", filename);
         	exit(1);
    	}
	syslog(LOG_INFO, "x: %d y: %d",  img->x, img->y);
	
	if(img->x != 320 || img->y != 240)
	{
		syslog(LOG_ERR, "Image must be 320x240");
		exit(1);
	}

    	//read rgb component
	int rgb_comp;
    	if (fscanf(fp, "%d", &rgb_comp) != 1) {
         	syslog(LOG_ERR, "Invalid rgb component (error loading '%s')\n", filename);
         	exit(1);
    	}
	syslog(LOG_INFO, "rgb_val: %d",  rgb_comp);
    
	
  	
	if (rgb_comp!= RGB_COMPONENT) {
         syslog(LOG_ERR, "'%s' does not have 8-bits components\n", filename);
         exit(1);
    
	}

 	
	
 	while (fgetc(fp) != '\n'); 
    	
    	/*Read pixel data from file*/   	
	for(int i=0; i<RES; i++)
	{
		fread(&pixArr[i],3*sizeof(uint8_t),1,fp);
	}

    	fclose(fp);
	free(img);

    	syslog(LOG_INFO, "Done reading image\n");
    
}

/*Convert 8-bit color values to 6-bit and package in a 32-bit value.
*
*@param pixel pixel struct containing rgb data
*@param vinfo pointer to fb_var_screeninfo struct to obtain pixel offsets
*/
uint32_t convert_8_to_6(struct PPMpixel pixel, struct fb_var_screeninfo *vinfo)
{
	uint32_t pixelTotal = ((pixel.red>>2) << vinfo->red.offset) | ((pixel.blue >> 2) << vinfo->blue.offset) | ((pixel.green >> 2) << vinfo->green.offset);

	return pixelTotal;
}

int main(int argc, char * argv[])
{
	/*Setup logging*/
	openlog(NULL, LOG_PID, LOG_USER);
	syslog(LOG_INFO, "-----Beginning fbwrite-----\n");
	
	char * filename = argv[1];
	//usage: ./fb_test [file]
	readPPM(filename);

	/*Open a file descriptor to the frame buffer*/
	int fb_fd = open("/dev/fb1", O_RDWR);

	struct fb_fix_screeninfo finfo;
	struct fb_var_screeninfo vinfo;
	
	/*Get variable screen info*/
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
	vinfo.grayscale=0;
	vinfo.bits_per_pixel=24;

	ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);

	/*Get fixed screen info*/
	ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);

	/*Calculate total size of the screen in bytes*/
	/*horizontal lines on screen x length of each line in bytes*/
	long int screensize =vinfo.xres * vinfo.yres * vinfo.bits_per_pixel/8;

//	syslog(LOG_INFO, "vinfo.yres_virtual %d\n",vinfo.yres_virtual); 
//	syslog(LOG_INFO, "finfo.linelength %d\n",finfo.line_length); 
//	syslog(LOG_INFO, "screensize = %ld\n", screensize);
//	syslog(LOG_INFO, "vinfo.xres = %d\n", vinfo.xres);
//	syslog(LOG_INFO, "vinfo.yres = %d\n", vinfo.yres);
//	syslog(LOG_INFO, "vinfo.yoffset = %d\n", vinfo.yoffset);
//	syslog(LOG_INFO, "vinfo.xoffset = %d\n", vinfo.xoffset);
//	syslog(LOG_INFO, "vinfo.red.offset: %d, green: %d, blue: %d, transp: %d", vinfo.red.offset, vinfo.green.offset, vinfo.blue.offset, vinfo.transp.offset);
	
	char *fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, (off_t)0);
	int x = 0;
	int y = 0;
	
	for (x = 0; x < vinfo.xres; x++)
	{
		for (y = 0; y < vinfo.yres; y++)
		{

			long int location = (x+vinfo.xoffset) * (vinfo.bits_per_pixel/8) + (y+vinfo.yoffset) * finfo.line_length;
			int pixel = x + y*vinfo.xres;
			*((uint32_t*)(fbp + location)) = convert_8_to_6(pixArr[pixel], &vinfo);
		}
	}


	close(fb_fd);
	
	return 0;
}

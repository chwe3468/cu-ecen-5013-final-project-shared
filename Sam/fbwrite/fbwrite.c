/* C file to copy an image to the framebuffer
*  2.2" pi tft screen resolution
*  W = 320, H = 240
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
struct PPMpixel{
	uint8_t red;
	uint8_t green;
	uint8_t blue;
};

struct PPMimage{
	int x, y;
 	struct PPMpixel * data;
};

#define RGB_COMPONENT_COLOR 255
struct PPMimage * readPPM(char * filename)
{
	int err = 0;
	
	FILE *fp = fopen(filename, "r");
	if(!fp)
	{
		err = errno;
		perror("Could not open PPM file");
		syslog(LOG_ERR, "Could not open file %s, errno: %s", filename, strerror(err));
		exit(1);
	}

	char buf[2]; //16;
	if(!fgets(buf, sizeof(buf), fp))
	{
		err = errno;
		perror("Could not open PPM file");
		syslog(LOG_ERR, "Could not fget from file %s, errno: %s", filename, strerror(err));
		exit(1);
	}

	if(buf[0] != 'P' || buf[1] != '6')
	{
		syslog(LOG_ERR, "Image format must be P6");
		exit(1);
	}


	struct PPMimage * img = (struct PPMimage *)malloc(sizeof(struct PPMimage));
	if(!img)
	{
		err = errno;
		syslog(LOG_ERR, "Unable to allocate memory for PPMimage, errno: %s", strerror(err));
		exit(1);
	}

	//check for comments
    	int c = getc(fp);
    	while (c == '#') 
	{
    		while (getc(fp) != '\n') ;
         	c = getc(fp);
    	}
    	ungetc(c, fp);
    
	//read image size information
    	if (fscanf(fp, "%d %d", &img->x, &img->y) != 2) 
	{
		syslog(LOG_ERR, "Invalid image size (error loading '%s')\n", filename);
         	exit(1);
    	}

    	//read rgb component
	int rgb_comp_color;
    	if (fscanf(fp, "%d", &rgb_comp_color) != 1) {
         	syslog(LOG_ERR, "Invalid rgb component (error loading '%s')\n", filename);
         	exit(1);
    	}

    //check rgb component depth
    if (rgb_comp_color!= RGB_COMPONENT_COLOR) {
         syslog(LOG_ERR, "'%s' does not have 8-bits components\n", filename);
         exit(1);
    }

    while (fgetc(fp) != '\n');
    //memory allocation for pixel data
    img->data = (struct PPMpixel *)malloc(img->x * img->y * sizeof(struct PPMpixel));

    if (!img->data) {
         syslog(LOG_ERR, "Unable to allocate memory\n");
         exit(1);
    }

    //read pixel data from file
    if (fread(img->data, 3 * img->x, img->y, fp) != img->y) {
         syslog(LOG_ERR, "Error loading image '%s'\n", filename);
         exit(1);
    }

    fclose(fp);

    printf("Done reading image\n");
    return img;


}

uint32_t pixel_color(uint8_t r, uint8_t g, uint8_t b, struct fb_var_screeninfo *vinfo)
{
	return (r<<vinfo->red.offset) | (g<<vinfo->green.offset) | (b<<vinfo->blue.offset);
}

int main(int argc, char * argv[])
{

	/*Setup logging*/
	openlog(NULL, LOG_PID, LOG_USER);
	syslog(LOG_INFO, "-----Beginning fb_test-----\n");
	
	char * filename = argv[1];
	//usage: ./fb_test [file]
	struct PPMimage * image = readPPM(filename);

	/*Open a file descriptor to the frame buffer*/
	int fb_fd = open("/dev/fb1", O_RDWR);

	struct fb_fix_screeninfo finfo;
	struct fb_var_screeninfo vinfo;
	
	/*Get variable screen info*/
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
	vinfo.grayscale=0;
	vinfo.bits_per_pixel=32;
	ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);

	/*Get fixed screen info*/
	ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);

	/*Calculate total size of the screen in bytes*/
	/*horizontal lines on screen x length of each line in bytes*/
	long screensize = vinfo.yres_virtual * finfo.line_length;

	printf("vinfo.yres_virtual %d\n",vinfo.yres_virtual); 
	printf("screensize = %ld\n", screensize);

	uint8_t *fbp = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, (off_t)0);

	int x,y;

	printf("vinfo.xres = %d\n", vinfo.xres);
	printf("vinfo.yres = %d\n", vinfo.yres);

	for (x=0;x<vinfo.xres/2;x++)
	{
		for (y=0;y<vinfo.yres/2;y++)
		{

			/*x + y*/
			long location = (x+vinfo.xoffset) * (vinfo.bits_per_pixel/8) + (y+vinfo.yoffset) * finfo.line_length;
			*((uint32_t*)(fbp + location)) = pixel_color(image->data[location].red,image->data[location].green,image->data[location].blue, &vinfo);
			//*((uint32_t*)(fbp + location)) = pixel_color((uint8_t)x,0x00,0xFF, &vinfo);
		}
	}

	free(image->data);
	free(image);

	return 0;
}

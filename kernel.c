#include "multiboot.h"
#include "memory.h"
#include "console.h"
#include "bootpack.h"

#include <math.h>

struct CONSOLE *cons;

int w,h,p;
unsigned int *fb;

#define ABSI(n) ((n) > 0 ? (n) : -(n))

void arrayline(int *ay, int x1, int y1, int x2, int y2)
{
	if(y1 == y2) {
		ay[y1] = (x1 + x2) / 2;
		return;
	}
	
	int steep = ABSI(y2-y1) > ABSI(x2-x1);
	
	if(steep) {
		int tmp;
		tmp=x1;
		x1=y1;
		y1=tmp;
		
		tmp=x2;
		x2=y2;
		y2=tmp;
	}
	
	if(x1>x2) {
		int tmp;
		tmp=x1;
		x1=x2;
		x2=tmp;
		
		tmp=y1;
		y1=y2;
		y2=tmp;
	}
	
	int dx=ABSI(x2-x1);
	int dy=ABSI(y2-y1);
	
	int err=dx/2;
	int sy;
	
	sy = y1 < y2 ? 1 : -1;
	
	int y = y1;
	
	for(int x=x1;x<=x2;x++) {
		if(steep) {
			if(x >= 0 && x < h) ay[x] = y;
		} else {
			if(y >= 0 && y < h) ay[y] = x;
		}
		
		err -= dy;
		if(err < 0) {
			y+=sy;
			err+=dx;
		}
	}
}

#define INTERP(xi,xi1,yi,yi1,x) (yi + ((( yi1 - yi ) * ( x - xi )) / ( xi1 - xi )))

void pppoly(int *xv, int *yv, int c)
{
	int s[3][1024];
	int m[3][4]; // min x,max x,min y,max y
	
	int miny=32768,maxy=-32767;
	
	for(int i=0;i<3;i++) memset(s[i],255,1024*sizeof(int));
	
	int x[3],y[3];
	int mii,mai;
	
	for(int i=0;i<3;i++) {
		if(yv[i] > maxy) {
			maxy = yv[i];
			mai = i;
		}
		if(yv[i] < miny) {
			miny = yv[i];
			mii = i;
		}
	}
	
	for(int i=0;i<3;i++) {
		if(i == mii) {
			x[0] = xv[i];
			y[0] = yv[i];
		} else if(i == mai) {
			x[2] = xv[i];
			y[2] = yv[i];
		} else {
			x[1] = xv[i];
			y[1] = yv[i];
		}
	}
	
	for(int i=0;i<3;i++) {
		int x1=x[i],y1=y[i],x2=x[(i+1)%3],y2=y[(i+1)%3];
		m[i][0] = x1 < x2 ? x1 : x2;
		m[i][1] = x1 > x2 ? x1 : x2;
		m[i][2] = y1 < y2 ? y1 : y2;
		m[i][3] = y1 > y2 ? y1 : y2;
	}
	
	for(int i=0;i<3;i++) {
		arrayline(s[i],x[i],y[i],x[(i+1)%3],y[(i+1)%3]);
	}
	
	if(miny >= h && maxy < 0) return;
	if(miny < 0) miny = 0;
	if(maxy >= h) maxy = h - 1;
	
	for(int i=miny;i<maxy;i++) {
		int sa,sb;
		
		int tmp;
		
		if(m[0][2] <= i && m[0][3] > i) {
			sa = s[0][i];
		}
		if(m[1][2] <= i && m[1][3] > i) {
			sa = s[1][i];
		}
		if(m[2][2] <= i && m[2][3] > i) {
			sb = s[2][i];
		}
		
		if(sa > sb) {
			tmp=sa;
			sa=sb;
			sb=tmp;
		}
		
		if(sa >= w && sb < 0) continue;
		if(sa < 0) sa = 0;
		if(sb >= w) sb = w - 1;
		
		for(int j=sa;j<sb;j++) {
			fb[i*p+j] = c;
		}
	}
}

void _kernel_entry(UINT32 magic, MULTIBOOT_INFO *info)
{
	struct FIFO32 fifo, keycmd;
	int fifobuf[128], keycmd_buf[32];
	struct TASK *task_a, *task;
	
	init_gdtidt(info);
	init_pic();
	io_sti(); /* IDT/PICの初期化が終わったのでCPUの割り込み禁止を解除 */
	fifo32_init(&fifo, 128, fifobuf, 0);
	*((int *) 0x0fec) = (int) &fifo;
	init_pit();
	init_keyboard(&fifo, 256);
	io_out8(PIC0_IMR, 0xf8); /* PITとPIC1とキーボードを許可(11111000) */
	io_out8(PIC1_IMR, 0xff); /* 割り込み禁止(11111111) */
	fifo32_init(&keycmd, 32, keycmd_buf, 0);
	
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	int memtotal = info->mem_upper;
	memman_init(memman);
	memman_free(memman, 0x00001000, 0x0009e000); /* 0x00001000 - 0x0009efff */
	memman_free(memman, 0x00400000, memtotal - 0x00400000);
	
	task_a = task_init(memman);
	fifo.task = task_a;
	task_run(task_a, 1, 2);
	
	cons = (struct CONSOLE *) memman_alloc_4k(memman, sizeof(struct CONSOLE));
	
	char s[60];
	
	//sprintf(s, "CS:%08x SS:%08x\n", read_cs(), read_ss());
	//cons_putstr0(cons, s);
	
	unsigned int *ffb = (unsigned int *)info->framebuffer_addr_low;
	
	fb = (unsigned int *)memman_alloc_4k(memman, 800*600*4);
	
	memset(fb,0,800*600*4);
	
	w = info->framebuffer_width;
	h = info->framebuffer_height;
	p = info->framebuffer_pitch/4;
	
	int px[] = {120,550,320};
	int py[] = {120,150,550};
	
	int qx[3], qy[3];
	
	memcpy(qx,px,sizeof(px));
	memcpy(qy,py,sizeof(py));
	
	float a = 0;
	
	for(;;) {
		memset(fb,0,800*600*4);
		
		for(int i=0;i<3;i++) {
			qx[i] = (px[i]-400) * cos(a) - (py[i]-300) * sin(a) + 400;
			qy[i] = (px[i]-400) * sin(a) + (py[i]-300) * cos(a) + 300;
		}
		
		int r = rand() & 255;
		int g = rand() & 255;
		int b = rand() & 255;
		
		a += 0.01;
		
		pppoly(qx,qy,(r << 16) | (g << 8) | b);
		
		memcpy(ffb,fb,800*600*4);
	}
}

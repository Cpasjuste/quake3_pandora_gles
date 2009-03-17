#define QUAKE1 0
#define QUAKE2 0
#define QUAKE3 1

#include "pandora_event.h"
#include "pandora_type.h"
 
#if QUAKE1
#include "cvar.h"
#include "keys.h"
#include "client.h"

extern float mouse_x, mouse_y;
#elif QUAKE2
// todo
#elif QUAKE3
#include "../game/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../ui/keycodes.h"

cvar_t* cvarKey;
cvar_t* cvarMouse;
#endif

unsigned char keymap[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
  '9', '0', '-', '=', K_BACKSPACE,	/* Backspace */
  K_TAB,			/* Tab */
  'q', 'w', 'e', 'r',	/* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', K_ENTER,	/* Enter key */
    K_CTRL,			/* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
 '\'', '`',   K_SHIFT,		/* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
  'm', ',', '.', '/',   K_SHIFT,				/* Right shift */
  '*',
    K_ALT,	/* Alt */
  K_SPACE,	/* Space bar */
    0,	/* Caps lock */
    K_F1,	/* 59 - F1 key ... > */
    K_F2,   K_F3,   K_F4,   K_F5,   K_F6,   K_F7,   K_F8,   K_F9,
    K_F10,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    K_HOME,	/* Home key */
    K_UPARROW,	/* Up Arrow */
    K_PGUP,	/* Page Up */
  '-',
    K_LEFTARROW,	/* Left Arrow */
    0,
    K_RIGHTARROW,	/* Right Arrow */
  '+',
    K_END,	/* 79 - End key*/
    K_DOWNARROW,	/* Down Arrow */
    K_PGDN,	/* Page Down */
    K_INS,	/* Insert Key */
    K_DEL,	/* Delete Key */
    0,   0,   0,
    K_F11,	/* F11 Key */
    K_F12,	/* F12 Key */
    0,	/* All other keys are undefined */
};

char event_name[30];
int fd_usbk, fd_usbm, fd_gpio, rd, i, j, k;
struct input_event ev[64];
int version;
unsigned short id[4];
unsigned long bit[EV_MAX][NBITS(KEY_MAX)];
char name[256] = "Unknown";
int absolute[5];

char pnd_gpio[10] = "gpio-keys";

void PND_Setup_Controls( void )
{
	int event_key = 0;
	int event_mouse = 0;

	printf( "Setting up Pandora Controls\n" );
#if QUAKE1
	event_key = 6; // COM_CheckParm("-usbk");
	event_mouse = 8; // COM_CheckParm("-usbm");
#elif QUAKE2
	// Todo
#elif QUAKE3
	cvarKey = Cvar_Get("usbk", "0", 0);
	cvarMouse = Cvar_Get("usbm", "0", 0);

	event_key = cvarKey->value;
	event_mouse = cvarMouse->value;
#endif
	// Pandora buttons
	fd_gpio = PND_OpenEventDeviceByName(pnd_gpio);

	// USB keyboard
	if( event_key > 0 ) {
		fd_usbk = PND_OpenEventDeviceByID(event_key);
	} else {
		printf( "No device selected for USB keyboard\n" );
	}

	// USB mouse
	if( event_mouse > 0 ) {
		fd_usbm = PND_OpenEventDeviceByID(event_mouse);
	} else {
		printf( "No device selected for USB mouse\n" );
	}
}

void PND_Close_Controls( void )
{
	printf( "Closing Pandora Controls\n" );
	if( fd_gpio > 0 )
		close(fd_gpio);
	if( fd_usbk > 0 )
		close(fd_usbk);
	if( fd_usbm > 0 )
		close(fd_usbm);
}

void PND_SendKeys( void )
{
	// Pandora buttons
	if( fd_gpio != 0 )
	{
		rd = read(fd_gpio, ev, sizeof(struct input_event) * 64);
		if (rd > (int) sizeof(struct input_event))
		{
			for (i = 0; i < rd / sizeof(struct input_event); i++)
			{
				PND_CheckEvent( &ev[i] );
			}
		}
	}

  	// USB Keyboard
	if( fd_usbk != 0 )
	{
		rd = read(fd_usbk, ev, sizeof(struct input_event) * 64);

		if (rd > (int) sizeof(struct input_event))
		{
			for (i = 0; i < rd / sizeof(struct input_event); i++)
			{
				PND_CheckEvent( &ev[i] );
			}
		}
	}

	if( fd_usbm != 0 )
	{
		// USB Mouse
		rd = read(fd_usbm, ev, sizeof(struct input_event) * 64);

		if (rd > (int) sizeof(struct input_event))
		{
			for (i = 0; i < rd / sizeof(struct input_event); i++)
			{
				PND_CheckEvent( &ev[i] );
			}
		}
	}
}

void PND_CheckEvent( struct input_event *event )
{
	int sym, value;
	long x, y;

	printf( "Type %d Code %d Value %d\n", event->type, event->code, event->value );

	x	= 0;
	y	= 0;
	sym	= 0;
	value	= event->value;
	switch( event->type )
	{
		case EV_KEY:
			switch( event->code )
			{
				case KEY_UP:
					sym = K_UPARROW;
					break;
				case KEY_DOWN:
					sym = K_DOWNARROW;
					break;
				case KEY_LEFT:
					sym = K_LEFTARROW;
					break;
				case KEY_RIGHT:
					sym = K_RIGHTARROW;
					break;
				case KEY_MENU:
					break;
				case BTN_START:
					break;
				case BTN_SELECT:
					break;
				case BTN_X:
					sym = K_CTRL;
					break;
				case BTN_Y:
					sym = K_SPACE;
					break;
				case BTN_A:
					sym = 'a';
					break;
				case BTN_B:
					sym = 'b';
					break;
				case BTN_TL:
					sym = K_ESCAPE;
					break;
				case BTN_TR:
					sym = K_ENTER;
					break;
				case BTN_LEFT:
					sym = K_MOUSE1;
					break;
				case BTN_RIGHT:
					sym = K_MOUSE2;
					break;
				default:
					if( keymap[event->code]>0 )
						sym = keymap[event->code];
					break;
			}

			if( sym > 0 ) {
#if QUAKE1
				Key_Event(sym, value);
#elif QUAKE2
				// Todo
#elif QUAKE3
				Sys_QueEvent(0, SE_KEY, sym, value, 0, NULL);
#endif  
			}
			break;
		case EV_REL:
			switch( event->code )
			{
				case REL_X:
					x = value;
					break;
				case REL_Y:
					y = value;
					break;
			}

			if( x != 0 || y != 0 )
			{
#if QUAKE1
				if( x != 0 ) mouse_x = x*10;
				if( y != 0 ) mouse_y = y*10;
#elif QUAKE2
				// Todo
#elif QUAKE3
				Sys_QueEvent( 0, SE_MOUSE, x, y, 0, NULL ); 
#endif
			}
			break;
	}
}

int PND_OpenEventDeviceByID( int event_id )
{
	int fd;

	snprintf( event_name, sizeof(event_name), "/dev/input/event%d", event_id );
	printf( "Device: %s\n", event_name );
	if ((fd = open(event_name, O_RDONLY |  O_NDELAY)) < 0) {
		perror("ERROR: Could not open device");
		return 0;
	}

	if (ioctl(fd, EVIOCGVERSION, &version)) {
		perror("evtest: can't get version");
		return 0;
	}

	printf("Input driver version is %d.%d.%d\n",
		version >> 16, (version >> 8) & 0xff, version & 0xff);

	ioctl(fd, EVIOCGID, id);
	printf("Input device ID: bus 0x%x vendor 0x%x product 0x%x version 0x%x\n",
		id[ID_BUS], id[ID_VENDOR], id[ID_PRODUCT], id[ID_VERSION]);

	ioctl(fd, EVIOCGNAME(sizeof(name)), name);
	printf("Input device name: \"%s\"\n", name);

	return fd;
}

int PND_OpenEventDeviceByName( char device_name[] )
{
	int fd;

	for (i = 0; 1; i++)
	{
		snprintf( event_name, sizeof(event_name), "/dev/input/event%d", i );
		printf( "Device: %s\n", event_name );
		if ((fd = open(event_name, O_RDONLY |  O_NDELAY)) < 0) {
			perror("ERROR: Could not open device");
			return 1;
		}
		if (fd < 0) break; /* no more devices */

		ioctl(fd, EVIOCGNAME(sizeof(name)), name);
		if (strcmp(name, device_name) == 0)
		{
			if (ioctl(fd, EVIOCGVERSION, &version)) {
				perror("evtest: can't get version");
				return 1;
			}

			printf("Input driver version is %d.%d.%d\n",
				version >> 16, (version >> 8) & 0xff, version & 0xff);

			ioctl(fd, EVIOCGID, id);
			printf("Input device ID: bus 0x%x vendor 0x%x product 0x%x version 0x%x\n",
				id[ID_BUS], id[ID_VENDOR], id[ID_PRODUCT], id[ID_VERSION]);

			ioctl(fd, EVIOCGNAME(sizeof(name)), name);
			printf("Input device name: \"%s\"\n", name);
		  
			return fd;
		}
		close(fd); /* we don't need this device */
	}
	return 0;
}

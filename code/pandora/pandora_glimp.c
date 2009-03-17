/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "../renderer/tr_local.h"
#include "../ui/keycodes.h"
#include "../client/client.h"

#include "pandora_glimp.h"

#include <stdint.h>
#include <assert.h>

#include <stdio.h>

#ifdef X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/xf86vmode.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#define KEY_MASK (KeyPressMask | KeyReleaseMask)
#define MOUSE_MASK (ButtonPressMask | ButtonReleaseMask | \
		    PointerMotionMask | ButtonMotionMask )
#define X_MASK (KEY_MASK | MOUSE_MASK | VisibilityChangeMask | StructureNotifyMask )

static qboolean mouse_avail;
static qboolean mouse_active = qfalse;
static int mwx, mwy;
static int mx = 0, my = 0;
static int win_x, win_y;

// Time mouse was reset, we ignore the first 50ms of the mouse to allow settling of events
static int mouseResetTime = 0;
#define MOUSE_RESET_DELAY 50

static cvar_t *in_mouse;
cvar_t *in_subframe;
cvar_t *in_nograb; // this is strictly for developers

// bk001130 - from cvs1.17 (mkv), but not static
cvar_t   *in_joystick      = NULL;
cvar_t   *in_joystickDebug = NULL;
cvar_t   *joy_threshold    = NULL;

static int mouse_accel_numerator;
static int mouse_accel_denominator;
static int mouse_threshold;   

Window				x11Window	= 0;
Display*			x11Display	= 0;
long				x11Screen	= 0;
XVisualInfo*			x11Visual	= 0;
Colormap			x11Colormap	= 0;

static XF86VidModeGamma vidmode_InitialGamma;
qboolean vidmode_ext = qfalse;
static int num_vidmodes;
static qboolean vidmode_active = qfalse;
static XF86VidModeModeInfo **vidmodes;
static int vidmode_MajorVersion = 0, vidmode_MinorVersion = 0; // major and minor of XF86VidExtensions

/*
* Find the first occurrence of find in s.
*/
// bk001130 - from cvs1.17 (mkv), const
// bk001130 - made first argument const
static const char *Q_stristr( const char *s, const char *find)
{
  register char c, sc;
  register size_t len;

  if ((c = *find++) != 0)
  {
    if (c >= 'a' && c <= 'z')
    {
      c -= ('a' - 'A');
    }
    len = strlen(find);
    do
    {
      do
      {
        if ((sc = *s++) == 0)
          return NULL;
        if (sc >= 'a' && sc <= 'z')
        {
          sc -= ('a' - 'A');
        }
      } while (sc != c);
    } while (Q_stricmpn(s, find, len) != 0);
    s--;
  }
  return s;
}

/*****************************************************************************
** KEYBOARD
** NOTE TTimo the keyboard handling is done with KeySyms
**   that means relying on the keyboard mapping provided by X
**   in-game it would probably be better to use KeyCode (i.e. hardware key codes)
**   you would still need the KeySyms in some cases, such as for the console and all entry textboxes
**     (cause there's nothing worse than a qwerty mapping on a french keyboard)
**
** you can turn on some debugging and verbose of the keyboard code with #define KBD_DBG
******************************************************************************/

//#define KBD_DBG

static char *XLateKey(XKeyEvent *ev, int *key)
{
  static char buf[64];
  KeySym keysym;
  int XLookupRet;

  *key = 0;

  XLookupRet = XLookupString(ev, buf, sizeof buf, &keysym, 0);
#ifdef KBD_DBG
  ri.Printf(PRINT_ALL, "XLookupString ret: %d buf: %s keysym: %x\n", XLookupRet, buf, keysym);
#endif
  
  switch (keysym)
  {
  case XK_KP_Page_Up: 
  case XK_KP_9:  *key = K_KP_PGUP; break;
  case XK_Page_Up:   *key = K_PGUP; break;

  case XK_KP_Page_Down: 
  case XK_KP_3: *key = K_KP_PGDN; break;
  case XK_Page_Down:   *key = K_PGDN; break;

  case XK_KP_Home: *key = K_KP_HOME; break;
  case XK_KP_7: *key = K_KP_HOME; break;
  case XK_Home:  *key = K_HOME; break;

  case XK_KP_End:
  case XK_KP_1:   *key = K_KP_END; break;
  case XK_End:   *key = K_END; break;

  case XK_KP_Left: *key = K_KP_LEFTARROW; break;
  case XK_KP_4: *key = K_KP_LEFTARROW; break;
  case XK_Left:  *key = K_LEFTARROW; break;

  case XK_KP_Right: *key = K_KP_RIGHTARROW; break;
  case XK_KP_6: *key = K_KP_RIGHTARROW; break;
  case XK_Right:  *key = K_RIGHTARROW;    break;

  case XK_KP_Down:
  case XK_KP_2:    *key = K_KP_DOWNARROW; break;
  case XK_Down:  *key = K_DOWNARROW; break;

  case XK_KP_Up:   
  case XK_KP_8:    *key = K_KP_UPARROW; break;
  case XK_Up:    *key = K_UPARROW;   break;

  case XK_Escape: *key = K_ESCAPE;    break;

  case XK_KP_Enter: *key = K_KP_ENTER;  break;
  case XK_Return: *key = K_ENTER;    break;

  case XK_Tab:    *key = K_TAB;      break;

  case XK_F1:    *key = K_F1;       break;

  case XK_F2:    *key = K_F2;       break;

  case XK_F3:    *key = K_F3;       break;

  case XK_F4:    *key = K_F4;       break;

  case XK_F5:    *key = K_F5;       break;

  case XK_F6:    *key = K_F6;       break;

  case XK_F7:    *key = K_F7;       break;

  case XK_F8:    *key = K_F8;       break;

  case XK_F9:    *key = K_F9;       break;

  case XK_F10:    *key = K_F10;      break;

  case XK_F11:    *key = K_F11;      break;

  case XK_F12:    *key = K_F12;      break;

    // bk001206 - from Ryan's Fakk2 
    //case XK_BackSpace: *key = 8; break; // ctrl-h
  case XK_BackSpace: *key = K_BACKSPACE; break; // ctrl-h

  case XK_KP_Delete:
  case XK_KP_Decimal: *key = K_KP_DEL; break;
  case XK_Delete: *key = K_DEL; break;

  case XK_Pause:  *key = K_PAUSE;    break;

  case XK_Shift_L:
  case XK_Shift_R:  *key = K_SHIFT;   break;

  case XK_Execute: 
  case XK_Control_L: 
  case XK_Control_R:  *key = K_CTRL;  break;

  case XK_Alt_L:  
  case XK_Meta_L: 
  case XK_Alt_R:  
  case XK_Meta_R: *key = K_ALT;     break;

  case XK_KP_Begin: *key = K_KP_5;  break;

  case XK_Insert:   *key = K_INS; break;
  case XK_KP_Insert:
  case XK_KP_0: *key = K_KP_INS; break;

  case XK_KP_Multiply: *key = '*'; break;
  case XK_KP_Add:  *key = K_KP_PLUS; break;
  case XK_KP_Subtract: *key = K_KP_MINUS; break;
  case XK_KP_Divide: *key = K_KP_SLASH; break;

    // bk001130 - from cvs1.17 (mkv)
  case XK_exclam: *key = '1'; break;
  case XK_at: *key = '2'; break;
  case XK_numbersign: *key = '3'; break;
  case XK_dollar: *key = '4'; break;
  case XK_percent: *key = '5'; break;
  case XK_asciicircum: *key = '6'; break;
  case XK_ampersand: *key = '7'; break;
  case XK_asterisk: *key = '8'; break;
  case XK_parenleft: *key = '9'; break;
  case XK_parenright: *key = '0'; break;
  
  // weird french keyboards ..
  // NOTE: console toggle is hardcoded in cl_keys.c, can't be unbound
  //   cleaner would be .. using hardware key codes instead of the key syms
  //   could also add a new K_KP_CONSOLE
  case XK_twosuperior: *key = '~'; break;
		
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=472
	case XK_space:
	case XK_KP_Space: *key = K_SPACE; break;

  default:
    if (XLookupRet == 0)
    {
      if (com_developer->value)
      {
        ri.Printf(PRINT_ALL, "Warning: XLookupString failed on KeySym %d\n", keysym);
      }
      return NULL;
    }
    else
    {
      // XK_* tests failed, but XLookupString got a buffer, so let's try it
      *key = *(unsigned char *)buf;
      if (*key >= 'A' && *key <= 'Z')
        *key = *key - 'A' + 'a';
      // if ctrl is pressed, the keys are not between 'A' and 'Z', for instance ctrl-z == 26 ^Z ^C etc.
      // see https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=19
      else if (*key >= 1 && *key <= 26)
     	  *key = *key + 'a' - 1;
    }
    break;
  } 

  return buf;
}

// ========================================================================
// makes a null cursor
// ========================================================================

static Cursor CreateNullCursor(Display *display, Window root)
{
	Pixmap cursormask; 
	XGCValues xgc;
	GC gc;
	XColor dummycolour;
	Cursor cursor;

  cursormask = XCreatePixmap(display, root, 1, 1, 1/*depth*/);
  xgc.function = GXclear;
  gc =  XCreateGC(display, cursormask, GCFunction, &xgc);
  XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
  dummycolour.pixel = 0;
  dummycolour.red = 0;
  dummycolour.flags = 04;
  cursor = XCreatePixmapCursor(display, cursormask, cursormask,
                               &dummycolour,&dummycolour, 0,0);
  XFreePixmap(display,cursormask);
  XFreeGC(display,gc);
  return cursor;
}

static void install_grabs(void)
{
  // inviso cursor
  XWarpPointer(x11Display, None, x11Window,
               0, 0, 0, 0,
               glConfig.vidWidth / 2, glConfig.vidHeight / 2);
  XSync(x11Display, False);

  XDefineCursor(x11Display, x11Window, CreateNullCursor(x11Display, x11Window));

  XGrabPointer(x11Display, x11Window, // bk010108 - do this earlier?
               False,
               MOUSE_MASK,
               GrabModeAsync, GrabModeAsync,
               x11Window,
               None,
               CurrentTime);

  XGetPointerControl(x11Display, &mouse_accel_numerator, &mouse_accel_denominator,
                     &mouse_threshold);

  XChangePointerControl(x11Display, True, True, 1, 1, 0);

  XSync(x11Display, False);

  mouseResetTime = Sys_Milliseconds ();

    mwx = glConfig.vidWidth / 2;
    mwy = glConfig.vidHeight / 2;
    mx = my = 0;

  XGrabKeyboard(x11Display, x11Window,
                False,
                GrabModeAsync, GrabModeAsync,
                CurrentTime);

  XSync(x11Display, False);
}

static void uninstall_grabs(void)
{

  XChangePointerControl(x11Display, qtrue, qtrue, mouse_accel_numerator, 
                        mouse_accel_denominator, mouse_threshold);

  XUngrabPointer(x11Display, CurrentTime);
  XUngrabKeyboard(x11Display, CurrentTime);

  XWarpPointer(x11Display, None, x11Window,
               0, 0, 0, 0,
               glConfig.vidWidth / 2, glConfig.vidHeight / 2);

  // inviso cursor
  XUndefineCursor(x11Display, x11Window);
}

// bk001206 - from Ryan's Fakk2
/**
 * XPending() actually performs a blocking read 
 *  if no events available. From Fakk2, by way of
 *  Heretic2, by way of SDL, original idea GGI project.
 * The benefit of this approach over the quite
 *  badly behaved XAutoRepeatOn/Off is that you get
 *  focus handling for free, which is a major win
 *  with debug and windowed mode. It rests on the
 *  assumption that the X server will use the
 *  same timestamp on press/release event pairs 
 *  for key repeats. 
 */
static qboolean X11_PendingInput(void) {

  assert(x11Display != NULL);

  // Flush the display connection
  //  and look to see if events are queued
  XFlush( x11Display );
  if ( XEventsQueued( x11Display, QueuedAlready) )
  {
    return qtrue;
  }

  // More drastic measures are required -- see if X is ready to talk
  {
    static struct timeval zero_time;
    int x11_fd;
    fd_set fdset;

    x11_fd = ConnectionNumber( x11Display );
    FD_ZERO(&fdset);
    FD_SET(x11_fd, &fdset);
    if ( select(x11_fd+1, &fdset, NULL, NULL, &zero_time) == 1 )
    {
      return(XPending(x11Display));
    }
  }

  // Oh well, nothing is ready ..
  return qfalse;
}

// bk001206 - from Ryan's Fakk2. See above.
static qboolean repeated_press(XEvent *event)
{
  XEvent        peekevent;
  qboolean      repeated = qfalse;

  assert(x11Display != NULL);

  if (X11_PendingInput())
  {
    XPeekEvent(x11Display, &peekevent);

    if ((peekevent.type == KeyPress) &&
        (peekevent.xkey.keycode == event->xkey.keycode) &&
        (peekevent.xkey.time == event->xkey.time))
    {
      repeated = qtrue;
      XNextEvent(x11Display, &peekevent);  // skip event.
    } // if
  } // if

  return(repeated);
} // repeated_press


int Sys_XTimeToSysTime (Time xtime);
static void HandleEvents(void)
{
  int b;
  int key;
  XEvent event;
  qboolean dowarp = qfalse;
  char *p;
  int dx, dy;
  int t = 0; // default to 0 in case we don't set
	
  if (!x11Display)
    return;

  while (XPending(x11Display))
  {
    XNextEvent(x11Display, &event);
    switch (event.type)
    {
    case KeyPress:
			t = Sys_XTimeToSysTime(event.xkey.time);
      p = XLateKey(&event.xkey, &key);
      if (key)
      {
        Sys_QueEvent( t, SE_KEY, key, qtrue, 0, NULL );
      }
      if (p)
      {
        while (*p)
        {
          Sys_QueEvent( t, SE_CHAR, *p++, 0, 0, NULL );
        }
      }
      break;

    case KeyRelease:
			t = Sys_XTimeToSysTime(event.xkey.time);
      // bk001206 - handle key repeat w/o XAutRepatOn/Off
      //            also: not done if console/menu is active.
      // From Ryan's Fakk2.
      // see game/q_shared.h, KEYCATCH_* . 0 == in 3d game.  
      if (cls.keyCatchers == 0)
      {   // FIXME: KEYCATCH_NONE
        if (repeated_press(&event) == qtrue)
          continue;
      } // if
      XLateKey(&event.xkey, &key);

      Sys_QueEvent( t, SE_KEY, key, qfalse, 0, NULL );
      break;

    case MotionNotify:
			t = Sys_XTimeToSysTime(event.xkey.time);
      if (mouse_active)
      {
          // If it's a center motion, we've just returned from our warp
          if (event.xmotion.x == glConfig.vidWidth/2 &&
              event.xmotion.y == glConfig.vidHeight/2)
          {
            mwx = glConfig.vidWidth/2;
            mwy = glConfig.vidHeight/2;
            if (t - mouseResetTime > MOUSE_RESET_DELAY )
            {
              Sys_QueEvent( t, SE_MOUSE, mx, my, 0, NULL );
            }
            mx = my = 0;
            break;
          }

          dx = ((int)event.xmotion.x - mwx);
          dy = ((int)event.xmotion.y - mwy);
          if (abs(dx) > 1)
            mx += dx * 2;
          else
            mx += dx;
          if (abs(dy) > 1)
            my += dy * 2;
          else
            my += dy;

          mwx = event.xmotion.x;
          mwy = event.xmotion.y;
          dowarp = qtrue;
      }
      break;

    case ButtonPress:
		  t = Sys_XTimeToSysTime(event.xkey.time);
      if (event.xbutton.button == 4)
      {
        Sys_QueEvent( t, SE_KEY, K_MWHEELUP, qtrue, 0, NULL );
      } else if (event.xbutton.button == 5)
      {
        Sys_QueEvent( t, SE_KEY, K_MWHEELDOWN, qtrue, 0, NULL );
      } else
      {
        // NOTE TTimo there seems to be a weird mapping for K_MOUSE1 K_MOUSE2 K_MOUSE3 ..
        b=-1;
        if (event.xbutton.button == 1)
        {
          b = 0; // K_MOUSE1
        } else if (event.xbutton.button == 2)
        {
          b = 2; // K_MOUSE3
        } else if (event.xbutton.button == 3)
        {
          b = 1; // K_MOUSE2
        } else if (event.xbutton.button == 6)
        {
          b = 3; // K_MOUSE4
        } else if (event.xbutton.button == 7)
        {
          b = 4; // K_MOUSE5
        };

        Sys_QueEvent( t, SE_KEY, K_MOUSE1 + b, qtrue, 0, NULL );
      }
      break;

    case ButtonRelease:
		  t = Sys_XTimeToSysTime(event.xkey.time);
      if (event.xbutton.button == 4)
      {
        Sys_QueEvent( t, SE_KEY, K_MWHEELUP, qfalse, 0, NULL );
      } else if (event.xbutton.button == 5)
      {
        Sys_QueEvent( t, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL );
      } else
      {
        b=-1;
        if (event.xbutton.button == 1)
        {
          b = 0;
        } else if (event.xbutton.button == 2)
        {
          b = 2;
        } else if (event.xbutton.button == 3)
        {
          b = 1;
        } else if (event.xbutton.button == 6)
        {
          b = 3; // K_MOUSE4
        } else if (event.xbutton.button == 7)
        {
          b = 4; // K_MOUSE5
        };
        Sys_QueEvent( t, SE_KEY, K_MOUSE1 + b, qfalse, 0, NULL );
      }
      break;

    case CreateNotify :
      win_x = event.xcreatewindow.x;
      win_y = event.xcreatewindow.y;
      break;

    case ConfigureNotify :
      win_x = event.xconfigure.x;
      win_y = event.xconfigure.y;
      break;
    }
  }

  if (dowarp)
  {
    XWarpPointer(x11Display,None,x11Window,0,0,0,0, 
                 (glConfig.vidWidth/2),(glConfig.vidHeight/2));
  }
}

// NOTE TTimo for the tty console input, we didn't rely on those .. 
//   it's not very surprising actually cause they are not used otherwise
void KBD_Init(void)
{
}

void KBD_Close(void)
{
}

void IN_ActivateMouse( void ) 
{
  if (!mouse_avail || !x11Display || !x11Window)
    return;

  if (!mouse_active)
  {
	if (!in_nograb->value)
		install_grabs();
    mouse_active = qtrue;
  }
}

void IN_DeactivateMouse( void ) 
{
  if (!mouse_avail || !x11Display || !x11Window)
    return;

  if (mouse_active)
  {
		if (!in_nograb->value)
      uninstall_grabs();
    mouse_active = qfalse;
  }
}

#endif

EGLDisplay sglDisplay;
EGLConfig  sglConfig;
EGLContext sglContext;
EGLSurface sglSurface;

EGLSurface pglSurface;
EGLContext pglContext;


#define MAX_ARRAY_SIZE		1024

//static Q3ScreenView *_screenView;
//static EAGLContext *_context;
static GLenum _GLimp_beginmode;
static float _GLimp_texcoords[MAX_ARRAY_SIZE][2];
static float _GLimp_vertexes[MAX_ARRAY_SIZE][3];
static float _GLimp_colors[MAX_ARRAY_SIZE][4];
static GLuint _GLimp_numInputVerts, _GLimp_numOutputVerts;
static qboolean _GLimp_texcoordbuffer;
static qboolean _GLimp_colorbuffer;

unsigned int QGLBeginStarted = 0;

#ifdef QGL_CHECK_GL_ERRORS
void
QGLErrorBreak(void)
{
}

void
QGLCheckError(const char *message)
{
    GLenum error;
    static unsigned int errorCount = 0;

	error = _glGetError();
	if (error != GL_NO_ERROR)
	{
        if (errorCount == 100)
            Com_Printf("100 GL errors printed ... disabling further error reporting.\n");
        else if (errorCount < 100)
		{
            if (errorCount == 0)
                fprintf(stderr, "BREAK ON QGLErrorBreak to stop at the GL errors\n");
            fprintf(stderr, "OpenGL Error(%s): 0x%04x\n", message, (int)error);
            QGLErrorBreak();
        }
        ++errorCount;
    }
}

#endif // QGL_CHECK_GL_ERRORS

void
qglLockArrays(GLint i, GLsizei size)
{
	//UNIMPL();
}

void
qglUnlockArrays(void)
{
	//UNIMPL();
}


void
qglBegin(GLenum mode)
{
	assert(!QGLBeginStarted);
	QGLBeginStarted = qtrue;
	_GLimp_beginmode = mode;
	_GLimp_numInputVerts = _GLimp_numOutputVerts = 0;
	_GLimp_texcoordbuffer = qfalse;
	_GLimp_colorbuffer = qfalse;
}

void
qglDrawBuffer(GLenum mode)
{
	if (mode != GL_BACK)
		UNIMPL();
}

void
qglEnd(void)
{
	GLenum mode;

	assert(QGLBeginStarted);
	QGLBeginStarted = qfalse;

	if (_GLimp_texcoordbuffer)
	{
		qglTexCoordPointer(2, GL_FLOAT, sizeof(_GLimp_texcoords[0]), _GLimp_texcoords);
		qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	else
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

	if (_GLimp_colorbuffer)
	{
		qglColorPointer(4, GL_FLOAT, sizeof(_GLimp_colors[0]), _GLimp_colors);
		qglEnableClientState(GL_COLOR_ARRAY);
	}
	else
		qglDisableClientState(GL_COLOR_ARRAY);

	qglVertexPointer(3, GL_FLOAT, sizeof(_GLimp_vertexes[0]), _GLimp_vertexes);
	qglEnableClientState(GL_VERTEX_ARRAY);

	if (_GLimp_beginmode == IPHONE_QUADS)
		mode = GL_TRIANGLES;
	else if (_GLimp_beginmode == IPHONE_POLYGON)
		assert(0);
	else
		mode = _GLimp_beginmode;

	qglDrawArrays(mode, 0, _GLimp_numOutputVerts);
}

void
qglColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	GLfloat v[4] = {r, g, b, a};

	qglColor4fv(v);
}

void
qglColor4fv(GLfloat *v)
{
	if (QGLBeginStarted)
	{
		assert(_GLimp_numOutputVerts < MAX_ARRAY_SIZE);
		bcopy(v, _GLimp_colors[_GLimp_numOutputVerts], sizeof(_GLimp_colors[0]));
		_GLimp_colorbuffer = qtrue;
	}
	else
	{
		glColor4f(v[0], v[1], v[2], v[3]);
#ifdef QGL_CHECK_GL_ERRORS
		QGLCheckError("glColor4fv");
#endif // QGL_CHECK_GL_ERRORS
	}
}

void
qglTexCoord2f(GLfloat s, GLfloat t)
{
	GLfloat v[2] = {s, t};

	qglTexCoord2fv(v);
}

void
qglTexCoord2fv(GLfloat *v)
{
	assert(_GLimp_numOutputVerts < MAX_ARRAY_SIZE);
	bcopy(v, _GLimp_texcoords[_GLimp_numOutputVerts], sizeof(_GLimp_texcoords[0]));
	_GLimp_texcoordbuffer = qtrue;
}

void
qglVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	GLfloat v[3] = {x, y, z};

	qglVertex3fv(v);
}

void
qglVertex3fv(GLfloat *v)
{
	assert(_GLimp_numOutputVerts < MAX_ARRAY_SIZE);
	bcopy(v, _GLimp_vertexes[_GLimp_numOutputVerts++], sizeof(_GLimp_vertexes[0]));
	++_GLimp_numInputVerts;

	if (_GLimp_beginmode == IPHONE_QUADS && _GLimp_numInputVerts % 4 == 0)
	{
		assert(_GLimp_numOutputVerts < MAX_ARRAY_SIZE - 2);
		bcopy(_GLimp_vertexes[_GLimp_numOutputVerts - 4],
				_GLimp_vertexes[_GLimp_numOutputVerts],
				sizeof(_GLimp_vertexes[0]));
		bcopy(_GLimp_texcoords[_GLimp_numOutputVerts - 4],
				_GLimp_texcoords[_GLimp_numOutputVerts],
				sizeof(_GLimp_texcoords[0]));
		bcopy(_GLimp_vertexes[_GLimp_numOutputVerts - 2],
				_GLimp_vertexes[_GLimp_numOutputVerts + 1],
				sizeof(_GLimp_vertexes[0]));
		bcopy(_GLimp_texcoords[_GLimp_numOutputVerts - 2],
				_GLimp_texcoords[_GLimp_numOutputVerts + 1],
				sizeof(_GLimp_texcoords[0]));
		_GLimp_numOutputVerts+= 2;
	}
	else if (_GLimp_beginmode == IPHONE_POLYGON)
		assert(0);
}

void
qglCallList(GLuint list)
{
	UNIMPL();
}


qboolean ( * qwglSwapIntervalEXT)( int interval ) = NULL;
void ( * qglMultiTexCoord2fARB )( GLenum texture, float s, float t ) = NULL;
void ( * qglActiveTextureARB )( GLenum texture ) = NULL;
void ( * qglClientActiveTextureARB )( GLenum texture ) = NULL;

void ( * qglLockArraysEXT)( int, int) = NULL;
void ( * qglUnlockArraysEXT) ( void ) = NULL;

cvar_t *in_subframe;

/*
** GLW_InitExtensions
*/
void InitGLExtensions( void )
{

}


EGLint attrib_list_fsaa[] =
{
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BUFFER_SIZE,  0,
        EGL_DEPTH_SIZE,   16,
	EGL_SAMPLE_BUFFERS, 1,
    	EGL_SAMPLES,        4,
        EGL_NONE
};


EGLint attrib_list_pbuff[] =
{
	EGL_WIDTH, 800,
	EGL_HEIGHT, 480,
	EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGB,
	EGL_NONE
};

#ifdef GLES2

GLuint _frameBuffer;
GLuint _renderBuffer;
GLuint _depthBuffer;

#define GL_RGB565                         0x8D62
#define GL_DEPTH_COMPONENT16              0x81A5
#define GL_RENDERBUFFER_BINDING           0x8CA7
#define GL_RENDERBUFFER                   0x8D41
#define GL_DEPTH_ATTACHMENT               0x8D00
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_FRAMEBUFFER_BINDING            0x8CA6
#define GL_FRAMEBUFFER                    0x8D40


#define kColorFormat  GL_RGB565
#define kNumColorBits 16
#define kDepthFormat  GL_DEPTH_COMPONENT16
#define kNumDepthBits 16

void createSurface()
{
	pglContext = eglCreateContext(sglDisplay, sglConfig, NULL, NULL);
	if( pglContext==0 )
	{
		ri.Printf( PRINT_ALL, "Error: GL Context\n" );
	}

	EGLint attribs[64];
	int i = 0;
	attribs[i++] = EGL_WIDTH;
	attribs[i++] = glConfig.vidWidth;
	attribs[i++] = EGL_HEIGHT;
	attribs[i++] = glConfig.vidHeight;
	attribs[i++] = EGL_TEXTURE_FORMAT;
	attribs[i++] = EGL_TEXTURE_RGB;
	attribs[i++] = EGL_NONE;

	pglSurface = eglCreatePbufferSurface(sglDisplay, sglConfig, attribs);

	GLint oldFrameBuffer, oldRenderBuffer;

	qglGetIntegerv(GL_RENDERBUFFER_BINDING, &oldRenderBuffer);
	qglGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFrameBuffer);

	glGenRenderbuffers(1, &_renderBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, _renderBuffer);

 	if( !glRenderbufferStorage ( GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, glConfig.vidWidth, glConfig.vidHeight) )
	{
		glDeleteRenderbuffers(1, &_renderBuffer);
		glBindRenderbuffer(GL_RENDERBUFFER_BINDING, oldRenderBuffer);
		ri.Printf( PRINT_ALL, "glRenderbufferStorage failed\n");
	}


	glGenFramebuffers(1, &_frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _renderBuffer);
	glGenRenderbuffers(1, &_depthBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, _depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, kDepthFormat, glConfig.vidWidth, glConfig.vidHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthBuffer);

	glBindRenderbuffer(GL_FRAMEBUFFER, oldFrameBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, oldRenderBuffer);

}
#endif

void GLimp_EndFrame( void ) 
{
#ifdef GLES2
	EGLContext oldContext = eglGetCurrentContext() ;
	GLuint oldRenderBuffer;

	if (oldContext != pglContext)
		eglMakeCurrent(sglDisplay, pglSurface, pglSurface, pglContext);


	qglGetIntegerv(GL_RENDERBUFFER_BINDING, (GLint *)&oldRenderBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, _renderBuffer);

	if ( !glIsRenderbuffer( GL_RENDERBUFFER ) )
	{
		ri.Printf( PRINT_ALL, "Failed to swap buffer\n");$
	}

	if (oldContext != sglContext)
		eglMakeCurrent(sglDisplay, sglSurface, sglSurface, sglContext);
#else
	eglSwapBuffers( sglDisplay, sglSurface );
#endif
}

void
GLimp_SetGamma(unsigned char red[256], unsigned char green[256], unsigned char blue[256])
{
#ifdef X11
	// NOTE TTimo we get the gamma value from cvar, because we can't work with the s_gammatable
	//   the API wasn't changed to avoid breaking other OSes
	float g = Cvar_Get("r_gamma", "1.0", 0)->value;
	XF86VidModeGamma gamma;
//	assert(glConfig.deviceSupportsGamma);
	glConfig.deviceSupportsGamma = qtrue;
	gamma.red = g;
	gamma.green = g;
	gamma.blue = g;
	XF86VidModeSetGamma(x11Display, x11Screen, &gamma);
#else
	UNIMPL();
#endif
}

#ifdef X11
static void GLW_InitGamma()
{
	/* Minimum extension version required */
	#define GAMMA_MINMAJOR 2
	#define GAMMA_MINMINOR 0
  
	glConfig.deviceSupportsGamma = qfalse;

	if (vidmode_ext)
	{
		if (vidmode_MajorVersion < GAMMA_MINMAJOR || 
			(vidmode_MajorVersion == GAMMA_MINMAJOR && vidmode_MinorVersion < GAMMA_MINMINOR))
		{
			ri.Printf( PRINT_ALL, "XF86 Gamma extension not supported in this version\n");
			return;
		}

		XF86VidModeGetGamma(x11Display, x11Screen, &vidmode_InitialGamma);
		ri.Printf( PRINT_ALL, "XF86 Gamma extension initialized\n");
		glConfig.deviceSupportsGamma = qtrue;
	}
}

#endif

EGLint attrib_list[] =
{
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_BUFFER_SIZE, 0,
	EGL_DEPTH_SIZE, 16,
	EGL_STENCIL_SIZE, 8,
	EGL_NONE
};

#ifdef X11
void GLimp_InitGLES()
{
	Window	sRootWindow;
	XSetWindowAttributes	sWA;
	unsigned int	ui32Mask;
	int	i32Depth;

	int actualWidth, actualHeight;

	// Initializes the display and screen
	x11Display = XOpenDisplay( ":0" );
	if (!x11Display)
	{
		ri.Printf( PRINT_ALL, "Error: Unable to open X display\n");
	}

	x11Screen = XDefaultScreen( x11Display );
	sRootWindow = RootWindow(x11Display, x11Screen);

	actualWidth = glConfig.vidWidth;
	actualHeight = glConfig.vidHeight;

	// Get video mode list
	if (!XF86VidModeQueryVersion(x11Display, &vidmode_MajorVersion, &vidmode_MinorVersion))
	{
		ri.Printf(PRINT_ALL, "XFree86-VidModeExtension Unvailable\n");
		vidmode_ext = qfalse;
	} else
	{
		ri.Printf(PRINT_ALL, "Using XFree86-VidModeExtension Version %d.%d\n",
					vidmode_MajorVersion, vidmode_MinorVersion);
		vidmode_ext = qtrue;
	}

	if (vidmode_ext)
	{
		int best_fit, best_dist, dist, x, y;

		XF86VidModeGetAllModeLines(x11Display, x11Screen, &num_vidmodes, &vidmodes);

		// Are we going fullscreen?  If so, let's change video mode
		int fullscreen = 1;
		if (fullscreen)
		{
			best_dist = 9999999;
			best_fit = -1;

			int i;
			for (i = 0; i < num_vidmodes; i++)
			{
				if (glConfig.vidWidth > vidmodes[i]->hdisplay ||
					glConfig.vidHeight > vidmodes[i]->vdisplay)
				continue;

				x = glConfig.vidWidth - vidmodes[i]->hdisplay;
				y = glConfig.vidHeight - vidmodes[i]->vdisplay;
				dist = (x * x) + (y * y);
				if (dist < best_dist)
				{
					best_dist = dist;
					best_fit = i;
				}
			}

			if (best_fit != -1)
			{
				actualWidth = vidmodes[best_fit]->hdisplay;
				actualHeight = vidmodes[best_fit]->vdisplay;

				// change to the mode
				XF86VidModeSwitchToMode(x11Display, x11Screen, vidmodes[best_fit]);
				vidmode_active = qtrue;

 				// Move the viewport to top left
				XF86VidModeSetViewPort(x11Display, x11Screen, 0, 0);

  				ri.Printf(PRINT_ALL, "XFree86-VidModeExtension Activated at %dx%d\n",
      					actualWidth, actualHeight);

			}
			else
			{
				fullscreen = 0;
				ri.Printf(PRINT_ALL, "XFree86-VidModeExtension: No acceptable modes found\n");
			}
		}
		else
		{
			ri.Printf(PRINT_ALL, "XFree86-VidModeExtension:  Ignored on non-fullscreen/Voodoo\n");
		}
	}

	// Gets the display parameters so we can pass the same parameters to the window to be created.
	i32Depth	= DefaultDepth(x11Display, x11Screen);
	x11Visual = malloc(sizeof(XVisualInfo));

	XMatchVisualInfo( x11Display, x11Screen, i32Depth, TrueColor, x11Visual);
	if (!x11Visual)
	{
		ri.Printf( PRINT_ALL, "Error: Unable to acquire visual\n");
	}

	sWA.background_pixel = BlackPixel(x11Display, x11Screen);
	sWA.border_pixel = 0;
	sWA.colormap = XCreateColormap(x11Display, sRootWindow, x11Visual->visual, AllocNone);
	sWA.event_mask = X_MASK;
	if (vidmode_active)
	{
 		ui32Mask = CWBackPixel | CWColormap | CWSaveUnder | CWBackingStore | 
			CWEventMask | CWOverrideRedirect;
		sWA.override_redirect = True;
		sWA.backing_store = NotUseful;
		sWA.save_under = False;
	} else
		ui32Mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

	x11Window = XCreateWindow( x11Display, RootWindow(x11Display, x11Screen), 0, 0, glConfig.vidWidth, glConfig.vidHeight,
								 0, CopyFromParent, InputOutput, CopyFromParent, ui32Mask, &sWA);
	XMapWindow(x11Display, x11Window);
	XFlush(x11Display);

	sglDisplay = eglGetDisplay((NativeDisplayType)x11Display);
	if( sglDisplay == EGL_NO_DISPLAY )
	{
		ri.Printf( PRINT_ALL, "GL No Display\n" );
	}

	EGLint iMajorVersion, iMinorVersion;
	if (!eglInitialize(sglDisplay, &iMajorVersion, &iMinorVersion))
	{
		ri.Printf( PRINT_ALL, "Error: eglInitialize() failed.\n");
	}


	EGLint *attribList = NULL;
	attribList = attrib_list;

	int iConfigs;
	if (!eglChooseConfig(sglDisplay, attribList, &sglConfig, 1, &iConfigs) || (iConfigs != 1))
	{
		ri.Printf( PRINT_ALL, "Error: eglChooseConfig() failed.\n");
	}

	sglSurface = eglCreateWindowSurface(sglDisplay, sglConfig, (NativeWindowType)x11Window, NULL);
	if( sglSurface==0 )
	{
		ri.Printf( PRINT_ALL, "Error: GL surface\n" );
	}

	sglContext = eglCreateContext(sglDisplay, sglConfig, NULL, NULL);
	if( sglContext==0 )
	{
		ri.Printf( PRINT_ALL, "Error: GL Context\n" );
	}

	eglMakeCurrent(sglDisplay, sglSurface, sglSurface, sglContext);
}
#else
void GLimp_InitGLES()
{
	EGLint *attribList = NULL;
	attribList = attrib_list;

	EGLint numConfigs;
	EGLint majorVersion;
	EGLint minorVersion;
	
	sglDisplay = eglGetDisplay( (NativeDisplayType)0 );
	if( sglDisplay == EGL_NO_DISPLAY )
	{
		ri.Printf( PRINT_ALL, "GL No Display\n" );
	}

	if( !eglInitialize( sglDisplay, &majorVersion, &minorVersion ) ) 
	{
		ri.Printf( PRINT_ALL, "GL Init\n" );
	}

	if( !eglChooseConfig( sglDisplay, attribList, &sglConfig, 1, &numConfigs ) )
	{
		ri.Printf( PRINT_ALL, "GL Config\n" );
	}

	sglContext = eglCreateContext( sglDisplay, sglConfig, NULL, NULL );
	if( sglContext==0 )
	{
		ri.Printf( PRINT_ALL, "GL Context\n" );
	}

	sglSurface = eglCreateWindowSurface( sglDisplay, sglConfig, (NativeWindowType)0, NULL );
	if( sglSurface==0 )
	{
		ri.Printf( PRINT_ALL, "GL surface\n" );
	}
	
	eglMakeCurrent( sglDisplay, sglSurface, sglSurface, sglContext );    

#ifdef GLES2
	createSurface();
#endif
	qglHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	qglHint(GL_FOG_HINT, GL_FASTEST);
	qglHint(GL_LINE_SMOOTH_HINT, GL_FASTEST);
}
#endif

void GLimp_SetMode(int w, int h)
{
	glConfig.isFullscreen = qtrue;

	glConfig.colorBits = 16;
	glConfig.depthBits = 16;
	glConfig.stencilBits = 8;

	glConfig.vidWidth = w;
	glConfig.vidHeight = h;
	glConfig.windowAspect = (float)glConfig.vidWidth / glConfig.vidHeight;
	
	glConfig.stereoEnabled = qfalse;

	ri.Printf( PRINT_ALL, "CALL: GLimp_SetMode(%ix%i)\n", w, h );

	//R_GetModeInfo( &glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, -1 );

}

void GLimp_Init( void )
{
	ri.Printf( PRINT_ALL, "Initializing OpenGL subsystem\n" );

	bzero(&glConfig, sizeof(glConfig));

	GLimp_SetMode(800, 480);

	GLimp_InitGLES();
    
	Q_strncpyz(glConfig.vendor_string, (const char *)qglGetString(GL_VENDOR), sizeof(glConfig.vendor_string));
	Q_strncpyz(glConfig.renderer_string, (const char *)qglGetString(GL_RENDERER), sizeof(glConfig.renderer_string));
	Q_strncpyz(glConfig.version_string, (const char *)qglGetString(GL_VERSION), sizeof(glConfig.version_string));
	Q_strncpyz(glConfig.extensions_string,
			(const char *)qglGetString(GL_EXTENSIONS),
			sizeof(glConfig.extensions_string));

	qglLockArraysEXT = qglLockArrays;
	qglUnlockArraysEXT = qglUnlockArrays;

	glConfig.textureCompression = TC_NONE;

	GLW_InitGamma();
}

void GLimp_Shutdown( void ) 
{
	eglMakeCurrent(sglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface( sglDisplay, sglSurface ); 
	eglDestroyContext( sglDisplay, sglContext );
	eglTerminate(sglDisplay);

#ifdef X11
	if (x11Window) XDestroyWindow(x11Display, x11Window);
	if (x11Colormap) XFreeColormap( x11Display, x11Colormap );
	if (x11Display) XCloseDisplay(x11Display);
	free(x11Visual);

	if (glConfig.deviceSupportsGamma)
	{
		XF86VidModeSetGamma(x11Display, x11Screen, &vidmode_InitialGamma);
	}

	IN_DeactivateMouse();
#endif
    
	memset( &glConfig, 0, sizeof( glConfig ) );
	memset( &glState, 0, sizeof( glState ) );

}

void GLimp_AcquireGL() { };

void GLimp_ReleaseGL() { };

void GLimp_EnableLogging( qboolean enable ) 
{
}

void GLimp_LogComment( char *comment ) 
{
}

qboolean QGL_Init( const char *dllname ) 
{
	return qtrue;
}

void QGL_Shutdown( void ) 
{
}

void
qglArrayElement(GLint i)
{
	UNIMPL();
}

void *GLimp_RendererSleep( void ) { return NULL;}
qboolean GLimp_SpawnRenderThread( void (*function)( void ) ) { return qfalse;}
void GLimp_FrontEndSleep( void ) {}
void GLimp_WakeRenderer( void *data ) {}
//void GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] ) {}


#ifdef X11
/*****************************************************************************/
/* MOUSE                                                                     */
/*****************************************************************************/

void IN_Init(void) {
	Com_Printf ("\n------- Input Initialization -------\n");
	// mouse variables
	in_mouse = Cvar_Get ("in_mouse", "1", CVAR_ARCHIVE);
	
	// turn on-off sub-frame timing of X events
	in_subframe = Cvar_Get ("in_subframe", "1", CVAR_ARCHIVE);
	
	// developer feature, allows to break without loosing mouse pointer
	in_nograb = Cvar_Get ("in_nograb", "0", 0);

	// bk001130 - from cvs.17 (mkv), joystick variables
	in_joystick = Cvar_Get ("in_joystick", "0", CVAR_ARCHIVE|CVAR_LATCH);
	// bk001130 - changed this to match win32
	in_joystickDebug = Cvar_Get ("in_debugjoystick", "0", CVAR_TEMP);
	joy_threshold = Cvar_Get ("joy_threshold", "0.15", CVAR_ARCHIVE); // FIXME: in_joythreshold


	in_mouse->value = qtrue;

	if (in_mouse->value)
		mouse_avail = qtrue;
	else
		mouse_avail = qfalse;

//  IN_StartupJoystick( ); // bk001130 - from cvs1.17 (mkv)
	Com_Printf ("------------------------------------\n");
}

void IN_Shutdown(void)
{
	mouse_avail = qfalse;
}

void IN_Frame (void) {

	// bk001130 - from cvs 1.17 (mkv)
	//  IN_JoyMove(); // FIXME: disable if on desktop?

	if ( cls.keyCatchers & KEYCATCH_CONSOLE )
	{
		// temporarily deactivate if not in the game and
		// running on the desktop
		// voodoo always counts as full screen
		if (Cvar_VariableValue ("r_fullscreen") == 0
			&& strcmp( Cvar_VariableString("r_glDriver"), _3DFX_DRIVER_NAME ) )
		{
			IN_DeactivateMouse ();
			return;
		}
	}

	IN_ActivateMouse();
}

void IN_Activate(void)
{

}

// bk001130 - cvs1.17 joystick code (mkv) was here, no linux_joystick.c

void Sys_SendKeyEvents (void)
{
  // XEvent event; // bk001204 - unused

	if (!x11Display) return;

	HandleEvents();
}

#else

void IN_Init(void)
{
	in_subframe = Cvar_Get ("in_subframe", "1", CVAR_ARCHIVE);
}

void IN_Shutdown(void)
{
}

void IN_Frame (void) 
{
}

void IN_Activate(void)
{
}
#endif


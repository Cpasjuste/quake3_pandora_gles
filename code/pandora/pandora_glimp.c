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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/xf86vmode.h>

#define KEY_MASK (KeyPressMask | KeyReleaseMask)
#define MOUSE_MASK (ButtonPressMask | ButtonReleaseMask | \
		    PointerMotionMask | ButtonMotionMask )
#define X_MASK (KEY_MASK | MOUSE_MASK | VisibilityChangeMask | StructureNotifyMask )


int x11 = 0;

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


EGLDisplay sglDisplay;
EGLConfig  sglConfig;
EGLContext sglContext;
EGLSurface sglSurface;

EGLSurface pglSurface;
EGLContext pglContext;

#define MAX_ARRAY_SIZE		1024

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
/*
	// NOTE TTimo we get the gamma value from cvar, because we can't work with the s_gammatable
	//   the API wasn't changed to avoid breaking other OSes

	float g = Cvar_Get("r_gamma", "1.0", 0)->value;
	XF86VidModeGamma gamma;

	glConfig.deviceSupportsGamma = qtrue;
	assert(glConfig.deviceSupportsGamma);
	
	gamma.red = g;
	gamma.green = g;
	gamma.blue = g;
	ri.Printf( PRINT_ALL, "XF86VidModeSetGamma\n");
	XF86VidModeSetGamma(x11Display, x11Screen, &gamma);
*/

}

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

EGLint attrib_list[] =
{
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_BUFFER_SIZE, 0,
	EGL_DEPTH_SIZE, 16,
//	EGL_STENCIL_SIZE, 8,
	EGL_NONE
};

void GLimp_InitGLESx11()
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

void GLimp_InitGLESraw()
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

void GLimp_SetMode(int w, int h)
{
	glConfig.isFullscreen = qtrue;

	glConfig.colorBits = 16;
	glConfig.depthBits = 16;
	glConfig.stencilBits = 0;

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

	if(x11)
		ri.Printf( PRINT_ALL, "Video Mode : x11\n" );
	else
		ri.Printf( PRINT_ALL, "Video Mode : Raw\n" );

	bzero(&glConfig, sizeof(glConfig));

	GLimp_SetMode(800, 480);

	if(x11)
		GLimp_InitGLESx11();
	else
		GLimp_InitGLESraw();
    
	Q_strncpyz(glConfig.vendor_string, (const char *)qglGetString(GL_VENDOR), sizeof(glConfig.vendor_string));
	Q_strncpyz(glConfig.renderer_string, (const char *)qglGetString(GL_RENDERER), sizeof(glConfig.renderer_string));
	Q_strncpyz(glConfig.version_string, (const char *)qglGetString(GL_VERSION), sizeof(glConfig.version_string));
	Q_strncpyz(glConfig.extensions_string,
			(const char *)qglGetString(GL_EXTENSIONS),
			sizeof(glConfig.extensions_string));

	qglLockArraysEXT = qglLockArrays;
	qglUnlockArraysEXT = qglUnlockArrays;

	glConfig.textureCompression = TC_NONE;

	if(x11)
	{
		//GLW_InitGamma();
		//GLimp_SetGamma(NULL, NULL, NULL);
	}
}

void GLimp_Shutdown( void ) 
{
	eglMakeCurrent(sglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface( sglDisplay, sglSurface ); 
	eglDestroyContext( sglDisplay, sglContext );
	eglTerminate(sglDisplay);

	if(x11)
	{
/*
		if (glConfig.deviceSupportsGamma)
		{
			XF86VidModeSetGamma(x11Display, x11Screen, &vidmode_InitialGamma);
		}
*/
		if (x11Window) XDestroyWindow(x11Display, x11Window);
		if (x11Colormap) XFreeColormap( x11Display, x11Colormap );
		if (x11Display) XCloseDisplay(x11Display);
		if (x11Visual) free(x11Visual);

		IN_DeactivateMouse();
	}
    
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




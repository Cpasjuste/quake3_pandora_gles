#include "../game/q_shared.h"
#include "../qcommon/qcommon.h"

#define UNIMPL()	Com_Printf("%s(): Unimplemented\n", __FUNCTION__)

#ifndef PANDORA_GLIM_H
#define PANDORA_GLIM_H

#include <GLES/gl.h>
#include <GLES/egl.h>

extern void qglArrayElement(GLint i);
extern void qglLockArrays(GLint i, GLsizei size);
extern void qglUnlockArrays(void);

enum
{
	IPHONE_QUADS = 0x10000,
	IPHONE_POLYGON
};

#undef GL_CLAMP
#define GL_CLAMP				GL_CLAMP_TO_EDGE
#undef GL_LINE
#define GL_LINE					0x0001
#undef GL_FILL
#define GL_FILL					0
#undef GL_RGB5
#define GL_RGB5					0x8057
#undef GL_RGB8
#define GL_RGB8					0x8051
#undef GL_RGBA4
#define GL_RGBA4				0x8056
#undef GL_RGBA8
#define GL_RGBA8				0x8058
#undef GL_QUADS
#define GL_QUADS				IPHONE_QUADS
#undef GL_STENCIL_INDEX
#define GL_STENCIL_INDEX		0x1901
#undef GL_BACK_LEFT
#define GL_BACK_LEFT			GL_FALSE
#undef GL_BACK_RIGHT
#define GL_BACK_RIGHT			GL_FALSE
#undef GL_DEPTH_COMPONENT
#define GL_DEPTH_COMPONENT		0x1902
#undef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR	GL_FALSE
#undef GL_POLYGON
#define GL_POLYGON				IPHONE_POLYGON
#undef GL_UNSIGNED_INT
#define GL_UNSIGNED_INT			0x1405

void qglBegin(GLenum mode);
void qglDrawBuffer(GLenum mode);
void qglEnd(void);
#define qglOrtho qglOrthof
#define qglPolygonMode(f, m)
void qglTexCoord2f(GLfloat s, GLfloat t);
#define qglVertex2f(x, y)	qglVertex3f(x, y, 0.0)
void qglVertex3f(GLfloat x, GLfloat y, GLfloat z);
void qglVertex3fv(GLfloat *v);
#define qglClipPlane qglClipPlanef
#define qglColor3f(r, g, b)	qglColor4f(r, g, b, 1.0f)
void qglColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void qglColor4fv(GLfloat *v);
#define qglDepthRange qglDepthRangef
#define qglClearDepth qglClearDepthf
#define qglColor4ubv(v)		qglColor4f(v[0] / 255.0, v[1] / 255.0, v[2] / 255.0, v[3] / 255.0)
void qglTexCoord2fv(GLfloat *v);
void qglCallList(GLuint list);

void GLimp_MakeCurrent(void);

#endif // IPHONE_GLIMP_H

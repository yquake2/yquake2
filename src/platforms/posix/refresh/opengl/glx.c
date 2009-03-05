/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/*
** GLW_IMP.C
**
** This file contains ALL Linux specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_Shutdown
** GLimp_SwitchFullscreen
**
*/

#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <dlfcn.h>
#ifdef Joystick
#include <fcntl.h>
#endif
#include "../ref_gl/gl_local.h"

#include "../client/keys.h"

#include "../linux/rw_linux.h"
#include "../linux/glw_linux.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#include <X11/extensions/xf86dga.h>
#include <X11/extensions/xf86vmode.h>
#ifdef Joystick
# if defined (__linux__)
#include <linux/joystick.h>
# elif defined (__FreeBSD__)
#include <sys/joystick.h>
# endif
#include <glob.h>
#endif
#include <GL/glx.h>

glwstate_t glw_state;

static Display *dpy = NULL;
static int scrnum;
static Window win;
static GLXContext ctx = NULL;
static Atom wmDeleteWindow;

#ifdef Joystick
static int joy_fd;
#endif

#define KEY_MASK (KeyPressMask | KeyReleaseMask)
#define MOUSE_MASK (ButtonPressMask | ButtonReleaseMask | \
		    PointerMotionMask | ButtonMotionMask )
#define X_MASK (KEY_MASK | MOUSE_MASK | VisibilityChangeMask | StructureNotifyMask )

//GLX Functions
static XVisualInfo * (*qglXChooseVisual)( Display *dpy, int screen, int *attribList );
static GLXContext (*qglXCreateContext)( Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct );
static void (*qglXDestroyContext)( Display *dpy, GLXContext ctx );
static Bool (*qglXMakeCurrent)( Display *dpy, GLXDrawable drawable, GLXContext ctx);
static void (*qglXCopyContext)( Display *dpy, GLXContext src, GLXContext dst, GLuint mask );
static void (*qglXSwapBuffers)( Display *dpy, GLXDrawable drawable );
static int (*qglXGetConfig) (Display *dpy, XVisualInfo *vis, int attrib, int *value);




/*****************************************************************************/
/* MOUSE                                                                     */
/*****************************************************************************/

// this is inside the renderer shared lib, so these are called from vid_so

int mx, my, mouse_buttonstate;
int win_x, win_y;

static cvar_t	*in_dgamouse;

static cvar_t	*r_fakeFullscreen;

static XF86VidModeModeInfo **vidmodes;
static int num_vidmodes;
static qboolean vidmode_active = false;
static XF86VidModeGamma oldgamma;

static qboolean mouse_active = false;
static qboolean dgamouse = false;
static qboolean vidmode_ext = false;

/* stencilbuffer shadows */
qboolean have_stencil = false;

static Time myxtime;

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
  XDefineCursor(dpy, win, CreateNullCursor(dpy, win));
  
  XGrabPointer(dpy, win,
	       True,
	       0,
	       GrabModeAsync, GrabModeAsync,
	       win,
	       None,
	       CurrentTime);
  
  if (in_dgamouse->value) {
    int MajorVersion, MinorVersion;
    
    if (!XF86DGAQueryVersion(dpy, &MajorVersion, &MinorVersion)) { 
      // unable to query, probalby not supported
      ri.Con_Printf( PRINT_ALL, "Failed to detect XF86DGA Mouse\n" );
      ri.Cvar_Set( "in_dgamouse", "0" );
    } else {
      dgamouse = true;
      XF86DGADirectVideo(dpy, DefaultScreen(dpy), XF86DGADirectMouse);
      XWarpPointer(dpy, None, win, 0, 0, 0, 0, 0, 0);
    }
  } else {
    XWarpPointer(dpy, None, win,
		 0, 0, 0, 0,
		 vid.width / 2, vid.height / 2);
  }
  
  XGrabKeyboard(dpy, win,
		False,
		GrabModeAsync, GrabModeAsync,
		CurrentTime);
  
  mouse_active = true;
  
  //	XSync(dpy, True);
}

static void uninstall_grabs(void)
{
  if (!dpy || !win)
    return;
  
  if (dgamouse) {
    dgamouse = false;
    XF86DGADirectVideo(dpy, DefaultScreen(dpy), 0);
  }
  
  XUngrabPointer(dpy, CurrentTime);
  XUngrabKeyboard(dpy, CurrentTime);
  
  // inviso cursor
  XUndefineCursor(dpy, win);
  
  mouse_active = false;
}

static void IN_DeactivateMouse( void ) 
{
  //if (!mouse_avail || !dpy || !win)
  //return;
  
  if (mouse_active) {
    uninstall_grabs();
    mouse_active = false;
  }
}

static void IN_ActivateMouse( void ) 
{
  //if (!mouse_avail || !dpy || !win)
  //return;
  
  if (!mouse_active) {
    mx = my = 0; // don't spazz
    install_grabs();
    mouse_active = true;
  }
}

void getMouse(int *x, int *y, int *state) {
  *x = mx;
  *y = my;
  *state = mouse_buttonstate;
}

void doneMouse() {
  mx = my = 0;
}

void RW_IN_PlatformInit()
{
  
  in_dgamouse = ri.Cvar_Get ("in_dgamouse", "0", CVAR_ARCHIVE);
}

void RW_IN_Activate(qboolean active)
{
  if (active || vidmode_active)
    IN_ActivateMouse();
  else
    IN_DeactivateMouse ();
}

/*****************************************************************************/
/* KEYBOARD                                                                  */
/*****************************************************************************/

static int XLateKey(XKeyEvent *ev)
{

  int key;
  char buf[64];
  KeySym keysym;
  
  key = 0;
  
  XLookupString(ev, buf, sizeof buf, &keysym, 0);
  
  switch(keysym)
    {
    case XK_KP_Page_Up:	 key = K_KP_PGUP; break;
    case XK_Page_Up:	 key = K_PGUP; break;
      
    case XK_KP_Page_Down: key = K_KP_PGDN; break;
    case XK_Page_Down:	 key = K_PGDN; break;
      
    case XK_KP_Home: key = K_KP_HOME; break;
    case XK_Home:	 key = K_HOME; break;
      
    case XK_KP_End:  key = K_KP_END; break;
    case XK_End:	 key = K_END; break;
      
    case XK_KP_Left: key = K_KP_LEFTARROW; break;
    case XK_Left:	 key = K_LEFTARROW; break;
      
    case XK_KP_Right: key = K_KP_RIGHTARROW; break;
    case XK_Right:	key = K_RIGHTARROW;		break;
      
    case XK_KP_Down: key = K_KP_DOWNARROW; break;
    case XK_Down:	 key = K_DOWNARROW; break;
      
    case XK_KP_Up:   key = K_KP_UPARROW; break;
    case XK_Up:		 key = K_UPARROW;	 break;
      
    case XK_Escape: key = K_ESCAPE;		break;
      
    case XK_KP_Enter: key = K_KP_ENTER;	break;
    case XK_Return: key = K_ENTER;		 break;
      
    case XK_Tab:		key = K_TAB;			 break;
      
    case XK_F1:		 key = K_F1;				break;
      
    case XK_F2:		 key = K_F2;				break;
      
    case XK_F3:		 key = K_F3;				break;
      
    case XK_F4:		 key = K_F4;				break;
      
    case XK_F5:		 key = K_F5;				break;
      
    case XK_F6:		 key = K_F6;				break;
      
    case XK_F7:		 key = K_F7;				break;
      
    case XK_F8:		 key = K_F8;				break;
      
    case XK_F9:		 key = K_F9;				break;
      
    case XK_F10:		key = K_F10;			 break;
      
    case XK_F11:		key = K_F11;			 break;
      
    case XK_F12:		key = K_F12;			 break;
      
    case XK_BackSpace: key = K_BACKSPACE; break;
      
    case XK_KP_Delete: key = K_KP_DEL; break;
    case XK_Delete: key = K_DEL; break;
      
    case XK_Pause:	key = K_PAUSE;		 break;
      
    case XK_Shift_L:
    case XK_Shift_R:	key = K_SHIFT;		break;
      
		case XK_Execute: 
		case XK_Control_L: 
		case XK_Control_R:	key = K_CTRL;		 break;

		case XK_Alt_L:	
		case XK_Meta_L: 
		case XK_Alt_R:	
		case XK_Meta_R: key = K_ALT;			break;

		case XK_KP_Begin: key = K_KP_5;	break;

		case XK_Insert:key = K_INS; break;
		case XK_KP_Insert: key = K_KP_INS; break;

		case XK_KP_Multiply: key = '*'; break;
		case XK_KP_Add:  key = K_KP_PLUS; break;
		case XK_KP_Subtract: key = K_KP_MINUS; break;
		case XK_KP_Divide: key = K_KP_SLASH; break;

#if 0
		case 0x021: key = '1';break;/* [!] */
		case 0x040: key = '2';break;/* [@] */
		case 0x023: key = '3';break;/* [#] */
		case 0x024: key = '4';break;/* [$] */
		case 0x025: key = '5';break;/* [%] */
		case 0x05e: key = '6';break;/* [^] */
		case 0x026: key = '7';break;/* [&] */
		case 0x02a: key = '8';break;/* [*] */
		case 0x028: key = '9';;break;/* [(] */
		case 0x029: key = '0';break;/* [)] */
		case 0x05f: key = '-';break;/* [_] */
		case 0x02b: key = '=';break;/* [+] */
		case 0x07c: key = '\'';break;/* [|] */
		case 0x07d: key = '[';break;/* [}] */
		case 0x07b: key = ']';break;/* [{] */
		case 0x022: key = '\'';break;/* ["] */
		case 0x03a: key = ';';break;/* [:] */
		case 0x03f: key = '/';break;/* [?] */
		case 0x03e: key = '.';break;/* [>] */
		case 0x03c: key = ',';break;/* [<] */
#endif

		default:
			key = *(unsigned char*)buf;
			if (key >= 'A' && key <= 'Z')
				key = key - 'A' + 'a';
			if (key >= 1 && key <= 26) /* ctrl+alpha */
				key = key + 'a' - 1;
			break;
	} 

	return key;
}


/* Check to see if this is a repeated key.
   (idea shamelessly lifted from SDL who...)
   (idea shamelessly lifted from GII -- thanks guys! :)
   This has bugs if two keys are being pressed simultaneously and the
   events start getting interleaved.
*/
int X11_KeyRepeat(Display *display, XEvent *event)
{
	XEvent peekevent;
	int repeated;

	repeated = 0;
	if ( XPending(display) ) {
		XPeekEvent(display, &peekevent);
		if ( (peekevent.type == KeyPress) &&
		     (peekevent.xkey.keycode == event->xkey.keycode) &&
		     ((peekevent.xkey.time-event->xkey.time) < 2) ) {
		  repeated = 1;
		  XNextEvent(display, &peekevent);
		}
	}
	return(repeated);
}


static void HandleEvents(void)
{
  XEvent event;
  int b;
  qboolean dowarp = false;
  int mwx = vid.width/2;
  int mwy = vid.height/2;
  in_state_t *in_state = getState();
  if (!dpy)
    return;
  
  while (XPending(dpy)) {
    //ri.Con_Printf(PRINT_ALL,"Bar");
    XNextEvent(dpy, &event);
    mx = my = 0;
    switch(event.type) {
    case KeyPress:
      myxtime = event.xkey.time;
      if (in_state && in_state->Key_Event_fp)
	in_state->Key_Event_fp (XLateKey(&event.xkey), true);
      break;
    case KeyRelease:
      if (! X11_KeyRepeat(dpy, &event)) {
	if (in_state && in_state->Key_Event_fp)
	  in_state->Key_Event_fp (XLateKey(&event.xkey), false);
      }
      break;
    case MotionNotify:
      if (mouse_active) {
	if (dgamouse) {
	  mx += (event.xmotion.x + win_x) * 2;
	  my += (event.xmotion.y + win_y) * 2;
	}
	else 
	  {
	    mx -= ((int)event.xmotion.x - mwx) * 2;
	    my -= ((int)event.xmotion.y - mwy) * 2;
	    mwx = event.xmotion.x;
	    mwy = event.xmotion.y;
	    
	    if (mx || my)
	      dowarp = true;
	  }
      }
      break;
      
      
    case ButtonPress:
      myxtime = event.xbutton.time;
      
      b=-1;
      if (event.xbutton.button == 1)
	b = 0;
      else if (event.xbutton.button == 2)
	b = 2;
      else if (event.xbutton.button == 3)
	b = 1;
      else if (event.xbutton.button == 4)
	in_state->Key_Event_fp (K_MWHEELUP, 1);
      else if (event.xbutton.button == 5)
	in_state->Key_Event_fp (K_MWHEELDOWN, 1);
      if (b>=0 && in_state && in_state->Key_Event_fp)
	in_state->Key_Event_fp (K_MOUSE1 + b, true);
      if (b>=0)
	mouse_buttonstate |= 1<<b;
      break;
      
    case ButtonRelease:
      b=-1;
      if (event.xbutton.button == 1)
	b = 0;
      else if (event.xbutton.button == 2)
	b = 2;
      else if (event.xbutton.button == 3)
	b = 1;
      else if (event.xbutton.button == 4)
	in_state->Key_Event_fp (K_MWHEELUP, 0);
      else if (event.xbutton.button == 5)
	in_state->Key_Event_fp (K_MWHEELDOWN, 0);
      if (b>=0 && in_state && in_state->Key_Event_fp)
	in_state->Key_Event_fp (K_MOUSE1 + b, false);
      if (b>=0)
	mouse_buttonstate &= ~(1<<b);
      break;
      
    case CreateNotify :
      win_x = event.xcreatewindow.x;
      win_y = event.xcreatewindow.y;
      break;
      
    case ConfigureNotify :
      win_x = event.xconfigure.x;
      win_y = event.xconfigure.y;
      break;
      
    case ClientMessage:
      if (event.xclient.data.l[0] == wmDeleteWindow)
	ri.Cmd_ExecuteText(EXEC_NOW, "quit");
      break;
    }
  }
  
  if (dowarp) {
    /* move the mouse to the window center again */
    XWarpPointer(dpy,None,win,0,0,0,0, vid.width/2,vid.height/2);
  }
}

Key_Event_fp_t Key_Event_fp;

void KBD_Init(Key_Event_fp_t fp)
{
  Key_Event_fp = fp;
}

void KBD_Update(void)
{
  // get events from x server
  HandleEvents();
}

void KBD_Close(void)
{
}

/*****************************************************************************/

char *RW_Sys_GetClipboardData()
{
	Window sowner;
	Atom type, property;
	unsigned long len, bytes_left, tmp;
	unsigned char *data;
	int format, result;
	char *ret = NULL;
			
	sowner = XGetSelectionOwner(dpy, XA_PRIMARY);
			
	if (sowner != None) {
		property = XInternAtom(dpy,
				       "GETCLIPBOARDDATA_PROP",
				       False);
				
		XConvertSelection(dpy,
				  XA_PRIMARY, XA_STRING,
				  property, win, myxtime); /* myxtime == time of last X event */
		XFlush(dpy);

		XGetWindowProperty(dpy,
				   win, property,
				   0, 0, False, AnyPropertyType,
				   &type, &format, &len,
				   &bytes_left, &data);
		if (bytes_left > 0) {
			result =
			XGetWindowProperty(dpy,
					   win, property,
					   0, bytes_left, True, AnyPropertyType,
					   &type, &format, &len,
					   &tmp, &data);
			if (result == Success) {
				ret = strdup(data);
			}
			XFree(data);
		}
	}
	return ret;
}

/*****************************************************************************/

qboolean GLimp_InitGL (void);

static void signal_handler(int sig)
{
	printf("Received signal %d, exiting...\n", sig);
	GLimp_Shutdown();
	_exit(0);
}

static void InitSig(void)
{
	signal(SIGHUP, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGILL, signal_handler);
	signal(SIGTRAP, signal_handler);
	signal(SIGIOT, signal_handler);
	signal(SIGBUS, signal_handler);
	signal(SIGFPE, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGTERM, signal_handler);
}

/*
** GLimp_SetMode
*/
int GLimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen )
{
	int width, height;
	int attrib[] = {
		GLX_RGBA,
		GLX_DOUBLEBUFFER,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DEPTH_SIZE, 1,
		GLX_STENCIL_SIZE, 1,
		None
	};
	int attrib_nostencil[] = {
		GLX_RGBA,
		GLX_DOUBLEBUFFER,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DEPTH_SIZE, 1,
		None
	};

	Window root;
	XVisualInfo *visinfo;
	XSetWindowAttributes attr;
	XSizeHints *sizehints;
	XWMHints *wmhints;
	unsigned long mask;
	int MajorVersion, MinorVersion;
	int actualWidth, actualHeight;
	int i;

	r_fakeFullscreen = ri.Cvar_Get( "r_fakeFullscreen", "0", CVAR_ARCHIVE);

	ri.Con_Printf( PRINT_ALL, "Initializing OpenGL display\n");

	if (fullscreen)
		ri.Con_Printf (PRINT_ALL, "...setting fullscreen mode %d:", mode );
	else
		ri.Con_Printf (PRINT_ALL, "...setting mode %d:", mode );

	if ( !ri.Vid_GetModeInfo( &width, &height, mode ) )
	{
		ri.Con_Printf( PRINT_ALL, " invalid mode\n" );
		return rserr_invalid_mode;
	}

	ri.Con_Printf( PRINT_ALL, " %d %d\n", width, height );

	// destroy the existing window
	GLimp_Shutdown ();

#if 0 // this breaks getenv()? - sbf
	// Mesa VooDoo hacks
	if (fullscreen)
		putenv("MESA_GLX_FX=fullscreen");
	else
		putenv("MESA_GLX_FX=window");
#endif

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "Error couldn't open the X display\n");
		return rserr_invalid_mode;
	}

	scrnum = DefaultScreen(dpy);
	root = RootWindow(dpy, scrnum);

	// Get video mode list
	MajorVersion = MinorVersion = 0;
	if (!XF86VidModeQueryVersion(dpy, &MajorVersion, &MinorVersion)) { 
		vidmode_ext = false;
	} else {
		ri.Con_Printf(PRINT_ALL, "Using XFree86-VidModeExtension Version %d.%d\n",
			MajorVersion, MinorVersion);
		vidmode_ext = true;
	}

	visinfo = qglXChooseVisual(dpy, scrnum, attrib);
	if (!visinfo) {
		fprintf(stderr, "W: couldn't get an RGBA, DOUBLEBUFFER, DEPTH, STENCIL visual\n");
		visinfo = qglXChooseVisual(dpy, scrnum, attrib_nostencil);
		if (!visinfo) {
			fprintf(stderr, "E: couldn't get an RGBA, DOUBLEBUFFER, DEPTH visual\n");
			return rserr_invalid_mode;
		}
	}

	gl_state.hwgamma = false;

	/* do some pantsness */
	if ( qglXGetConfig )
	{
		int red_bits, blue_bits, green_bits, depth_bits, alpha_bits;

		qglXGetConfig(dpy, visinfo, GLX_RED_SIZE, &red_bits);
		qglXGetConfig(dpy, visinfo, GLX_BLUE_SIZE, &blue_bits);
		qglXGetConfig(dpy, visinfo, GLX_GREEN_SIZE, &green_bits);
		qglXGetConfig(dpy, visinfo, GLX_DEPTH_SIZE, &depth_bits);
		qglXGetConfig(dpy, visinfo, GLX_ALPHA_SIZE, &alpha_bits);

		ri.Con_Printf(PRINT_ALL, "I: got %d bits of red\n", red_bits);
		ri.Con_Printf(PRINT_ALL, "I: got %d bits of blue\n", blue_bits);
		ri.Con_Printf(PRINT_ALL, "I: got %d bits of green\n", green_bits);
		ri.Con_Printf(PRINT_ALL, "I: got %d bits of depth\n", depth_bits);
		ri.Con_Printf(PRINT_ALL, "I: got %d bits of alpha\n", alpha_bits);
	}

	/* stencilbuffer shadows */
	if ( qglXGetConfig )
	{
		int stencil_bits;

		if (!qglXGetConfig(dpy, visinfo, GLX_STENCIL_SIZE, &stencil_bits)) {
			ri.Con_Printf(PRINT_ALL, "I: got %d bits of stencil\n", stencil_bits);
			if (stencil_bits >= 1) {
				have_stencil = true;
			}
		}
	} else {
		have_stencil = true;
	}

	if (vidmode_ext) {
		int best_fit, best_dist, dist, x, y;
		
		XF86VidModeGetAllModeLines(dpy, scrnum, &num_vidmodes, &vidmodes);

		// Are we going fullscreen?  If so, let's change video mode
		if (fullscreen && !r_fakeFullscreen->value) {
			best_dist = 9999999;
			best_fit = -1;

			for (i = 0; i < num_vidmodes; i++) {
				if (width > vidmodes[i]->hdisplay ||
					height > vidmodes[i]->vdisplay)
					continue;

				x = width - vidmodes[i]->hdisplay;
				y = height - vidmodes[i]->vdisplay;
				dist = (x * x) + (y * y);
				if (dist < best_dist) {
					best_dist = dist;
					best_fit = i;
				}
			}

			if (best_fit != -1) {
				actualWidth = vidmodes[best_fit]->hdisplay;
				actualHeight = vidmodes[best_fit]->vdisplay;

				// change to the mode
				XF86VidModeSwitchToMode(dpy, scrnum, vidmodes[best_fit]);
				vidmode_active = true;

				if (XF86VidModeGetGamma(dpy, scrnum, &oldgamma)) {
					gl_state.hwgamma = true;
					/* We can not reliably detect hardware gamma
					   changes across software gamma calls, which
					   can reset the flag, so change it anyway */
					vid_gamma->modified = true;
					ri.Con_Printf( PRINT_ALL, "Using hardware gamma\n");
				}

				// Move the viewport to top left
				XF86VidModeSetViewPort(dpy, scrnum, 0, 0);
			} else
				fullscreen = 0;
		}
	}

	/* window attributes */
	attr.background_pixel = 0;
	attr.border_pixel = 0;
	attr.colormap = XCreateColormap(dpy, root, visinfo->visual, AllocNone);
	attr.event_mask = X_MASK;
	if (vidmode_active) {
		mask = CWBackPixel | CWColormap | CWSaveUnder | CWBackingStore | 
			CWEventMask | CWOverrideRedirect;
		attr.override_redirect = True;
		attr.backing_store = NotUseful;
		attr.save_under = False;
	} else
		mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

	win = XCreateWindow(dpy, root, 0, 0, width, height,
						0, visinfo->depth, InputOutput,
						visinfo->visual, mask, &attr);
	
	sizehints = XAllocSizeHints();
	if (sizehints) {
		sizehints->min_width = width;
		sizehints->min_height = height;
		sizehints->max_width = width;
		sizehints->max_height = height;
		sizehints->base_width = width;
		sizehints->base_height = vid.height;
		
		sizehints->flags = PMinSize | PMaxSize | PBaseSize;
	}
	
	wmhints = XAllocWMHints();
	if (wmhints) {
		#include "q2icon.xbm"

		Pixmap icon_pixmap, icon_mask;
		unsigned long fg, bg;
		int i;
		
		fg = BlackPixel(dpy, visinfo->screen);
		bg = WhitePixel(dpy, visinfo->screen);
		icon_pixmap = XCreatePixmapFromBitmapData(dpy, win, (char *)q2icon_bits, q2icon_width, q2icon_height, fg, bg, visinfo->depth);
		for (i = 0; i < sizeof(q2icon_bits); i++)
			q2icon_bits[i] = ~q2icon_bits[i];
		icon_mask = XCreatePixmapFromBitmapData(dpy, win, (char *)q2icon_bits, q2icon_width, q2icon_height, bg, fg, visinfo->depth); 
	
		wmhints->flags = IconPixmapHint|IconMaskHint;
		wmhints->icon_pixmap = icon_pixmap;
		wmhints->icon_mask = icon_mask;
	}

	XSetWMProperties(dpy, win, NULL, NULL, NULL, 0,
			sizehints, wmhints, None);
	if (sizehints)
		XFree(sizehints);
	if (wmhints)
		XFree(wmhints);
	
	XStoreName(dpy, win, "Quake II");
	
	wmDeleteWindow = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, &wmDeleteWindow, 1);
	
	XMapWindow(dpy, win);

	if (vidmode_active) {
		XMoveWindow(dpy, win, 0, 0);
		XRaiseWindow(dpy, win);
		XWarpPointer(dpy, None, win, 0, 0, 0, 0, 0, 0);
		XFlush(dpy);
		// Move the viewport to top left
		XF86VidModeSetViewPort(dpy, scrnum, 0, 0);
	}

	XFlush(dpy);

	ctx = qglXCreateContext(dpy, visinfo, NULL, True);

	qglXMakeCurrent(dpy, win, ctx);

	*pwidth = width;
	*pheight = height;

	// let the sound and input subsystems know about the new window
	ri.Vid_NewWindow (width, height);

	qglXMakeCurrent(dpy, win, ctx);

	return rserr_ok;
}

/*
** GLimp_Shutdown
**
** This routine does all OS specific shutdown procedures for the OpenGL
** subsystem.  Under OpenGL this means NULLing out the current DC and
** HGLRC, deleting the rendering context, and releasing the DC acquired
** for the window.  The state structure is also nulled out.
**
*/
void GLimp_Shutdown( void )
{
	uninstall_grabs();
	mouse_active = false;
	dgamouse = false;

	if (dpy) {
		if (ctx)
			qglXDestroyContext(dpy, ctx);
		if (win)
			XDestroyWindow(dpy, win);
		if (gl_state.hwgamma) {
			XF86VidModeSetGamma(dpy, scrnum, &oldgamma);
			/* The gamma has changed, but SetMode will change it
			   anyway, so why bother?
			vid_gamma->modified = true; */
		}
		if (vidmode_active)
			XF86VidModeSwitchToMode(dpy, scrnum, vidmodes[0]);
		XUngrabKeyboard(dpy, CurrentTime);
		XCloseDisplay(dpy);
	}
	ctx = NULL;
	dpy = NULL;
	win = 0;
	ctx = NULL;
/*	
	qglXChooseVisual             = NULL;
	qglXCreateContext            = NULL;
	qglXDestroyContext           = NULL;
	qglXMakeCurrent              = NULL;
	qglXCopyContext              = NULL;
	qglXSwapBuffers              = NULL;
*/	
}

/*
** GLimp_Init
**
** This routine is responsible for initializing the OS specific portions
** of OpenGL.  
*/
int GLimp_Init( void *hinstance, void *wndproc )
{
	InitSig();

	if ( glw_state.OpenGLLib) {
		#define GPA( a ) dlsym( glw_state.OpenGLLib, a )

		qglXChooseVisual             =  GPA("glXChooseVisual");
		qglXCreateContext            =  GPA("glXCreateContext");
		qglXDestroyContext           =  GPA("glXDestroyContext");
		qglXMakeCurrent              =  GPA("glXMakeCurrent");
		qglXCopyContext              =  GPA("glXCopyContext");
		qglXSwapBuffers              =  GPA("glXSwapBuffers");
		qglXGetConfig                =  GPA("glXGetConfig");
		
		return true;
	}
	
	return false;
}

/*
** GLimp_BeginFrame
*/
void GLimp_BeginFrame( float camera_seperation )
{
}

/*
** GLimp_EndFrame
** 
** Responsible for doing a swapbuffers and possibly for other stuff
** as yet to be determined.  Probably better not to make this a GLimp
** function and instead do a call to GLimp_SwapBuffers.
*/
void GLimp_EndFrame (void)
{
	qglFlush();
	qglXSwapBuffers(dpy, win);
}

/*
** UpdateHardwareGamma
**
** We are using gamma relative to the desktop, so that we can share it
** with software renderer and don't require to change desktop gamma
** to match hardware gamma image brightness. It seems that Quake 3 is
** using the opposite approach, but it has no software renderer after
** all.
*/
void UpdateHardwareGamma()
{
	XF86VidModeGamma gamma;
	float g;

	g = (1.3 - vid_gamma->value + 1);
	g = (g>1 ? g : 1);
	gamma.red = oldgamma.red * g;
	gamma.green = oldgamma.green * g;
	gamma.blue = oldgamma.blue * g;
	XF86VidModeSetGamma(dpy, scrnum, &gamma);
}

/*
** GLimp_AppActivate
*/
void GLimp_AppActivate( qboolean active )
{
}

void Fake_glColorTableEXT( GLenum target, GLenum internalformat,
                             GLsizei width, GLenum format, GLenum type,
                             const GLvoid *table )
{
	byte temptable[256][4];
	byte *intbl;
	int i;

	for (intbl = (byte *)table, i = 0; i < 256; i++) {
		temptable[i][2] = *intbl++;
		temptable[i][1] = *intbl++;
		temptable[i][0] = *intbl++;
		temptable[i][3] = 255;
	}
	qgl3DfxSetPaletteEXT((GLuint *)temptable);
}


#ifdef Joystick
qboolean OpenJoystick(cvar_t *joy_dev) {
  int i, err;
  glob_t pglob;
  struct js_event e;

  err = glob(joy_dev->string, 0, NULL, &pglob);

  if (err) {
    switch (err) {
    case GLOB_NOSPACE:
      ri.Con_Printf(PRINT_ALL, "Error, out of memory while looking for joysticks\n");
      break;
    case GLOB_NOMATCH:
      ri.Con_Printf(PRINT_ALL, "No joysticks found\n");
      break;
    default:
      ri.Con_Printf(PRINT_ALL, "Error #%d while looking for joysticks\n",err);
    }
    return false;
  }  
  
  for (i=0;i<pglob.gl_pathc;i++) {
    ri.Con_Printf(PRINT_ALL, "Trying joystick dev %s\n", pglob.gl_pathv[i]);
    joy_fd = open (pglob.gl_pathv[i], O_RDONLY | O_NONBLOCK);
    if (joy_fd == -1) {
      ri.Con_Printf(PRINT_ALL, "Error opening joystick dev %s\n", 
		    pglob.gl_pathv[i]);
      return false;
    }
    else {
      while (read(joy_fd, &e, sizeof(struct js_event))!=-1 &&
	     (e.type & JS_EVENT_INIT))
	ri.Con_Printf(PRINT_ALL, "Read init event\n");
      ri.Con_Printf(PRINT_ALL, "Using joystick dev %s\n", pglob.gl_pathv[i]);
      return true;
    }
  }
  globfree(&pglob);
  return false;
}

void PlatformJoyCommands(int *axis_vals, int *axis_map) {
  struct js_event e;
  int key_index;
  in_state_t *in_state = getState();
  
  while (read(joy_fd, &e, sizeof(struct js_event))!=-1) {
    if (JS_EVENT_BUTTON & e.type) {
      key_index = (e.number < 4) ? K_JOY1 : K_AUX1;
      if (e.value) {
	in_state->Key_Event_fp (key_index + e.number, true);
      }
      else {
	in_state->Key_Event_fp (key_index + e.number, false);
      }
    }
    else if (JS_EVENT_AXIS & e.type) {
      axis_vals[axis_map[e.number]] = e.value;
    }
  }
}

qboolean CloseJoystick(void) {
  if (close(joy_fd))
    ri.Con_Printf(PRINT_ALL, "Error, Problem closing joystick.");
  return true;
}
#endif

/* keyevents.c

Copyright (C) 1999-2003 Tom Gilbert.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies of the Software and its documentation and acknowledgment shall be
given in the documentation and software packages that this Software was
used.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "feh.h"
#include "thumbnail.h"
#include "filelist.h"
#include "winwidget.h"
#include "options.h"

#define XK_Dollar              0x24        /* $ */
#define XK_Dead_Circumflex     0xfe52      /* shift+^+<ANY KEY> */
#define XK_Circumflex          0x5e        /* shift+^ */

void feh_event_invoke_action(winwidget winwid, char *action)
{
	D(4, ("action is '%s'\n", action));
	D(4, ("winwid is '%p'\n", winwid));
	if (action) {
		if (opt.slideshow) {
			feh_action_run(FEH_FILE(winwid->file->data), action);
			slideshow_change_image(winwid, SLIDE_NEXT);
		} else if ((winwid->type == WIN_TYPE_SINGLE)
				|| (winwid->type == WIN_TYPE_THUMBNAIL_VIEWER)) {
			feh_action_run(FEH_FILE(winwid->file->data), action);
			winwidget_destroy(winwid);
		} else if (winwid->type == WIN_TYPE_THUMBNAIL) {
			printf("actions from the main thumb window aren't currentl supported!\n");
			printf("For now, open the image to perform the action on it.\n");
		}
	}
	return;
}

void feh_event_handle_keypress(XEvent * ev)
{
	int len;
	char kbuf[20];
	KeySym keysym;
	XKeyEvent *kev;
	winwidget winwid = NULL;
	int curr_screen = 0;
	feh_menu_item *selected_item;
	feh_menu *selected_menu;

	winwid = winwidget_get_from_window(ev->xkey.window);

	/* nuke dupe events, unless we're typing text */
	if (winwid && !winwid->caption_entry) {
		while (XCheckTypedWindowEvent(disp, ev->xkey.window, KeyPress, ev));
	}

	kev = (XKeyEvent *) ev;
	len = XLookupString(&ev->xkey, (char *) kbuf, sizeof(kbuf), &keysym, NULL);

	/* menus are showing, so this is a menu control keypress */
	if (ev->xbutton.window == menu_cover) {
		selected_item = feh_menu_find_selected_r(menu_root, &selected_menu);
		switch (keysym) {
		case XK_Escape:
			feh_menu_hide(menu_root, True);
			break;
    case XK_h:
			feh_menu_select_parent(selected_menu);
			break;
    case XK_j:
			feh_menu_select_next(selected_menu, selected_item);
			break;
		case XK_k:
			feh_menu_select_prev(selected_menu, selected_item);
			break;
		case XK_l:
			feh_menu_select_submenu(selected_menu);
			break;
		case XK_Return:
		case XK_space:
			feh_menu_item_activate(selected_menu, selected_item);
			break;
		default:
			break;
		}

		return;
	}

	if (winwid == NULL)
		return;

	if (winwid->caption_entry) {
		switch (keysym) {
		case XK_Return:
			if (kev->state & ControlMask) {
				/* insert actual newline */
				ESTRAPPEND(FEH_FILE(winwid->file->data)->caption, "\n");
				winwidget_render_image_cached(winwid);
			} else {
				/* finish caption entry, write to captions file */
				FILE *fp;
				char *caption_filename;
				caption_filename = build_caption_filename(FEH_FILE(winwid->file->data));
				winwid->caption_entry = 0;
				winwidget_render_image_cached(winwid);
				XFreePixmap(disp, winwid->bg_pmap_cache);
				winwid->bg_pmap_cache = 0;
				fp = fopen(caption_filename, "w");
				if (!fp) {
					weprintf("couldn't write to captions file %s:", caption_filename);
					return;
				}
				fprintf(fp, "%s", FEH_FILE(winwid->file->data)->caption);
				free(caption_filename);
				fclose(fp);
			}
			break;
		case XK_Escape:
			/* cancel, revert caption */
			winwid->caption_entry = 0;
			free(FEH_FILE(winwid->file->data)->caption);
			FEH_FILE(winwid->file->data)->caption = NULL;
			winwidget_render_image_cached(winwid);
			XFreePixmap(disp, winwid->bg_pmap_cache);
			winwid->bg_pmap_cache = 0;
			break;
		case XK_BackSpace:
			/* backspace */
			ESTRTRUNC(FEH_FILE(winwid->file->data)->caption, 1);
			winwidget_render_image_cached(winwid);
			break;
		default:
			if (isascii(keysym)) {
				/* append to caption */
				ESTRAPPEND_CHAR(FEH_FILE(winwid->file->data)->caption, keysym);
				winwidget_render_image_cached(winwid);
			}
			break;
		}
		return;
	}

	switch (keysym) {
	case XK_h:
		if (opt.slideshow)
			slideshow_change_image(winwid, SLIDE_PREV);
		break;
	case XK_l:
		if (opt.slideshow)
			slideshow_change_image(winwid, SLIDE_NEXT);
		break;
	case XK_H:
		if (opt.slideshow)
			slideshow_change_image(winwid, SLIDE_JUMP_BACK);
		break;
	case XK_Escape:
		winwidget_destroy_all();
		break;
	case XK_L:
		if (opt.slideshow)
			slideshow_change_image(winwid, SLIDE_JUMP_FWD);
		break;
	case XK_d:
		/* Holding ctrl gets you a filesystem deletion and removal from the * 
		   filelist. Just 'd' gets you filelist removal only. */
		if (kev->state & ControlMask) {
			if (winwid->type == WIN_TYPE_THUMBNAIL_VIEWER)
				feh_thumbnail_mark_removed(FEH_FILE(winwid->file->data), 1);
			feh_filelist_image_remove(winwid, 1);
		} else {
			if (winwid->type == WIN_TYPE_THUMBNAIL_VIEWER)
				feh_thumbnail_mark_removed(FEH_FILE(winwid->file->data), 0);
			feh_filelist_image_remove(winwid, 0);
		}
		break;
	case XK_Circumflex:
		if (opt.slideshow)
			slideshow_change_image(winwid, SLIDE_FIRST);
		break;
	case XK_Dollar:
		if (opt.slideshow)
			slideshow_change_image(winwid, SLIDE_LAST);
		break;
	case XK_Return:
	case XK_KP_Enter:
	case XK_0:
	case XK_KP_0:
		feh_event_invoke_action(winwid, opt.actions[0]);
		break;
	case XK_1:
	case XK_KP_1:
		feh_event_invoke_action(winwid, opt.actions[1]);
		break;
	case XK_2:
	case XK_KP_2:
		feh_event_invoke_action(winwid, opt.actions[2]);
		break;
	case XK_3:
	case XK_KP_3:
		feh_event_invoke_action(winwid, opt.actions[3]);
		break;
	case XK_4:
	case XK_KP_4:
		feh_event_invoke_action(winwid, opt.actions[4]);
		break;
	case XK_5:
	case XK_KP_5:
		feh_event_invoke_action(winwid, opt.actions[5]);
		break;
	case XK_6:
	case XK_KP_6:
		feh_event_invoke_action(winwid, opt.actions[6]);
		break;
	case XK_7:
	case XK_KP_7:
		feh_event_invoke_action(winwid, opt.actions[7]);
		break;
	case XK_8:
	case XK_KP_8:
		feh_event_invoke_action(winwid, opt.actions[8]);
		break;
	case XK_9:
	case XK_KP_9:
		feh_event_invoke_action(winwid, opt.actions[9]);
		break;
	case XK_KP_Left:
		winwid->im_x = winwid->im_x - 10;
		winwidget_render_image(winwid, 0, 0);
		break;
	case XK_KP_Right:
		winwid->im_x = winwid->im_x + 10;
		winwidget_render_image(winwid, 0, 0);
		break;
	case XK_KP_Up:
		winwid->im_y = winwid->im_y - 10;
		winwidget_render_image(winwid, 0, 0);
		break;
	case XK_KP_Down:
		winwid->im_y = winwid->im_y + 10;
		winwidget_render_image(winwid, 0, 0);
		break;
	case XK_Z:
		len = 0;
		winwid->zoom = winwid->zoom * 1.50;
		winwidget_render_image(winwid, 0, 0);
		break;
	case XK_z:
		len = 0;
		winwid->zoom = winwid->zoom * 0.50;
		winwidget_render_image(winwid, 0, 0);
		break;
	case XK_KP_Multiply:
		len = 0;
		winwid->zoom = 1;
		winwidget_center_image(winwid);
		winwidget_render_image(winwid, 0, 0);
		break;
	case XK_KP_Divide:
		len = 0;
		feh_calc_needed_zoom(&winwid->zoom, winwid->im_w, winwid->im_h, winwid->w, winwid->h);
		winwidget_center_image(winwid);
		winwidget_render_image(winwid, 0, 1);
		break;
	case XK_KP_Begin:
		winwidget_render_image(winwid, 0, 1);
		break;
	default:
		break;
	}

	if (len <= 0 || len > (int) sizeof(kbuf))
		return;
	kbuf[len] = '\0';

	switch (*kbuf) {
	case 'a':
		opt.draw_actions = !opt.draw_actions;
		winwidget_rerender_all(0, 1);
		break;
	case 'd':
		opt.draw_filename = !opt.draw_filename;
		winwidget_rerender_all(0, 1);
		break;
	case 'n':
	case ' ':
		if (opt.slideshow)
			slideshow_change_image(winwid, SLIDE_NEXT);
		break;
	case 'o':
		winwidget_set_pointer(winwid, opt.hide_pointer);
		opt.hide_pointer = !opt.hide_pointer;
		break;
	case 'p':
	case '\b':
		if (opt.slideshow)
			slideshow_change_image(winwid, SLIDE_PREV);
		break;
	case 'z':
		if (opt.slideshow)
			slideshow_change_image(winwid, SLIDE_RAND);
		break;
	case 'q':
		winwidget_destroy_all();
		break;
	case 'c':
		if (opt.caption_path)
			winwid->caption_entry = 1;
		winwidget_render_image(winwid, 0, 1);
		break;
	case 'r':
		feh_reload_image(winwid, 0, 0);
		break;
	case 'h':
		slideshow_pause_toggle(winwid);
		break;
	case 's':
		slideshow_save_image(winwid);
		break;
	case 'f':
		feh_save_filelist();
		break;
	case 'w':
		winwidget_size_to_image(winwid);
		break;
	case 'm':
		winwidget_show_menu(winwid);
		break;
	case 'x':
		winwidget_destroy(winwid);
		break;
	case '>':
		feh_edit_inplace_orient(winwid, 1);
		break;
	case '<':
		feh_edit_inplace_orient(winwid, 3);
		break;
	case 'v':
#ifdef HAVE_LIBXINERAMA
		if (opt.xinerama && xinerama_screens) {
			int i, rect[4];

			winwidget_get_geometry(winwid, rect);
			/* printf("window: (%d, %d)\n", rect[0], rect[1]);
			   printf("found %d screens.\n", num_xinerama_screens); */
			for (i = 0; i < num_xinerama_screens; i++) {
				xinerama_screen = 0;
				/* printf("%d: [%d, %d, %d, %d] (%d, %d)\n",
				   i,
				   xinerama_screens[i].x_org, xinerama_screens[i].y_org,
				   xinerama_screens[i].width, xinerama_screens[i].height,
				   rect[0], rect[1]); */
				if (XY_IN_RECT(rect[0], rect[1],
							xinerama_screens[i].x_org,
							xinerama_screens[i].y_org,
							xinerama_screens[i].width,
							xinerama_screens[i].height)) {
					curr_screen = xinerama_screen = i;
					break;
				}
			}
		}
#endif				/* HAVE_LIBXINERAMA */
		winwid->full_screen = !winwid->full_screen;
		winwidget_destroy_xwin(winwid);
		winwidget_create_window(winwid, winwid->im_w, winwid->im_h);
		winwidget_render_image(winwid, 1, 1);
		winwidget_show(winwid);
#ifdef HAVE_LIBXINERAMA
		/* if we have xinerama and we're using it, then full screen the window
		 * on the head that the window was active on */
		if (winwid->full_screen == TRUE && opt.xinerama && xinerama_screens) {
			xinerama_screen = curr_screen;
			winwidget_move(winwid,
					xinerama_screens[curr_screen].x_org, xinerama_screens[curr_screen].y_org);
		}
#endif				/* HAVE_LIBXINERAMA */
	case '+':
		if (opt.reload < SLIDESHOW_RELOAD_MAX)
			opt.reload++;
		else if (opt.verbose)
			weprintf("Cannot set RELOAD higher than %d seconds.", opt.reload);
		break;
	case '-':
		if (opt.reload > 1)
			opt.reload--;
		else if (opt.verbose)
			weprintf("Cannot set RELOAD lower than 1 second.");
		break;
	default:
		break;
	}
	return;
}

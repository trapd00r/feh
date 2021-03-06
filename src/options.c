/* options.c

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
#include "filelist.h"
#include "options.h"

static void check_options(void);
static void feh_create_default_config(char *rcfile);
static void feh_parse_option_array(int argc, char **argv);
static void feh_parse_environment_options(void);
static void feh_check_theme_options(int arg, char **argv);
static void feh_parse_options_from_string(char *opts);
static void feh_load_options_for_theme(char *theme);
static void show_usage(void);
static void show_version(void);
static char *theme;

fehoptions opt;

void init_parse_options(int argc, char **argv)
{
	/* For setting the command hint on X windows */
	cmdargc = argc;
	cmdargv = argv;

	/* Set default options */
	memset(&opt, 0, sizeof(fehoptions));
	opt.display = 1;
	opt.aspect = 1;
	opt.slideshow_delay = 0.0;
	opt.thumb_w = 60;
	opt.thumb_h = 60;
	opt.thumb_redraw = 10;
	opt.menu_font = estrdup(DEFAULT_MENU_FONT);
	opt.font = NULL;
	opt.image_bg = estrdup("default");
	opt.menu_bg = estrdup(PREFIX "/share/feh/images/menubg_default.png");
	opt.menu_style = estrdup(PREFIX "/share/feh/fonts/menu.style");
	opt.menu_border = 4;

	opt.reload_button = 0;
	opt.pan_button = 1;
	opt.zoom_button = 2;
	opt.menu_button = 3;
	opt.menu_ctrl_mask = 0;
	opt.prev_button = 4;
	opt.next_button = 5;

	opt.rotate_button = 2;
	opt.no_rotate_ctrl_mask = 0;
	opt.blur_button = 1;
	opt.no_blur_ctrl_mask = 0;

	opt.start_list_at = NULL;
	opt.no_jump_on_resort = 0;

	opt.builtin_http = 0;

	opt.xinerama = 0;
	opt.screen_clip = 1;
#ifdef HAVE_LIBXINERAMA
	/* if we're using xinerama, then enable it by default */
	opt.xinerama = 1;
#endif				/* HAVE_LIBXINERAMA */

	D(3, ("About to parse env options (if any)\n"));
	/* Check for and parse any options in FEH_OPTIONS */
	feh_parse_environment_options();

	D(3, ("About to parse commandline options\n"));
	/* Parse the cmdline args */
	feh_parse_option_array(argc, argv);

	D(3, ("About to check for theme configuration\n"));
	feh_check_theme_options(argc, argv);

	/* If we have a filelist to read, do it now */
	if (opt.filelistfile) {
		/* joining two reverse-sorted lists in this manner works nicely for us
		   here, as files specified on the commandline end up at the *end* of
		   the combined filelist, in the specified order. */
		D(3, ("About to load filelist from file\n"));
		filelist = gib_list_cat(filelist, feh_read_filelist(opt.filelistfile));
	}

	D(4, ("Options parsed\n"));

	if (opt.bgmode)
		return;

	filelist_len = gib_list_length(filelist);
	if (!filelist_len)
		show_mini_usage();

	check_options();

	feh_prepare_filelist();
	return;
}

static void feh_check_theme_options(int arg, char **argv)
{
	if (!theme) {
		/* This prevents screw up when running src/feh or ./feh */
		char *pos = strrchr(argv[0], '/');

		if (pos)
			theme = estrdup(pos + 1);
		else
			theme = estrdup(argv[0]);
	}
	D(3, ("Theme name is %s\n", theme));

	feh_load_options_for_theme(theme);

	free(theme);
	return;
	arg = 0;
}

static void feh_load_options_for_theme(char *theme)
{
	FILE *fp = NULL;
	char *home;
	char *rcpath = NULL;
	char s[1024], s1[1024], s2[1024];
	int cont = 0;
	int bspos;

	if (opt.rcfile) {
		if ((fp = fopen(opt.rcfile, "r")) == NULL) {
			weprintf("couldn't load the specified rcfile %s\n", opt.rcfile);
			return;
		}
	} else {
		home = getenv("HOME");
		if (!home)
			eprintf("D'oh! Please define HOME in your environment! "
					"It would really help me out...\n");
		rcpath = estrjoin("/", home, ".fehrc", NULL);
		D(3, ("Trying %s for config\n", rcpath));
		fp = fopen(rcpath, "r");

		if (!fp && ((fp = fopen("/etc/fehrc", "r")) == NULL)) {
			feh_create_default_config(rcpath);

			if ((fp = fopen(rcpath, "r")) == NULL)
				return;
		}

		free(rcpath);
	}

	/* Oooh. We have an options file :) */
	for (; fgets(s, sizeof(s), fp);) {
		s1[0] = '\0';
		s2[0] = '\0';

		if (cont) {
			sscanf(s, " %[^\n]\n", (char *) &s2);
			if (!*s2)
				break;
			D(5, ("Got continued options %s\n", s2));
		} else {
			sscanf(s, "%s %[^\n]\n", (char *) &s1, (char *) &s2);
			if (!(*s1) || (!*s2) || (*s1 == '\n') || (*s1 == '#')) {
				cont = 0;
				continue;
			}
			D(5, ("Got theme/options pair %s/%s\n", s1, s2));
		}

		if (!strcmp(s1, theme) || cont) {

			bspos = strlen(s2)-1;

			if (s2[bspos] == '\\') {
				D(5, ("Continued line\n"));
				s2[bspos] = '\0';
				cont = 1;
				/* A trailing whitespace confuses the option parser */
				if (bspos && (s2[bspos-1] == ' '))
					s2[bspos-1] = '\0';
			} else
				cont = 0;

			D(4, ("A match. Using options %s\n", s2));
			feh_parse_options_from_string(s2);

			if (!cont)
				break;
		}
	}
	fclose(fp);
	return;
}

static void feh_parse_environment_options(void)
{
	char *opts;

	if ((opts = getenv("FEH_OPTIONS")) == NULL)
		return;

	weprintf
	    ("The FEH_OPTIONS configuration method is depreciated and will soon die.\n"
	     "Use the .fehrc configuration file instead.");

	/* We definitely have some options to parse */
	feh_parse_options_from_string(opts);
	return;
}

/* FIXME This function is a crufty bitch ;) */
static void feh_parse_options_from_string(char *opts)
{
	char **list = NULL;
	int num = 0;
	char *s;
	char *t;
	char last = 0;
	int inquote = 0;
	int i = 0;

	/* So we don't reinvent the wheel (not again, anyway), we use the
	   getopt_long function to do this parsing as well. This means it has to
	   look like the real argv ;) */

	list = malloc(sizeof(char *));

	list[num++] = estrdup(PACKAGE);

	for (s = opts, t = opts;; t++) {
		if ((*t == ' ') && !(inquote)) {
			*t = '\0';
			num++;
			list = erealloc(list, sizeof(char *) * num);

			list[num - 1] = feh_string_normalize(s);
			s = t + 1;
		} else if (*t == '\0') {
			num++;
			list = erealloc(list, sizeof(char *) * num);

			list[num - 1] = feh_string_normalize(s);
			break;
		} else if (*t == '\"' && last != '\\')
			inquote = !(inquote);
		last = *t;
	}

	feh_parse_option_array(num, list);

	for (i = 0; i < num; i++)
		if (list[i])
			free(list[i]);
	if (list)
		free(list);
	return;
}

char *feh_string_normalize(char *str)
{
	char ret[4096];
	char *s;
	int i = 0;
	char last = 0;

	D(4, ("normalizing %s\n", str));
	ret[0] = '\0';

	for (s = str;; s++) {
		if (*s == '\0')
			break;
		else if ((*s == '\"') && (last == '\\'))
			ret[i++] = '\"';
		else if ((*s == '\"') && (last == 0));
		else if ((*s == ' ') && (last == '\\'))
			ret[i++] = ' ';
		else
			ret[i++] = *s;

		last = *s;
	}
	if (i && ret[i - 1] == '\"')
		ret[i - 1] = '\0';
	else
		ret[i] = '\0';
	D(4, ("normalized to %s\n", ret));

	return(estrdup(ret));
}

static void feh_parse_option_array(int argc, char **argv)
{
	static char stropts[] =
	    "a:A:b:B:cC:dD:e:E:f:Fg:GhH:iIj:klL:mM:nNo:O:pqQrR:sS:tT:uUvVwW:xXy:zZ0:1:2:4:5:8:9:.@:^:~:):|:_:+:";
	static struct option lopts[] = {
		/* actions */
		{"help", 0, 0, 'h'},	/* okay */
		{"version", 0, 0, 'v'},	/* okay */
		/* toggles */
		{"montage", 0, 0, 'm'},	/* okay */
		{"collage", 0, 0, 'c'},	/* okay */
		{"index", 0, 0, 'i'},	/* okay */
		{"fullindex", 0, 0, 'I'},	/* okay */
		{"verbose", 0, 0, 'V'},	/* okay */
		{"borderless", 0, 0, 'x'},	/* okay */
		{"keep-http", 0, 0, 'k'},	/* okay */
		{"stretch", 0, 0, 's'},	/* okay */
		{"multiwindow", 0, 0, 'w'},	/* okay */
		{"recursive", 0, 0, 'r'},	/* okay */
		{"randomize", 0, 0, 'z'},	/* okay */
		{"list", 0, 0, 'l'},	/* okay */
		{"quiet", 0, 0, 'q'},	/* okay */
		{"loadable", 0, 0, 'U'},	/* okay */
		{"unloadable", 0, 0, 'u'},	/* okay */
		{"no-menus", 0, 0, 'N'},
		{"full-screen", 0, 0, 'F'},
		{"auto-zoom", 0, 0, 'Z'},
		{"ignore-aspect", 0, 0, 'X'},
		{"draw-filename", 0, 0, 'd'},
		{"preload", 0, 0, 'p'},
		{"reverse", 0, 0, 'n'},
		{"thumbnails", 0, 0, 't'},
		{"wget-timestamp", 0, 0, 'G'},
		{"builtin", 0, 0, 'Q'},
		{"scale-down", 0, 0, '.'},	/* okay */
		{"no-jump-on-resort", 0, 0, 220},
		{"hide-pointer", 0, 0, 221},
		{"draw-actions", 0, 0, 222},
		{"cache-thumbnails", 0, 0, 223},
		{"cycle-once", 0, 0, 224},
		{"no-xinerama", 0, 0, 225},
		{"no-rotate-ctrl-mask", 0, 0, 226},
		{"no-blur-ctrl-mask", 0, 0, 227},
		{"menu-ctrl-mask", 0, 0, 228},	/* okay */
		/* options with values */
		{"output", 1, 0, 'o'},	/* okay */
		{"output-only", 1, 0, 'O'},	/* okay */
		{"action", 1, 0, 'A'},	/* okay */
		{"limit-width", 1, 0, 'W'},	/* okay */
		{"limit-height", 1, 0, 'H'},	/* okay */
		{"reload", 1, 0, 'R'},	/* okay */
		{"alpha", 1, 0, 'a'},	/* okay */
		{"sort", 1, 0, 'S'},	/* okay */
		{"theme", 1, 0, 'T'},	/* okay */
		{"filelist", 1, 0, 'f'},	/* okay */
		{"customlist", 1, 0, 'L'},	/* okay */
		{"geometry", 1, 0, 'g'},	/* okay */
		{"menu-font", 1, 0, 'M'},
		{"thumb-width", 1, 0, 'y'},
		{"thumb-height", 1, 0, 'E'},
		{"slideshow-delay", 1, 0, 'D'},
		{"font", 1, 0, 'e'},
		{"title-font", 1, 0, '@'},
		{"title", 1, 0, '^'},
		{"thumb-title", 1, 0, '~'},
		{"bg", 1, 0, 'b'},
		{"fontpath", 1, 0, 'C'},
		{"menu-bg", 1, 0, ')'},
		{"image-bg", 1, 0, 'B'},
		{"reload-button", 1, 0, '0'},
		{"pan-button", 1, 0, '1'},
		{"zoom-button", 1, 0, '2'},
		{"menu-button", 1, 0, '3'},
		{"prev-button", 1, 0, '4'},
		{"next-button", 1, 0, '5'},
		{"rotate-button", 1, 0, '8'},
		{"blur-button", 1, 0, '9'},
		{"start-at", 1, 0, '|'},
		{"rcfile", 1, 0, '_'},
		{"debug-level", 1, 0, '+'},
		{"output-dir", 1, 0, 'j'},
		{"bg-tile", 1, 0, 200},
		{"bg-center", 1, 0, 201},
		{"bg-scale", 1, 0, 202},
		{"bg-seamless", 1, 0, 203},
		{"menu-style", 1, 0, 204},
		{"zoom", 1, 0, 205},
		{"no-screen-clip", 0, 0, 206},
		{"menu-border", 1, 0, 207},
		{"caption-path", 1, 0, 208},
		{"action1", 1, 0, 209},
		{"action2", 1, 0, 210},
		{"action3", 1, 0, 211},
		{"action4", 1, 0, 212},
		{"action5", 1, 0, 213},
		{"action6", 1, 0, 214},
		{"action7", 1, 0, 215},
		{"action8", 1, 0, 216},
		{"action9", 1, 0, 217},
		{"bg-fill", 1, 0, 218},
		{"index-name", 1, 0, 230},
		{"index-size", 1, 0, 231},
		{"index-dim", 1, 0, 232},
		{"thumb-redraw", 1, 0, 233},
		{0, 0, 0, 0}
	};
	int optch = 0, cmdx = 0;

	/* Now to pass some optionarinos */
	while ((optch = getopt_long(argc, argv, stropts, lopts, &cmdx)) != EOF) {
		D(5, ("Got option, getopt calls it %d, or %c\n", optch, optch));
		switch (optch) {
		case 0:
			break;
		case 'h':
			show_usage();
			break;
		case 'v':
			show_version();
			break;
		case 'm':
			opt.index = 1;
			opt.index_show_name = 0;
			opt.index_show_size = 0;
			opt.index_show_dim = 0;
			break;
		case 'c':
			opt.collage = 1;
			break;
		case 'i':
			opt.index = 1;
			opt.index_show_name = 1;
			opt.index_show_size = 0;
			opt.index_show_dim = 0;
			break;
		case '.':
			opt.scale_down = 1;
			break;
		case 'I':
			opt.index = 1;
			opt.index_show_name = 1;
			opt.index_show_size = 1;
			opt.index_show_dim = 1;
			break;
		case 'l':
			opt.list = 1;
			opt.display = 0;
			break;
		case 'G':
			opt.wget_timestamp = 1;
			break;
		case 'Q':
			opt.builtin_http = 1;
			break;
		case 'L':
			opt.customlist = estrdup(optarg);
			opt.display = 0;
			break;
		case 'M':
			free(opt.menu_font);
			opt.menu_font = estrdup(optarg);
			break;
		case '+':
			opt.debug_level = atoi(optarg);
			break;
		case 'n':
			opt.reverse = 1;
			break;
		case 'g':
			opt.geom_flags = XParseGeometry(optarg, &opt.geom_x, &opt.geom_y, &opt.geom_w, &opt.geom_h);
			break;
		case 'N':
			opt.no_menus = 1;
			break;
		case 'V':
			opt.verbose = 1;
			break;
		case 'q':
			opt.quiet = 1;
			break;
		case 'x':
			opt.borderless = 1;
			break;
		case 'k':
			opt.keep_http = 1;
			break;
		case 's':
			opt.stretch = 1;
			break;
		case 'w':
			opt.multiwindow = 1;
			break;
		case 'r':
			opt.recursive = 1;
			break;
		case 'z':
			opt.randomize = 1;
			break;
		case 'd':
			opt.draw_filename = 1;
			break;
		case 'F':
			opt.full_screen = 1;
			break;
		case 'Z':
			opt.auto_zoom = 1;
			break;
		case 'U':
			opt.loadables = 1;
			opt.display = 0;
			break;
		case 'u':
			opt.unloadables = 1;
			opt.display = 0;
			break;
		case 'p':
			opt.preload = 1;
			break;
		case 'X':
			opt.aspect = 0;
			break;
		case 'S':
			if (!strcasecmp(optarg, "name"))
				opt.sort = SORT_NAME;
			else if (!strcasecmp(optarg, "filename"))
				opt.sort = SORT_FILENAME;
			else if (!strcasecmp(optarg, "width"))
				opt.sort = SORT_WIDTH;
			else if (!strcasecmp(optarg, "height"))
				opt.sort = SORT_HEIGHT;
			else if (!strcasecmp(optarg, "pixels"))
				opt.sort = SORT_PIXELS;
			else if (!strcasecmp(optarg, "size"))
				opt.sort = SORT_SIZE;
			else if (!strcasecmp(optarg, "format"))
				opt.sort = SORT_FORMAT;
			else {
				weprintf("Unrecognised sort mode \"%s\". Defaulting to "
						"sort by filename", optarg);
				opt.sort = SORT_FILENAME;
			}
			break;
		case 'o':
			opt.output = 1;
			opt.output_file = estrdup(optarg);
			break;
		case 'O':
			opt.output = 1;
			opt.output_file = estrdup(optarg);
			opt.display = 0;
			break;
		case 'T':
			theme = estrdup(optarg);
			break;
		case 'C':
			D(3, ("adding fontpath %s\n", optarg));
			imlib_add_path_to_font_path(optarg);
			break;
		case 'e':
			opt.font = estrdup(optarg);
			break;
		case '@':
			opt.title_font = estrdup(optarg);
			break;
		case '^':
			opt.title = estrdup(optarg);
			break;
		case '~':
			opt.thumb_title = estrdup(optarg);
			break;
		case 'b':
			opt.bg = 1;
			opt.bg_file = estrdup(optarg);
			break;
		case '_':
			opt.rcfile = estrdup(optarg);
			break;
		case 'A':
			opt.actions[0] = estrdup(optarg);
			break;
		case 'W':
			opt.limit_w = atoi(optarg);
			break;
		case 'H':
			opt.limit_h = atoi(optarg);
			break;
		case 'y':
			opt.thumb_w = atoi(optarg);
			break;
		case 'E':
			opt.thumb_h = atoi(optarg);
			break;
		case ')':
			free(opt.menu_bg);
			opt.menu_bg = estrdup(optarg);
			break;
		case 'B':
			free(opt.image_bg);
			opt.image_bg = estrdup(optarg);
			break;
		case 'D':
			opt.slideshow_delay = atof(optarg);
			if (opt.slideshow_delay < 0.0) {
				opt.slideshow_delay *= (-1);
				opt.paused = 1;
			}
			break;
		case 'R':
			opt.reload = atoi(optarg);
			break;
		case 'a':
			opt.alpha = 1;
			opt.alpha_level = 255 - atoi(optarg);
			break;
		case 'f':
			opt.filelistfile = estrdup(optarg);
			break;
		case '0':
			opt.reload_button = atoi(optarg);
			break;
		case '1':
			opt.pan_button = atoi(optarg);
			break;
		case '2':
			opt.zoom_button = atoi(optarg);
			break;
		case '3':
			opt.menu_button = atoi(optarg);
			break;
		case '4':
			opt.prev_button = atoi(optarg);
			break;
		case '5':
			opt.next_button = atoi(optarg);
			break;
		case '8':
			opt.rotate_button = atoi(optarg);
			break;
		case '9':
			opt.blur_button = atoi(optarg);
			break;
		case '|':
			opt.start_list_at = estrdup(optarg);
			break;
		case 't':
			opt.thumbs = 1;
			opt.index_show_name = 1;
			opt.index_show_size = 0;
			opt.index_show_dim = 0;
			break;
		case 'j':
			opt.output_dir = estrdup(optarg);
			break;
		case 200:
			opt.bgmode = BG_MODE_TILE;
			opt.output_file = estrdup(optarg);
			break;
		case 201:
			opt.bgmode = BG_MODE_CENTER;
			opt.output_file = estrdup(optarg);
			break;
		case 202:
			opt.bgmode = BG_MODE_SCALE;
			opt.output_file = estrdup(optarg);
			break;
		case 203:
			opt.bgmode = BG_MODE_SEAMLESS;
			opt.output_file = estrdup(optarg);
			break;
		case 218:
			opt.bgmode = BG_MODE_FILL;
			opt.output_file = estrdup(optarg);
			break;
		case 204:
			free(opt.menu_style);
			opt.menu_style = estrdup(optarg);
			break;
		case 205:
			opt.default_zoom = atoi(optarg);
			break;
		case 206:
			opt.screen_clip = 0;
			break;
		case 207:
			opt.menu_border = atoi(optarg);
			break;
		case 208:
			opt.caption_path = estrdup(optarg);
			break;
		case 209:
			opt.actions[1] = estrdup(optarg);
			break;
		case 210:
			opt.actions[2] = estrdup(optarg);
			break;
		case 211:
			opt.actions[3] = estrdup(optarg);
			break;
		case 212:
			opt.actions[4] = estrdup(optarg);
			break;
		case 213:
			opt.actions[5] = estrdup(optarg);
			break;
		case 214:
			opt.actions[6] = estrdup(optarg);
			break;
		case 215:
			opt.actions[7] = estrdup(optarg);
			break;
		case 216:
			opt.actions[8] = estrdup(optarg);
			break;
		case 217:
			opt.actions[9] = estrdup(optarg);
			break;
		case 220:
			opt.no_jump_on_resort = 1;
			break;
		case 221:
			opt.hide_pointer = 1;
			break;
		case 222:
			opt.draw_actions = 1;
			break;
		case 223:
			opt.cache_thumbnails = 1;
			break;
		case 224:
			opt.cycle_once = 1;
			break;
		case 225:
			opt.xinerama = 0;
			break;
		case 226:
			opt.no_rotate_ctrl_mask = 1;
			break;
		case 227:
			opt.no_blur_ctrl_mask = 1;
			break;
		case 228:
			opt.menu_ctrl_mask = 1;
			break;
		case 230:
			opt.index_show_name = atoi(optarg);
			break;
		case 231:
			opt.index_show_size = atoi(optarg);
			break;
		case 232:
			opt.index_show_dim = atoi(optarg);
			break;
		case 233:
			opt.thumb_redraw = atoi(optarg);
			break;
		default:
			break;
		}
	}

	/* Now the leftovers, which must be files */
	if (optind < argc) {
		while (optind < argc) {
			/* If recursive is NOT set, but the only argument is a directory
			   name, we grab all the files in there, but not subdirs */
			add_file_to_filelist_recursively(argv[optind++], FILELIST_FIRST);
		}
	}

	/* So that we can safely be called again */
	optind = 1;
	return;
}

static void check_options(void)
{
	if ((opt.index + opt.collage) > 1) {
		weprintf("you can't use collage mode and index mode together.\n"
				"   I'm going with index");
		opt.collage = 0;
	}

	if (opt.full_screen && opt.multiwindow) {
		weprintf("you shouldn't combine multiwindow mode with full-screen mode,\n"
				"   Multiwindow mode has been disabled.");
		opt.multiwindow = 0;
	}

	if (opt.list && (opt.multiwindow || opt.index || opt.collage)) {
		weprintf("list mode can't be combined with other processing modes,\n"
				"   list mode disabled.");
		opt.list = 0;
	}

	if (opt.sort && opt.randomize) {
		weprintf("You cant sort AND randomize the filelist...\n"
				"randomize mode has been unset\n");
		opt.randomize = 0;
	}

	if (opt.loadables && opt.unloadables) {
		weprintf("You cant show loadables AND unloadables...\n"
				"you might as well use ls ;)\n"
				"loadables only will be shown\n");
		opt.unloadables = 0;
	}

	if (opt.thumb_title && (!opt.thumbs)) {
		weprintf("Doesn't make sense to set thumbnail title when not in\n"
				"thumbnail mode.\n");
		free(opt.thumb_title);
		opt.thumb_title = NULL;
	}

	return;
}

static void show_version(void)
{
	printf(PACKAGE " version " VERSION "\n");
	exit(0);
}

void show_mini_usage(void)
{
	fprintf(stderr, PACKAGE " - No loadable images specified.\n"
			"Use " PACKAGE " --help for detailed usage information\n");
	exit(1);
}

static void show_usage(void)
{
	fputs(
#include "help.inc"
	, stdout);
	exit(0);
}

static void feh_create_default_config(char *rcfile)
{
	FILE *fp;

	if ((fp = fopen(rcfile, "w")) == NULL) {
		weprintf("Unable to create default config file %s\n", rcfile);
		return;
	}

	fputs(
#include "fehrc.inc"
	, fp);
	fclose(fp);

	return;
}

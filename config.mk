# Change the prefix, if you want
prefix = /usr/local

# Directory for manuals
man_dir = $(prefix)/share/man

# Directory for executables
bin_dir = $(prefix)/bin

# debug = 1 if you want debug mode
debug =

# default CFLAGS
CFLAGS = -g -Wall -Wextra -O2

# Comment these out if you don't have libxinerama
xinerama = -DHAVE_LIBXINERAMA
xinerama_ld = -lXinerama

# Put extra header directories here
extra_headers =

# Put extra include (-Lfoo) directories here
extra_libs =

dmalloc = -DWITH_DMALLOC
# Enable this to use dmalloc
#CFLAGS += $(dmalloc)

# You can change the package name, if you want...
package = feh

# ... or the version
version = 1.4.1


# You should not need to change anything below this line.

CFLAGS += $(extra_headers) $(xinerama) -DPREFIX=\"$(prefix)\" \
	-DPACKAGE=\"$(package)\" -DVERSION=\"$(version)\" $(debug)

LDFLAGS = -lz -lpng -lX11 -lImlib2 -lfreetype -lXext -ldl -lm -lgiblib \
	$(xinerama_ld) $(extra_includes)

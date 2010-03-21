prefix ?= /usr/local

man_dir ?= $(prefix)/share/man
bin_dir ?= $(prefix)/bin

default:
	@$(MAKE) -C src prefix=$(prefix)

install:
	@echo installing $(man_dir)/{feh.1,feh-cam.1,gen-cam-menu.1}
	@mkdir -p $(man_dir)/man1
	@cp feh.1 feh-cam.1 $(man_dir)/man1
	@chmod 644 $(man_dir)/man1/feh.1 $(man_dir)/man1/feh-cam.1
	@ln -s feh-cam.1 $(man_dir)/man1/gen-cam-menu.1
	@echo installing $(bin_dir)/feh
	@mkdir -p $(bin_dir)
	@cp src/feh $(bin_dir)
	@chmod 755 $(bin_dir)/feh


uninstall:
	rm -f $(man_dir)/man1/feh.1 $(man_dir)/man1/feh-cam.1
	rm -f $(man_dir)/man1/gen-cam-menu.1
	rm -f $(bin_dir)/feh

clean:
	@$(MAKE) -C src clean

.PHONY: default install uninstall clean

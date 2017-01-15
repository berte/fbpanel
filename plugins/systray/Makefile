## miniconf makefiles ## 1.1 ##

TOPDIR := ../..

tray_src = egg-marshal.c \
    eggtraymanager.c \
    fixedtip.c \
    main.c
tray_cflags = -DPLUGIN $(GTK3_CFLAGS) 
tray_libs = $(GTK3_LIBS) 
tray_type = lib 

include $(TOPDIR)/.config/rules.mk

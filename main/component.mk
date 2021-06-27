#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_SRCDIRS := $(COMPONENT_SRCDIRS) command_loop command_loop/peripherals command_loop/tls

COMPONENT_EMBED_TXTFILES := server_root_cert.pem
COMPONENT_EMBED_TXTFILES += secret

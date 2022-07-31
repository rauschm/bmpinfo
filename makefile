ifeq ($(OS),Windows_NT)
  EXE =.exe
  OBJ =.obj
  QUOTE = "
  CC  = cl /nologo /D_CRT_SECURE_NO_WARNINGS /O2 /Fe:
  CP  = copy
  RM  = del 2>nul
  BIN_DIR = c:\doc\bin
else
  EXE =
  OBJ = .o
  QUOTE = '
  CC = cc -O2 -o
  CP = cp
  RM = rm -f
  BIN_DIR = /data/doc/bin
endif
PROTO_GEN = $(QUOTE)$$b=/^[A-Za-z].+[(,)]$$/||$$b;$$e=/\)$$/;if($$b){chomp;print;if($$e){print chr(59);$$b=!$$b};print$$/}$(QUOTE)

.PHONY : clean install

PROJ = $(notdir $(CURDIR))

$(PROJ)$(EXE) : $(PROJ).c
	@perl -ne $(PROTO_GEN) $(PROJ).c > $(PROJ)_proto.h
	$(CC) $(PROJ)$(EXE) $(PROJ).c

clean :
	@$(RM) $(PROJ)$(EXE) $(PROJ)$(OBJ) $(PROJ)_proto.h

install : $(PROJ)$(EXE)
	@$(CP) $(PROJ)$(EXE) $(BIN_DIR)

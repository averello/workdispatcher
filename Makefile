CC = gcc
CFLAGS = 
CFLAGS_PRIV = -Wall -Wextra -g3 -pedantic -std=c99 -I${INC} -D_XOPEN_SOURCE=700 -D__PROFILING__=1 -DDEBUG=1 $(CFLAGS) -I$(MEMORY_MANAGEMENT_LIB)/include
SHAREDFLAGS=
SHAREDFLAGS_PRIV=-fPIC -shared $(SHAREDFLAGS)
LDFLAGS = 
LDFLAGS_PRIV = -Llib -l${WD} -L$(MEMORY_MANAGEMENT_LIB)/lib -lmemorymanagement -lpthread $(LDFLAGS)

WLFLAGS=-Wl,-rpath,lib$(WD).so.$(WDMAJORVERSION)
BIN = bin
INC = include
OBJ = obj
SRC = src
MKDIR = mkdir

DOC = doc
LIB = lib
TEST = test
MKDIR = mkdir

MEMORY_MANAGEMENT_LIB=../memorymanagement

WD = workdispatcher

LD_LIBRARY_PATH := $(shell echo $$LD_LIBRARY_PATH):$(LIB)

UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
STRIP = strip --strip-unneeded
#LD_LIBRARY_PATH := $(LD_LIBRARY_PATH):$(OPENSSL)/lib
endif

ifeq ($(UNAME), Darwin)
STRIP = strip -X -x -S
endif


.PHONY: all directories compileall runall clean cleanall


#.SUFFIXES:            # Delete the default suffixes
#.SUFFIXES: .c .o .h           # Delete the default suffixes

# +------------+
# | Target all |
# +------------+

all: directories compileall

# +------------+
# | Target lib |
# +------------+

WDSTATIC = ${LIB}/lib${WD}.a
libworkdispatcher : directories libmemorymanagement $(WDSTATIC)

WDMAJORVERSION=0
WDMINORVERSION=1
WDRELEASENUMBER=1
WDSONAME = ${LIB}/lib${WD}.so.$(WDMAJORVERSION)
WDREALNAME = ${LIB}/lib${WD}.so.$(WDMAJORVERSION).$(WDMINORVERSION).$(WDRELEASENUMBER)
WDSHARED = ${LIB}/lib${WD}.so
FIRSTLINK = $(patsubst $(LIB)/%,%,$(WDREALNAME))
SECONDLINK = $(patsubst $(LIB)/%,%,$(WDSONAME))
libworkdispatchershared : directories $(WDSHARED)
	@ln -s $(FIRSTLINK) $(WDSONAME) 
	@ln -s $(SECONDLINK) $(WDSHARED) 

TESTS = $(patsubst $(TEST)/%.c,$(BIN)/%,$(wildcard $(TEST)/*.c))
tests : directories lib$(WD) $(TESTS)
	@for test in ${TESTS}; do \
		echo "**** Testing $$test"; \
		./"$$test" \
		echo "---- end of ${TESTSTRING}"; \
		done

test% : $(BIN)/test%
	@echo "**** Testing $@";
	@$<
	@echo "end of $@";


valgrind% : $(BIN)/test%
	@valgrind  --track-origins=yes --leak-check=full --show-reachable=yes $<

doc : $(DOC)/html/index.html

$(DOC)/html/index.html : 
	@cd $(DOC); /usr/bin/env doxygen


# +--------------------+
# | Target directories |
# +--------------------+

directories: ${OBJ} ${BIN} ${LIB} 

${OBJ}:
	${MKDIR} -p $@
${BIN}:
	${MKDIR} -p $@
${LIB}:
	${MKDIR} -p $@

# +----------------------------+
# | Target compilation with -c |
# +----------------------------+

${OBJ}/%.o : ${SRC}/%.c
	$(CC) -c -o $@ $< ${CFLAGS_PRIV} $(SHAREDFLAGS_PRIV)

${OBJ}/%.o : ${TEST}/%.c
	$(CC) -c -o $@ $< ${CFLAGS_PRIV}

${BIN}/% : ${OBJ}/%.o
	${CC} -o $@ $< ${LDFLAGS_PRIV}


${LIB}/lib${WD}.a : $(patsubst ${SRC}/%.c,${OBJ}/%.o,$(wildcard ${SRC}/*.c))
	${AR} r ${LIB}/lib${WD}.a $?

${LIB}/lib${WD}.so : $(WDSONAME)

${LIB}/lib${WD}.so.$(WDMAJORVERSION) : $(WDREALNAME)

${LIB}/lib${WD}.so.$(WDMAJORVERSION).$(WDMINORVERSION).$(WDRELEASENUMBER) : $(patsubst ${SRC}/%.c,${OBJ}/%.o,$(wildcard ${SRC}/*.c))
	$(CC) $(CFLAGS_PRIV) $(SHAREDFLAGS_PRIV) $(WLFLAGS) -o $@  $? -lc -lpthread
	$(STRIP) $@


libmemorymanagement : $(MEMORY_MANAGEMENT_LIB)/lib/libmemorymanagement

$(MEMORY_MANAGEMENT_LIB)/lib/libmemorymanagement : $(MEMORY_MANAGEMENT_LIB)/lib/libmemorymanagement.a
	$(MAKE) -C $(MEMORY_MANAGEMENT_LIB) 

# +-------------------+
# | Target compileall |
# +-------------------+

compileall: libworkdispatcher

# +---------------+
# | Target runall |
# +---------------+

runall: compileall
	$(MAKE) tests

# +--------------+
# | Target clean |
# +--------------+

clean:
	-rm -f ${OBJ}/* ${BIN}/* $(LIB)/*

# +-----------------+
# | Target cleanall |
# +-----------------+

cleanall: clean
	-rmdir ${OBJ} ${BIN} $(LIB) 2>/dev/null || exit 0
	-rm -f $(DOC)/html/search/*
	-rmdir $(DOC)/html/search
	-rm -f $(DOC)/{html,latex}/*
	-rmdir $(DOC)/{html,latex}
	-rm -f ${INC}/*~ ${SRC}/*~ *~ ${TEST}/*~


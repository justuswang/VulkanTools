# MikeW's Makefile for device simulation layer

PROJ_NAME = device_simulation

RELEASE_DIR  = build
DEBUG_DIR    = dbuild
EXTERNAL_DIR = external

LIB_RELEASE_NAME = ${RELEASE_DIR}/layersvt/libVkLayer_${PROJ_NAME}.so
LIB_DEBUG_NAME   = ${DEBUG_DIR}/layersvt/libVkLayer_${PROJ_NAME}.so

MAKE_PARALLEL = -j7

all:	${LIB_RELEASE_NAME} ${LIB_DEBUG_NAME}

${LIB_RELEASE_NAME}: ${EXTERNAL_DIR} ${RELEASE_DIR} layersvt/${PROJ_NAME}.cpp
	cd ${RELEASE_DIR}; make ${MAKE_PARALLEL}

${LIB_DEBUG_NAME}: ${EXTERNAL_DIR} ${DEBUG_DIR} layersvt/${PROJ_NAME}.cpp
	cd ${DEBUG_DIR}; make ${MAKE_PARALLEL}

${EXTERNAL_DIR}:
	./update_external_sources.sh

${RELEASE_DIR}:
	cmake -H. -B$@ -DCMAKE_BUILD_TYPE=Release

${DEBUG_DIR}:
	cmake -H. -B$@ -DCMAKE_BUILD_TYPE=Debug

test:	${LIB_RELEASE_NAME} ${LIB_DEBUG_NAME}
	./RUN_DEVSIM_TEST.sh

clean:
	rm -f ${LIB_RELEASE_NAME} ${LIB_DEBUG_NAME}

clobber: clean
	rm -rf ${RELEASE_DIR} ${DEBUG_DIR}

nuke: clobber
	rm -rf ${EXTERNAL_DIR}


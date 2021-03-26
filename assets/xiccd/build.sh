#! /bin/bash

here="$(realpath "$(dirname "${0}")")"


mainFunction () {
	createBlankBuildDir
	build
	so "mainFunction: moveBinary" mv "${here}/build/xiccd" "${here}/xiccd"
	so "mainFunction: removeBuild" rm --recursive "${here}/build"
}


build () {
	cd "${here}/build"
	so "build: macros" aclocal
	so "build: configuration" autoconf
	so "build: environment" automake --add-missing --foreign --force --copy
	so "build: builder" ./configure
	so "build: program" make
}


createBlankBuildDir () {
	so "cleanBuildDir: remove" rm --recursive --force "${here}/build"
	so "cleanBuildDir: create" cp --recursive sources "${here}/build"
}


so () {
	local tag="${1}"
	local commands="${*:2}"
	
	if ! error="$(eval "${commands}" 2>&1 >"/dev/null")" ; then
		if [ "${error}" == "" ] ; then
			error="Command failed: ${commands}"
		fi

		echo "${tag}: ${error}" >&2
		exit 1
	fi
}


set -e
mainFunction

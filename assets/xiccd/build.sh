#! /bin/bash


mainFunction () {
	silently "rm" "rm --recursive --force build"
	silently "cp" "cp --recursive sources build"
	cd build

	silently "aclocal" "aclocal"
	silently "automake" "automake --add-missing --foreign"
	silently "autoconf" "autoconf"
	silently "configure" "./configure"
	silently "make" "make"

	silently "mv" "mv xiccd ../xiccd"
	cd ..
	silently "rm" "rm --recursive build"
}


changeToThisProgramDir () {
	cd "$( dirname "${BASH_SOURCE[0]}" )"
}


silently () {
	function="${1}"
	command="${2}"
	error=$(eval "${command}" 2>&1 >"/dev/null")

	if [ ${?} -ne 0 ]; then
		echo "${function}: ${error}" >&2
		exit 1
	fi
}


changeToThisProgramDir
mainFunction

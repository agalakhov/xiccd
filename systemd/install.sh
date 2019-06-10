#! /bin/bash


mainFunction () {
	user="/usr/lib/systemd/user"
	service="xiccd.service"

	copyFile "${service}" "${user}/${service}"
	enableUserServices "${service}"
}


changeToThisProgramDir () {
	cd "$( dirname "${BASH_SOURCE[0]}" )"
}


checkPermissions () {
	user=$(id -u)

	if [ "${user}" -ne 0 ]; then
		if [ -f "/bin/sudo" ]; then
			sudo "${BASH_SOURCE[0]}"
        	exit ${?}
		else
			echo "Not enough permissions" >&2
			echo "For that type 'su'"
			exit 1
		fi
    fi
}


copyFile () {
	silently "copyFile" "cp ${1} ${2}"
}


enableUserServices () {
	silently "enableUserServices" "systemctl --user --global enable ${@}"
	reloadUsersServices ${@}
}


reloadUsersServices () {
	users=$(loginctl --no-legend list-users | awk '{print $2;}')

	for user in ${users}; do
		reloadUserServices "${user}" "${@}"
	done
}


reloadUserServices () {
	user="${1}"
	services="${2}"

	userNumber=$(id --user ${user})
	export DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/${userNumber}/bus"

	silentlyAsUser "${user}" "reloadUserServices: reload" "systemctl --user daemon-reload"
	silentlyAsUser "${user}" "reloadUserServices: start" "systemctl --user start ${services}"
}


setEnvironment () {
	changeToThisProgramDir
	checkPermissions
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


silentlyAsUser () {
	user="${1}"
	function="${2}"
	command="${3}"

	if [ -f "/bin/sudo" ]; then
		silently "${function}" "sudo -E -u ${user} ${command}"
	fi
}


setEnvironment
mainFunction

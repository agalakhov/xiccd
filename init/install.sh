#!/bin/bash


main () {
	this="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
	user="/usr/lib/systemd/user"
	service="xiccd.service"

	copyFile $this/$service $user/$service
	enableService $service
	reloadService $service
}


copyFile () {
	execute "copyFile" "sudo cp ${1} ${2}"
}


enableService () {
	execute "enableService" "sudo systemctl --user --global enable ${1}"
}


execute () {
    error=$(${2} 2>&1 >/dev/null)

    if [ ${?} -ne 0 ]; then
        echo "${1}: $error"
        exit 1
    fi
}


reloadService () {
	execute "reloadService" "systemctl --user daemon-reload"
	execute "reloadService" "systemctl --user start ${1}"
}


main

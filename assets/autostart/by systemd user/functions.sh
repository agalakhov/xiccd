#! /bin/bash

Service="xiccd.service"


post_install () {
	if [[ "$(whoami)" == "root" ]]; then
		systemctl --user --global enable "${Service}"
		CommandForAllUsers systemctl --user start "${Service}"
	fi
}


pre_remove () {
	if [[ "$(whoami)" == "root" ]]; then
		CommandForAllUsers systemctl --user stop "${Service}"
		systemctl --user --global disable "${Service}"
	fi
}


post_remove () {
	if [[ "$(whoami)" == "root" ]]; then
		CommandForAllUsers systemctl --user daemon-reload
	fi
}


post_upgrade () {
	post_remove
	post_install
}


CommandAsUser () {
	local user="${1}"
	local command="${*:2}"

	local userId="$(id --user "${user}")"
	local bus="unix:path=/run/user/${userId}/bus"
	local sudoAsUser="sudo -u ${user} DBUS_SESSION_BUS_ADDRESS=${bus}"
	
	${sudoAsUser} ${command}
}


CommandForAllUsers () {
	local command="${*}"
	local users; readarray -t users <<< "$(loginctl --no-legend list-users | awk '{print $2;}')"
	
	for user in "${users[@]}"; do
		CommandAsUser "${user}" "${command}"
	done
}


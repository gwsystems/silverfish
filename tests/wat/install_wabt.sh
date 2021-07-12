#!/bin/bash

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
	if grep "Ubuntu" /etc/os-release > /dev/null; then
		echo "Installing wabt via apt-get"
		sudo apt-get install wabt
	fi
elif [[ "$OSTYPE" == "darwin"* ]]; then
	echo "Installing wabt via brew"
	brew install wabt
fi

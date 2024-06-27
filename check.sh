#!/bin/sh
# Check if trip has been installed

set -eu

EXPECTED="$(realpath "$1/trip")"
for file in $(echo "$PATH" | awk -v RS=: '{ print $1 "/trip" }')
do
    if [ ! -e "$file" ]
    then
	continue
    fi

    if [ "$(realpath "$file")" = "$EXPECTED" ]
    then
	exit 0
    fi
done

tput setaf 1
tput smso
1>&2 cat <<EOF
It appears that trip was NOT installed successfully.  Make sure that

  $1

is in your \$PATH.  For example, if you are using the default Bash
shell, then you have to add

  export PATH="$1:\$PATH"

to your ~/.bashrc file.
EOF
tput sgr0


if [ -e ~/.bashrc ] && [ "$(tty)" != "not a tty" ]
then
    1>&2 printf "\nFix  ~/.bashrc automatically (y/N)? "
    read -r IN
    if [ "$IN" = "y" ] || [ "$IN" = "Y" ]
    then
	ed -s ~/.bashrc <<EOF
\$a

# look programms up in the local bin
export PATH="$1:\$PATH"
.
wq
EOF
	1>&2 echo "Done, restart your shell to take effect"
	exit 0
    fi
fi

exit 1

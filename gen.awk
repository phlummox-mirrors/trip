# Copyright 2022, 2023 Philip Kaludercic
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see
# <https://www.gnu.org/licenses/>.

BEGIN {
    FS = "\t";
    print "#include \"../macs.h\"";
    data[""] = "";
}

/^:/ {                          # copy verbatim
    sub(/^:[[:space:]]*/, "");
    print;
}

{ sub(/[[:space:]]*#.*$/, ""); } # remove comments

/^[[:alnum:]]+ +/ {
    print "Missing tab on line", FILENAME ":" NR > "/dev/stderr"
    exit 1
}

/^[[:alnum:]]+/ {
    data[$1] = $2;
}

function gen() {
    if (length(data) <= 1) {
	return;
    }

    args = gensub(/(^|(,))([[:space:]]*[[:alnum:]_]+[[:space:]]*(\*|\[\])*[[:space:]]*)+(\[\]|\*|[[:space:]])[[:space:]]*(const[[:space:]]+)?([[:alnum:]_]+)/,
		  "\\2 \\7", "g",
		  data["params"])

    if (data["name"] in seen) {
	print "Duplicate entry on ", FILENAME ":" NR > "/dev/stderr"
	exit 1;
    }
    seen[data["name"]] = 1;

    print                                               \
	"DEF(" data["return"] ",",                      \
	data["name"] ",",                               \
	"(" data["params"] "),",                        \
	"(" args "),",                                  \
	"(" data["fail"] "),",                       \
	data["errno"] ")"
    delete data;

}

/^[[:space:]]*$/ { gen(); }
ENDFILE          { gen(); }

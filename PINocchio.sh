# PINocchio.sh
#
# Copyright (C) 2017 Alexandre Luiz Brisighello Filho
#
# This software may be modified and distributed under the terms
# of the MIT license.  See the LICENSE file for details.

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PINocchio="$DIR/obj-intel64/PINocchio.so"
PIN_FLAGS=""
PROGRAM=""

# Check for Pin
if ! [ -x "$(command -v pin)" ]; then
  echo 'Error: pin is not in PATH.' >&2
  exit 1
fi

# Check for PINocchio
if [ ! -f $PINocchio ]; then
    echo "PINocchio not found."
    exit 1
fi

# Parse for pin arguments.
while test $# -gt 0; do
        case "$1" in
                -h|--help)
                        shift
                        PIN_FLAGS="$PIN_FLAGS -h"
                        ;;
                -p)
                        shift
                        PIN_FLAGS="$PIN_FLAGS -p $1"
                        shift
                        ;;
                -t)
                        shift
                        PIN_FLAGS="$PIN_FLAGS -t"
                        ;;
                -o)
                        shift
                        PIN_FLAGS="$PIN_FLAGS -o $1"
                        shift
                        ;;
                *)
                        break
                        ;;
        esac
done

# Check the parameters. If no parameter, just quit.
if test $# -lt 1; then
        echo "Error: no program specified"
        exit 1
fi

# Check for provided program.
if [ ! -f $1 ]; then
    echo "Program \"$1\" not found."
    exit 1
fi

PROGRAM="$*"

pin -t $PINocchio $PIN_FLAGS -- $PROGRAM

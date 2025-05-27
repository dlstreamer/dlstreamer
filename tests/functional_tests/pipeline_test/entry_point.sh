#!/bin/bash
set -e

arguments=$@

SCRIPTDIR="$(dirname "$(readlink -fm "$0")")"

if [[ -z "$MODEL_PROCS_PATH" ]]; then
    echo "ERROR: MODEL_PROCS_PATH environment variable is not set"
    exit -1
fi

if [[ -z "$LABELS_PATH" ]]; then
    echo "WARNING: LABELS_PATH environment variable is not set"
fi

pip3 install -r $SCRIPTDIR/requirements.txt

# Some configs require working dir be set to level of regression_tests dir location
WORK_DIR="$(dirname "$(dirname "${SCRIPTDIR}" )" )"
pushd $WORK_DIR

echo "Working dir is: `pwd`"
PYTHONPATH=$SCRIPTDIR:$PYTHONPATH python3 -u -m regression_test ${arguments}

popd

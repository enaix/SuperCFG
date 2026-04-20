#!/bin/env bash

LSYSTEMS=$(cat lsystems.json | python -c 'import json,sys;obj=json.load(sys.stdin);[print(x["str"]) for x in obj];')
CLING_PATH=""

i=0

mkdir -p run_lsystems/

while IFS= read -r line; do
	echo "Running task $i..."
	python3 -m superggd --module=superggd/examples/lsystem/__init__.py --lsystem="$line" --comp-strategy=skip --output-folder="run_lsystems/run_$i" --log-min-priority=panic --cling="$CLING_PATH" --mapping-type=lex 2>&1 | tee "run_systems/run_${i}_log.txt"
	i=$(($i+1))
done <<< "$LSYSTEMS"

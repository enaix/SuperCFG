#!/bin/env bash

LSYSTEMS=$(cat lsystems_small.json | python -c 'import json,sys;obj=json.load(sys.stdin);[print(x["str"]) for x in obj];')
#CLING_PATH=""

i=0

mkdir -p run_lsystems/

while IFS= read -r line; do
	echo "Running task $i..."
	LOG_FILE="./run_lsystems/run_${i}_log.txt"
	python3 -m superggd --module=superggd/examples/lsystem/__init__.py --lsystem="$line" --comp-strategy=skip --compilation-timeout=120 --output-folder="run_lsystems/run_$i" --log-min-priority=panic --cling="$CLING_PATH" --mapping-type=lex 2>&1 | tee "$LOG_FILE"
	i=$(($i+1))
done <<< "$LSYSTEMS"

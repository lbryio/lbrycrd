#!/bin/bash
	
for f in /tmp/sql/*; do
	case "$f" in
		*.sql)    echo "$0: running $f"; "${mysql[@]}" --force < "$f"; echo ;;
		*.sql.gz) echo "$0: running $f"; gunzip -c "$f" | "${mysql[@]}"; echo ;;
		*)        echo "$0: ignoring $f" ;;
	esac
	echo
done

#!/bin/bash

# Try to show a nice plot comparing a naively locked implementation
# with a mostly lock-free one.

REAL_VECT_FILE=`mktemp`
FAKE_VECT_FILE=`mktemp`

if [ -z $MIN_THREADS ]; then
    MIN_THREADS="1"
fi

if [ -z $MAX_THREADS ]; then
    MAX_THREADS="50"
fi

if [ -z $OUTPUT_PNG ]; then
    OUTPUT_PNG="graphs/`date +'%H-%M-%S-%F'`.png"
fi

for i in `seq $MIN_THREADS $MAX_THREADS`; do
    echo -n "$i  " >> "$REAL_VECT_FILE"
    echo -n "$i  " >> "$FAKE_VECT_FILE"
    echo "Stressing with with $i threads ..."
    ./build/test-fixed-vector --quiet $EXTRA_ARGS \
	--thread-count-lower "$i" --thread-count-upper "$i" --test-type real \
	>> "$REAL_VECT_FILE"
    ./build/test-fixed-vector --quiet $EXTRA_ARGS \
	--thread-count-lower "$i" --thread-count-upper "$i" --test-type fake \
	>> "$FAKE_VECT_FILE"
done

GNUPLOT_CMD_FILE=`mktemp`
mkdir -p graphs

echo "set terminal png size 1400,400" >> $GNUPLOT_CMD_FILE
echo "set output '$OUTPUT_PNG'" >> $GNUPLOT_CMD_FILE
echo "set xlabel 'Threads'" >> $GNUPLOT_CMD_FILE
echo "set xtics 1" >> $GNUPLOT_CMD_FILE
echo "set ylabel 'Time (milliseconds)'" >> $GNUPLOT_CMD_FILE

echo "plot '$FAKE_VECT_FILE' with line title \"Naive\", \
           '$REAL_VECT_FILE' with line title \"Real\"" >> $GNUPLOT_CMD_FILE

gnuplot "$GNUPLOT_CMD_FILE"
echo "Wrote to $OUTPUT_PNG"

# tool

TOOL_BINARY=./cbmc-binary
TOOL_NAME=CBMC
FIND_OPTIONS=""

# function to run the tool

run()
{
  if [ "$PROP" = "termination" ] ; then
    PROPERTY="$PROPERTY --no-assertions --no-self-loops-to-assumptions"
  fi

  gmon_suffix=$GMON_OUT_PREFIX
  export GMON_OUT_PREFIX="goto-cc_$gmon_suffix"
  ./goto-cc --function $ENTRY "${BM[@]}" -o $LOG.bin
  if [ -n "$FAIL_FUNCTION" ]; then
    ./goto-instrument $LOG.bin $LOG.bin --remove-function-body "$FAIL_FUNCTION" \
                                        --generate-function-body "$FAIL_FUNCTION" \
                                        --generate-function-body-options assert-false
  fi

  export GMON_OUT_PREFIX="cbmc_$gmon_suffix"
timeout 875 bash -c ' \
\
ulimit -v 15000000 ; \
\
EC=42 ; \
UNWIND=0 ; \
for c in 2 6 12 17 21 40 200 400 1025 2049 268435456 ; do \
echo "Unwind: $c" > $LOG.latest ; \
UNWIND=$c ; \
echo "Rounds: 1 (Fixed)" >> $LOG.latest ; \
echo "./cbmc-binary --lazy-c-seq-rounds 1 --no-unwinding-assertions --no-standard-checks --graphml-witness $LOG.witness --slice-formula --unwind $c --stop-on-fail --object-bits $OBJ_BITS $PROPERTY $LOG.bin" >> $LOG.latest ; \
./cbmc-binary --lazy-c-seq-rounds 1 --no-unwinding-assertions --no-standard-checks --graphml-witness $LOG.witness --slice-formula --unwind $c --stop-on-fail --object-bits $OBJ_BITS $PROPERTY $LOG.bin >> $LOG.latest 2>&1 ; \
ec=$? ; \
if [ $ec -eq 0 ] ; then \
if ! tail -n 10 $LOG.latest | grep -q "^VERIFICATION SUCCESSFUL$" ; then ec=1 ; else \
echo "./cbmc-binary --lazy-c-seq-rounds 1 --slice-formula --no-standard-checks --unwind $c --stop-on-fail --object-bits $OBJ_BITS $PROPERTY $LOG.bin" >> $LOG.latest ; \
./cbmc-binary --lazy-c-seq-rounds 1 --slice-formula --no-standard-checks --unwind $c --stop-on-fail --object-bits $OBJ_BITS $PROPERTY $LOG.bin > /dev/null 2>&1 || ec=42 ; \
fi ; \
fi ; \
if [ $ec -eq 10 ] ; then \
if ! tail -n 10 $LOG.latest | grep -q "^VERIFICATION FAILED$" ; then ec=1 ; fi ; \
fi ; \
\
case $ec in \
0) EC=0 ; mv $LOG.latest $LOG.ok ; echo "EC=$EC" >> $LOG.ok ; break ;; \
10) EC=10 ; mv $LOG.latest $LOG.ok ; echo "EC=$EC" >> $LOG.ok ; break ;; \
42) EC=42 ; mv $LOG.latest $LOG.ok ; echo "EC=$EC" >> $LOG.ok ;; \
*) if [ $EC -ne 0 ] ; then EC=$ec ; mv $LOG.latest $LOG.ok ; fi ; echo "EC=$EC" >> $LOG.ok ; break ;; \
esac ; \
\
done ; \
if [ $EC -ne 10 ]; then \
for d in 2 3 4 5 6 7 8 9 10 ; do \
echo "Unwind: $UNWIND" >> $LOG.latest ; \
echo "Rounds: $d" >> $LOG.latest ; \
echo "./cbmc-binary --lazy-c-seq-rounds $d --no-unwinding-assertions --no-standard-checks --graphml-witness $LOG.witness --slice-formula --unwind $UNWIND --stop-on-fail --object-bits $OBJ_BITS $PROPERTY $LOG.bin" >> $LOG.latest ; \
./cbmc-binary --lazy-c-seq-rounds $d --no-unwinding-assertions --no-standard-checks --graphml-witness $LOG.witness --slice-formula --unwind $UNWIND --stop-on-fail --object-bits $OBJ_BITS $PROPERTY $LOG.bin >> $LOG.latest 2>&1 ; \
ec=$? ; \
ec_1=$? ; \
if [ $ec -eq 0 ] ; then \
if ! tail -n 10 $LOG.latest | grep -q "^VERIFICATION SUCCESSFUL$" ; then ec=1 ; else \
ec_1=42 ; \
echo "./cbmc-binary --lazy-c-seq-rounds $d --check-rounds --no-unwinding-assertions --slice-formula --no-standard-checks --unwind $UNWIND --stop-on-fail --object-bits $OBJ_BITS $PROPERTY $LOG.bin" >> $LOG.latest ; \
./cbmc-binary --lazy-c-seq-rounds $d --check-rounds --no-unwinding-assertions --slice-formula --no-standard-checks --unwind $UNWIND --stop-on-fail --object-bits $OBJ_BITS $PROPERTY $LOG.bin > /dev/null 2>&1 ; \
ec_1=$? ; \
if [ $ec_1 -eq 0 ] ; then \
ec=42 ; \
fi ; \
fi ; \
fi ; \
if [ $ec -eq 10 ] ; then \
if ! tail -n 10 $LOG.latest | grep -q "^VERIFICATION FAILED$" ; then ec=1 ; fi ; \
fi ; \
\
case $ec in \
0) EC=0 ; mv $LOG.latest $LOG.ok ; echo "EC=$EC" >> $LOG.ok ; break ;; \
10) EC=10 ; mv $LOG.latest $LOG.ok ; echo "EC=$EC" >> $LOG.ok ; break ;; \
42) EC=42 ; mv $LOG.latest $LOG.ok ; echo "EC=$EC" >> $LOG.ok ;; \
*) if [ $EC -ne 0 ] ; then EC=$ec ; mv $LOG.latest $LOG.ok ; fi ; echo "EC=$EC" >> $LOG.ok ; break ;; \
esac ; \
\
done ; \
fi \
'
  if [ ! -s $LOG.ok ] ; then
    mv $LOG.latest $LOG.ok ; echo "EC=42" >> $LOG.ok
  fi
  eval `tail -n 1 $LOG.ok`
}

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
timeout 3600 bash -c ' \
\
ulimit -v 15000000 ; \
\
EC=42 ; \
echo "Unwind: $UNWIND_VAL" > $LOG.latest ; \
echo "./cbmc-binary --verbosity 8 --no-unwinding-assertions --no-standard-checks --graphml-witness $LOG.witness --slice-formula --unwind $UNWIND_VAL --stop-on-fail --object-bits $OBJ_BITS $PROPERTY $LOG.bin" >> $LOG.latest ; \
./cbmc-binary --verbosity 8 --no-unwinding-assertions --no-standard-checks --graphml-witness $LOG.witness --slice-formula --unwind $UNWIND_VAL --stop-on-fail --object-bits $OBJ_BITS $PROPERTY $LOG.bin >> $LOG.latest 2>&1 ; \
ec=$? ; \
if [ $ec -eq 0 ] ; then \
if ! tail -n 10 $LOG.latest | grep -q "^VERIFICATION SUCCESSFUL$" ; then ec=1 ; \
fi ; \
fi ; \
if [ $ec -eq 10 ] ; then \
if ! tail -n 10 $LOG.latest | grep -q "^VERIFICATION FAILED$" ; then ec=1 ; fi ; \
fi ; \
\
case $ec in \
0) EC=0 ; mv $LOG.latest $LOG.ok ; echo "EC=$EC" >> $LOG.ok  ;; \
10) EC=10 ; mv $LOG.latest $LOG.ok ; echo "EC=$EC" >> $LOG.ok  ;; \
42) EC=42 ; mv $LOG.latest $LOG.ok ; echo "EC=$EC" >> $LOG.ok ;; \
*) if [ $EC -ne 0 ] ; then EC=$ec ; mv $LOG.latest $LOG.ok ; fi ; echo "EC=$EC" >> $LOG.ok  ;; \
esac ; \
 \
'
  EXIT_CODE=$?

  if [ "$EXIT_CODE" -eq 124 ]; then
    EC=42 ; mv $LOG.latest $LOG.ok ; echo "EC=$EC" >> $LOG.ok
  fi

  if [ ! -s $LOG.ok ] ; then
    mv $LOG.latest $LOG.ok ; echo "EC=42" >> $LOG.ok
  fi
  eval `tail -n 1 $LOG.ok`
}

get_unwind_and_rounds() {
  local csv="output_unwind_rounds.csv"
  local prop=$(normalize_property "$PROP")
  local bench="$(basename ${BM[0]})"
  bench="${bench%.i}"  # rimuove estensione .i se presente
  bench="${bench%.c}"  # rimuove anche .c se presente

  if [ ! -f "$csv" ]; then
    echo "⚠️  File CSV non trovato: $csv"
    export UNWIND_VAL=2
    return
  fi

  local match_line=$(awk -F',' -v p="$prop" -v b="$bench" \
    '$1 == p && $2 == b {print $0}' "$csv")

  if [ -n "$match_line" ]; then
    local uw=$(echo "$match_line" | cut -d',' -f3)

    export UNWIND_VAL="${uw:-2}"
  else
    echo "⚠️  Riga non trovata nel CSV per $bench [$prop], uso default"
    export UNWIND_VAL=2
  fi
}

normalize_property() {
  case "$1" in
  overflow) echo "no-overflow" ;;
  unreach_call) echo "unreach-call" ;;  # se ci sono più varianti
  memsafety) echo "valid-memsafety" ;;
  *) echo "$1" ;;  # default: nessuna modifica
  esac
}

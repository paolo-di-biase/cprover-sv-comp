
# main tool wrapper script
# run "make <tool>-wrapper" to generate the wrapper script

# map properties to tool options

declare -A OPTIONS
OPTIONS["label"]="--error-label"
OPTIONS["unreach_call"]=""
OPTIONS["termination"]=""
OPTIONS["overflow"]="--signed-overflow-check --no-assertions"
OPTIONS["memsafety"]="--pointer-check --memory-leak-check --bounds-check --no-assertions"
OPTIONS["memcleanup"]="--pointer-check --memory-leak-check --memory-cleanup-check --bounds-check --no-assertions"
OPTIONS["runtime-exception"]="--uncaught-exception-check-only-for"

parse_property_file()
{
  local fn=$1

  cat $fn | sed 's/[[:space:]]//g' | perl -n -e '
if(/^CHECK\(init\((\S+)\(\)\),LTL\((\S+)\)\)$/) {
  print "ENTRY=$1\n";
  print "PROP=\"label\"\nLABEL=\"$1\"\n" if($2 =~ /^G!label\((\S+)\)$/);
  if($2 =~ /^G!call\((?<fn>\S+)\(\)\)$/) {
    print "PROP=\"unreach_call\"\n";
    print "FAIL_FUNCTION=\"$+{fn}\"\n";
  }
  print "PROP=\"unreach_call\"\n" if($2 =~ /^Gassert$/);
  print "PROP=\"memsafety\"\n" if($2 =~ /^Gvalid-(free|deref|memtrack)$/);
  print "PROP=\"memcleanup\"\n" if($2 =~ /^Gvalid-memcleanup$/);
  print "PROP=\"overflow\"\n" if($2 =~ /^G!overflow$/);
  print "PROP=\"termination\"\n" if($2 =~ /^Fend$/);
  # for Java only:
  print "PROP=\"runtime-exception\"\nLABEL=\"$1\"\n" if($2 =~ /^G!uncaught\((\S+)\)$/);
}'
}

parse_result()
{
  if tail -n 50 $LOG.ok | \
      grep -Eq "Unmodelled library functions have been called" ; then
    echo 'UNKNOWN'
  elif tail -n 50 $LOG.ok | \
      grep -Eq "^(\[.*\] .*__CPROVER_memory_leak == NULL|[[:space:]]*__CPROVER_memory_leak == NULL$)" ; then
    if [[ "$PROP" == "memcleanup" ]]; then
      echo 'FALSE(valid-memcleanup)'
    else
      echo 'FALSE(valid-memtrack)'
    fi
  elif tail -n 50 $LOG.ok | \
      grep -Eq "^(\[.*\] |[[:space:]]*)dynamically allocated memory never freed in " ; then
    if [[ "$PROP" == "memcleanup" ]]; then
      echo 'FALSE(valid-memcleanup)'
    else
      echo 'FALSE(valid-memtrack)'
    fi
  elif tail -n 50 $LOG.ok | \
      grep -Eq "^(\[.*\] |[[:space:]]*)dereference failure:" ; then
    echo 'FALSE(valid-deref)'
  elif tail -n 50 $LOG.ok | \
      grep -Eq "^(\[.*\] |[[:space:]]*)array.* (lower|upper) bound in " ; then
    echo 'FALSE(valid-deref)'
  elif tail -n 50 $LOG.ok | \
      grep -Eq "^[[:space:]]+mem(cpy|set|move) (source region readable|destination region writeable)" ; then
    echo 'FALSE(valid-deref)'
  elif tail -n 50 $LOG.ok | \
      grep -Eq "^(\[.*\] double free|[[:space:]]*double free$|[[:space:]]*free argument must be NULL or valid pointer$)" ; then
    echo 'FALSE(valid-free)'
  elif tail -n 50 $LOG.ok | \
      grep -Eq "^(\[.*\] free called for stack-allocated object|[[:space:]]*free called for stack-allocated object$)" ; then
    echo 'FALSE(valid-free)'
  elif tail -n 50 $LOG.ok | \
      grep -Eq "^(\[.*\] free argument has offset zero|[[:space:]]* free argument has offset zero$)" ; then
    if tail -n 50 $LOG.ok | grep -Eq "^[[:space:]]+[a-zA-Z0-9_]+=INVALID-" ; then
      echo 'FALSE(valid-deref)'
    else
      echo 'FALSE(valid-free)'
    fi
  elif tail -n 50 $LOG.ok | \
      grep -Eq "^(\[.*\] |[[:space:]]*)free argument (is|must be) dynamic object" ; then
    if tail -n 50 $LOG.ok | grep -Eq "^[[:space:]]+[a-zA-Z0-9_]+=INVALID-" ; then
      echo 'FALSE(valid-deref)'
    else
      echo 'FALSE(valid-free)'
    fi
  elif tail -n 50 $LOG.ok | \
      grep -Eq "^(\[.*\] |[[:space:]]*)arithmetic overflow on signed" ; then
    echo 'FALSE(no-overflow)'
  elif [[ "$PROP" == "termination" ]]; then
    echo 'FALSE(termination)'
  else
    echo FALSE
  fi
}

process_graphml()
{
  if [ -f $LOG.witness ]; then
    if [ $1 -eq 0 ]; then
      TYPE="correctness_witness"
    else
      TYPE="violation_witness"
    fi
    cat $LOG.witness | perl -p -e "s/(<graph edgedefault=\"directed\">)/\$1\\E
      <data key=\"witness-type\">$(echo $TYPE)<\/data>
      <data key=\"producer\">$(echo $TOOL_NAME)<\/data>
      <data key=\"specification\">$(<$PROP_FILE)<\/data>
      <data key=\"programfile\">$(echo ${BM[0]} | sed 's8/8\\/8g')<\/data>
      <data key=\"programhash\">$(sha256sum ${BM[0]} | awk '{print $1}')<\/data>
      <data key=\"architecture\">${BIT_WIDTH}bit<\/data>
      <data key=\"creationtime\">$(date -Iseconds)<\/data>\\Q/"
  fi
}

OBJ_BITS="11"
BIT_WIDTH="64"
declare -a BM
BM=()
PROP_FILE=""
WITNESS_FILE=""

while [ -n "$1" ] ; do
  case "$1" in
    --32|--64) BIT_WIDTH="${1##--}" ; shift 1 ;;
    --propertyfile) PROP_FILE="$2" ; shift 2 ;;
    --graphml-witness) WITNESS_FILE="$2" ; shift 2 ;;
    --version) $TOOL_BINARY --version ; exit 0 ;;
    *) SRC=(`eval "find $1 $FIND_OPTIONS"`) ; BM=("${BM[@]}" "${SRC[@]}") ; shift 1 ;;
  esac
done

if [ -z "${BM[0]}" ] || [ -z "$PROP_FILE" ] ; then
  echo "Missing benchmark or property file"
  exit 1
fi

if [ ! -s "${BM[0]}" ] || [ ! -s "$PROP_FILE" ] ; then
  echo "Empty benchmark or property file"
  exit 1
fi

eval `parse_property_file $PROP_FILE`

if [[ "$PROP" == "" ]]; then
  echo "Unrecognized property specification"
  exit 1
elif [[ "$PROP" == "label" ]]; then
  PROPERTY="${OPTIONS[$PROP]} $LABEL"
elif [[ "$PROP" == "runtime-exception" ]]; then
  PROPERTY="${OPTIONS[$PROP]} $LABEL"
else
  PROPERTY=${OPTIONS[$PROP]}
fi

export ENTRY
export PROPERTY
export BIT_WIDTH
export BM
export PROP
export OBJ_BITS

export GMON_OUT_PREFIX=`basename ${BM[0]}`.gmon.out

export BM_DIR=`mktemp -d -t ${TOOL_NAME}-benchmark.XXXXXX`

export LOG=`mktemp -t ${TOOL_NAME}-log.XXXXXX`
trap "rm -rf $LOG $LOG.latest $LOG.ok $LOG.witness $LOG.bin $BM_DIR" EXIT

run

if [ ! -s $LOG.ok ] ; then
  exit 1
fi

cat $LOG.ok
case $EC in
  0) if [[ "$WITNESS_FILE" != "" ]]; then process_graphml $EC > $WITNESS_FILE; \
fi; echo "TRUE" ;;
  10) if [[ "$WITNESS_FILE" != "" ]]; then process_graphml $EC > $WITNESS_FILE;\
 fi; parse_result ;;
  *) echo "UNKNOWN" ;;
esac
exit $EC

#!/bin/bash

DBGEN_PATH="../../../Include/TPC-H V3.0.1/dbgen"
SCALE_FACTOR=0.01    # GB
OUTPUT_DIR="../data"

declare -A TABLE_HEADERS
TABLE_HEADERS["region"]="R_REGIONKEY,R_NAME,R_COMMENT"
TABLE_HEADERS["nation"]="N_NATIONKEY,N_NAME,N_REGIONKEY,N_COMMENT"
TABLE_HEADERS["part"]="P_PARTKEY,P_NAME,P_MFGR,P_BRAND,P_TYPE,P_SIZE,P_CONTAINER,P_RETAILPRICE,P_COMMENT"
TABLE_HEADERS["supplier"]="S_SUPPKEY,S_NAME,S_ADDRESS,S_NATIONKEY,S_PHONE,S_ACCTBAL,S_COMMENT"
TABLE_HEADERS["partsupp"]="PS_PARTKEY,PS_SUPPKEY,PS_AVAILQTY,PS_SUPPLYCOST,PS_COMMENT"
TABLE_HEADERS["customer"]="C_CUSTKEY,C_NAME,C_ADDRESS,C_NATIONKEY,C_PHONE,C_ACCTBAL,C_MKTSEGMENT,C_COMMENT"
TABLE_HEADERS["orders"]="O_ORDERKEY,O_CUSTKEY,O_ORDERSTATUS,O_TOTALPRICE,O_ORDERDATE,O_ORDERPRIORITY,O_CLERK,O_SHIPPRIORITY,O_COMMENT"
TABLE_HEADERS["lineitem"]="L_ORDERKEY,L_PARTKEY,L_SUPPKEY,L_LINENUMBER,L_QUANTITY,L_EXTENDEDPRICE,L_DISCOUNT,L_TAX,L_RETURNFLAG,L_LINESTATUS,L_SHIPDATE,L_COMMITDATE,L_RECEIPTDATE,L_SHIPINSTRUCT,L_SHIPMODE,L_COMMENT"

DBGEN_EXEC="$DBGEN_PATH/dbgen"
if [ ! -x "$DBGEN_EXEC" ]; then
    echo "Error: dbgen not found or not executable at $DBGEN_EXEC"
    exit 1
fi

echo "Generating TPC-H data (scale factor $SCALE_FACTOR)..."
(cd "$DBGEN_PATH" && ./dbgen -s $SCALE_FACTOR)

mkdir -p "$OUTPUT_DIR"

for tbl_file in "$DBGEN_PATH"/*.tbl; do
    base_name=$(basename "$tbl_file" .tbl)
    dest_csv="$OUTPUT_DIR/${base_name}.csv"
    header="${TABLE_HEADERS[$base_name]}"

    if [ -z "$header" ]; then
        echo "Warning: No header defined for $base_name. Skipping."
        continue
    fi

    echo "Converting $tbl_file → $dest_csv"
    {
        echo "$header"
        awk -F"|" -v OFS="," 'NF>1 {NF--; print}' "$tbl_file"
    } > "$dest_csv"

    rm -f "$tbl_file"
done

echo "TPC-H data generated and saved to $OUTPUT_DIR"

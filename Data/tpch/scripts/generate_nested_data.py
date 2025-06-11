import re
import json

DDL_FILE = "../../../Include/TPC-H V3.0.1/dbgen/dss.ddl"
OUTPUT_DIR = "../data/"

type_map = {
    'INTEGER': 'integer',
    'INT': 'integer',
    'SMALLINT': 'integer',
    'CHAR': 'string',
    'VARCHAR': 'string',
    'DECIMAL': 'number',
    'FLOAT': 'number',
    'DATE': 'string'
}

table_descriptions = {
    "region": "Regions of the world",
    "nation": "Nations linked to regions",
    "part": "Parts or products",
    "supplier": "Suppliers of parts",
    "partsupp": "Supplier-part relationships",
    "customer": "Customers of the company",
    "orders": "Orders made by customers",
    "lineitem": "Line items of orders (detailed order info)"
}

def parse_ddl(ddl_path):
    with open(ddl_path, 'r') as f:
        content = f.read()

    tables = re.findall(r'CREATE TABLE\s+(\w+)\s*\((.*?)\);', content, re.DOTALL | re.IGNORECASE)


    metadata = {
        "name": "TPC-H Benchmark Dataset",
        "title": "TPC-H Benchmark Dataset",
        "description": "TPC-H dataset generated from dss.ddl schema parsing.",
        "tables": []
    }

    for table_name, cols_str in tables:
        # split columns by comma
        # but ignore commas inside parentheses (e.g. DECIMAL(15,2))
        columns_raw = re.findall(r'(\w+\s+\w+(?:\(\d+(?:,\d+)?\))?[^,]*)(?:,|$)', cols_str, re.IGNORECASE)

        fields = []
        for col_def in columns_raw:
            col_match = re.match(r'(\w+)\s+(\w+)(?:\((\d+)(?:,(\d+))?\))?', col_def.strip(), re.IGNORECASE)
            if not col_match:
                continue
            col_name, col_type, size1, size2 = col_match.groups()
            col_type = col_type.upper()

            json_type = type_map.get(col_type, "string")

            field = {
                "name": col_name,
                "type": json_type
            }
            fields.append(field)

        table_meta = {
            "name": table_name.lower(),
            "file": f"{table_name.lower()}.csv",
            "description": table_descriptions.get(table_name.lower(), ""),
            "schema": {
                "fields": fields
            }
        }

        metadata["tables"].append(table_meta)

    return metadata


if __name__ == "__main__":
    metadata = parse_ddl(DDL_FILE)
    with open(OUTPUT_DIR + "tpch_metadata.json", "w") as f:
        json.dump(metadata, f, indent=2)

    print("Generated tpch_metadata.json")

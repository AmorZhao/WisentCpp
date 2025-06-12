import os
import time
import pandas as pd
import pyarrow as pa
import pyarrow.parquet as pq
import humanize

dataset_path = "/root/Documents/WisentCpp/Data/tpch/data/"
csv_subdirs = [
    "data_0.005G",
    "data_0.01G",
    "data_0.05G",
    "data_0.1G",
    "data_0.2G",
    "data_0.5G", 
    "data_0.8G"
]

configs = {
    "A": {"compression": "snappy", "use_dictionary": True, "use_delta": False},
    "B": {"compression": "zstd",   "use_dictionary": True, "use_delta": False},
    "C": {"compression": "gzip",   "use_dictionary": False, "use_delta": False},
    "D": {"compression": "zstd",   "use_dictionary": True, "use_delta": True},
}

def save_parquet(df, out_path, compression, use_dictionary=True, use_delta=False):
    table = pa.Table.from_pandas(df)

    if use_delta and 'join_date' in df.columns:
        join_date = pd.to_datetime(df['join_date'])
        delta = (join_date - join_date.min()).dt.total_seconds().astype('int32')
        table = table.set_column(
            table.schema.get_field_index("join_date"), 
            "join_date", 
            pa.array(delta)
        )

    start = time.time()
    pq.write_table(
        table,
        out_path,
        compression=compression,
        use_dictionary=use_dictionary
    )
    duration = time.time() - start
    size = os.path.getsize(out_path)
    return size, duration

def compress_and_benchmark_all():
    for subdir in csv_subdirs:
        total_sizes = {
            "original": 0,
            "A": 0,
            "B": 0,
            "C": 0,
            "D": 0,
        }
        full_dir = os.path.join(dataset_path, subdir)
        print(f"\nProcessing directory: {subdir}")

        for fname in os.listdir(full_dir):
            if not fname.endswith(".csv"):
                continue

            csv_path = os.path.join(full_dir, fname)
            base_name = os.path.splitext(fname)[0]

            try:
                df = pd.read_csv(csv_path)
                table = pa.Table.from_pandas(df)
                parquet_path = os.path.join(full_dir, f"{base_name}_original.parquet")
                pq.write_table(table, parquet_path, compression=None)
                orig_size = os.path.getsize(parquet_path)
                total_sizes["original"] += orig_size

                for cfg_id, cfg in configs.items():
                    out_name = f"{base_name}_cfg{cfg_id}.parquet"
                    out_path = os.path.join(full_dir, out_name)

                    size, _ = save_parquet(
                        df, out_path,
                        compression=cfg["compression"],
                        use_dictionary=cfg["use_dictionary"],
                        use_delta=cfg["use_delta"]
                    )
                    total_sizes[cfg_id] += size

            except Exception as e:
                print(f"Failed to process {csv_path}: {e}")

        for key, size in total_sizes.items():
            print(f"{key:<10}{size} bytes")


compress_and_benchmark_all()

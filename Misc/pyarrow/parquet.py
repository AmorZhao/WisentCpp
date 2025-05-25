import pandas as pd
import numpy as np
import pyarrow as pa
import pyarrow.parquet as pq
import time
import os
import humanize

def generate_dataset(n_rows=10_000):
    np.random.seed(42)
    df = pd.DataFrame({
        "id": np.arange(n_rows),
        "name": np.random.choice(["Alice", "Bob", "Carol", "Dave", "Eve", "Frank"], size=n_rows),
        "age": np.random.randint(18, 65, size=n_rows),
        "gender": np.random.choice(["M", "F"], size=n_rows),
        "salary": np.round(np.random.normal(60, 10, size=n_rows), 2),
        "join_date": pd.date_range(start='2010-01-01', periods=n_rows, freq='T')
    })
    return df

def save_parquet(df, config_id, compression, use_dictionary=True, use_delta=False):
    table = pa.Table.from_pandas(df)
    
    if use_delta:
        join_date = pd.to_datetime(df['join_date'])
        delta = (join_date - join_date.min()).dt.total_seconds().astype('int32')
        table = table.set_column(table.schema.get_field_index("join_date"), "join_date", pa.array(delta))

    filename = f"test_config_{config_id}.parquet"
    start = time.time()
    
    pq.write_table(
        table,
        filename,
        compression=compression,
        use_dictionary=use_dictionary
    )
    
    duration = time.time() - start
    size = os.path.getsize(filename)
    return filename, size, duration

def benchmark():
    df = generate_dataset()

    print("Generated Dataset Preview:")
    print(df.head())
    print("\nColumn Types:")
    print(df.dtypes)

    configs = {
        "A": {"compression": "snappy", "use_dictionary": True, "use_delta": False},
        "B": {"compression": "zstd",   "use_dictionary": True, "use_delta": False},
        "C": {"compression": "gzip",   "use_dictionary": False, "use_delta": False},
        "D": {"compression": "zstd",   "use_dictionary": True, "use_delta": True},
    }

    original_size = df.memory_usage(deep=True).sum()
    print(f"Original size (in-memory): {humanize.naturalsize(original_size)}")

    results = []

    for cfg_id, cfg in configs.items():
        print(f"\nRunning config {cfg_id}...")
        fname, size, duration = save_parquet(df, cfg_id, **cfg)
        ratio = original_size / size
        results.append((cfg_id, size, duration, ratio))

    print("\nBenchmark Results:")
    print(f"{'Config':<8}{'Size':<12}{'Time (s)':<10}{'Compression Ratio'}")
    for r in results:
        print(f"{r[0]:<8}{humanize.naturalsize(r[1]):<12}{r[2]:<10.2f}{r[3]:.2f}x")

benchmark()

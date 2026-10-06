#!/usr/bin/env python3
"""Exercise actual network service bodies with injected ESP error responses."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "main/coffee_network.c").read_text()
functions = []
for start, end in (
    ("static void close_ap(void)", "static void cleanup_network(void)"),
    ("static esp_err_t connect_station(", "static void saved_connect(void)"),
    ("static void saved_connect(void)", "static esp_err_t open_ap(void)"),
    ("static void time_synced(", "static void start_sntp(void)"),
):
    begin = source.index(start)
    finish = source.index(end, begin)
    functions.append(source[begin:finish])
harness = (Path(__file__).parent / "test_cleanup.c").read_text()
harness = harness.replace("/* COFFEE_NETWORK_FUNCTIONS */", "\n".join(functions))
with tempfile.TemporaryDirectory(prefix="coffee-network-tests-") as tmp:
    test_source = Path(tmp) / "test.c"
    executable = Path(tmp) / "test"
    test_source.write_text(harness)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-I", str(root / "main"), str(test_source), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)

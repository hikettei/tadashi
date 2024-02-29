#!/usr/bin/env python

import subprocess
from pathlib import Path


class App:
    @property
    def include_paths(self) -> list[str]:
        return []

    def compile(self) -> bool:
        subprocess.run(self.compile_cmd)

    @property
    def compile_cmd(self) -> list[str]:
        raise NotImplementedError()

    @property
    def source_path(self) -> Path:
        raise NotImplementedError()

    @property
    def output_binary(self) -> str:
        raise NotImplementedError()

    def measure(self) -> float:
        cmd = [self.output_binary]
        result = subprocess.run(cmd, stdout=subprocess.PIPE)
        stdout = result.stdout.decode()
        runtime = self.extract_runtime(stdout)
        return float(runtime)

    @staticmethod
    def extract_runtime(stdout) -> str:
        raise NotImplementedError()


class Simple(App):
    source: Path

    def __init__(self, source: str):
        self.source = Path(source)

    @property
    def compile_cmd(self) -> list[str]:
        return [
            "gcc",
            self.source_path,
            "-o",
            self.output_binary,
        ]

    @property
    def source_path(self) -> Path:
        return self.source

    @property
    def output_binary(self) -> str:
        return self.source.with_suffix("")

    @staticmethod
    def extract_runtime(stdout):
        return "42.0"


class Polybench(App):
    """A single benchmark in of the Polybench suite."""

    benchmark: Path  # path to the benchmark dir from base
    base: Path  # the dir where polybench was unpacked

    def __init__(self, benchmark: str, base: str):
        self.benchmark = Path(benchmark)
        self.base = Path(base)

    @property
    def source_path(self) -> Path:
        path = self.base / self.benchmark / self.benchmark.name
        return path.with_suffix(".c")

    @property
    def output_binary(self) -> str:
        return self.source_path.parent / self.benchmark.name

    @property
    def utilities_path(self) -> Path:
        return self.base / "utilities"

    @property
    def include_paths(self) -> list[str]:
        return [str(self.utilities_path)]

    def compile_cmd(self) -> list[str]:
        return [
            "gcc",
            self.source_path,
            self.utilities_path / "polybench.c",
            "-I",
            self.include_paths[0],
            "-o",
            self.output_binary,
            "-DPOLYBENCH_TIME",
            "-DPOLYBENCH_USE_RESTRICT",
        ]

    @staticmethod
    def filter(stdout) -> float:
        return stdout.split()[0]

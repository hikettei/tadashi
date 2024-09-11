import json
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def get_df(files):
    means = {}
    for file in files:
        name = file.with_suffix("").name
        means[name] = json.load(open(file))
    df = pd.concat({k: pd.DataFrame(v) for k, v in means.items()})
    df.index.names = ["benchmark", "step"]
    print(df.head(20))
    print(df.loc["gemm-10", :]["Random transformation"].sum())
    print(df.groupby(by=["benchmark"]).mean())
    # https://stackoverflow.com/questions/50976297/reduce-a-panda-dataframe-by-groups
    return df


def main(files):
    means = get_df(list(files)[:3])
    fig, ax = plt.subplots()
    means.plot.bar(stacked=True, ax=ax)
    plt.tight_layout()
    plt.savefig("plot.pdf")


if __name__ == "__main__":
    main(files=Path("./times/").glob("*-10.json"))

#!/usr/bin/env python3
"""Stage offline documents beside an existing proxy; preserve signed app bytes."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'amiga'))
from release_docs import write_docs
from release_policy import check_tree


def prepare(bundle, output):
    bundle, output = Path(bundle).resolve(), Path(output).resolve()
    if output.exists():
        raise ValueError('Choose a new staging directory')
    if bundle.suffix == '.app':
        output.mkdir(parents=True)
        subprocess.run(['ditto', str(bundle), str(output/bundle.name)], check=True)
    else:
        shutil.copytree(bundle, output)
    write_docs(output, 'proxy')
    check_tree(output)
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(prepare(args.bundle, args.output))

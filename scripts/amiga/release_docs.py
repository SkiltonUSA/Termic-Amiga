"""Generate readable offline release documents from the project's main README."""
import argparse
from pathlib import Path
import re
import textwrap
import unicodedata
from urllib.parse import urljoin

ROOT = Path(__file__).resolve().parents[2]
DOCUMENTS = ('README.txt', 'ABOUT-LICENSE.txt')
COMPONENTS = {
    'native': 'Termic Amiga native MUI development workspace',
    'monitor': 'Termic standalone Process Monitor',
    'proxy': 'Termic AI Proxy for macOS and Windows',
    'bridge': 'Amiga-MCP multi-client gateway',
    'release': 'Termic Amiga release and separate host applications',
}
QUICK_START = {
    'native': 'Open the Termic-Amiga drawer and double-click Termic Amiga.\nKeep TermicNative beside the launcher. Back up existing installations first.\nMUI 5 must already be installed. See the installation section below.',
    'monitor': 'Open Program from Workbench. Read ReadMe for process-control limitations.\nThe same monitor is integrated into the main Termic application.',
    'proxy': 'Mac: open Termic AI Proxy.app. Windows: extract the complete folder and\nopen Termic AI Proxy.exe, keeping _internal beside it. Configure the gateway\non port 55110 and your separately installed Codex, Claude or Cursor CLI.',
    'bridge': 'Open Start-Amiga-MCP.command. An existing connected DevBench backend\non port 55010 and its Amiga-side bridge are required. Clients use 55110.\nStarts paused; follow Gateway-Setup.txt before enabling. Reuse an existing\ngateway rather than starting a second instance against the same backend.',
    'release': 'Choose the native Amiga LHA, the Proxy for your host platform, and the\nseparate multi-client gateway as needed. Installation and download details\nare reproduced below. Verify downloads using SHA256SUMS.',
}


def plain_readme(markdown, base_url):
    """Keep prose/code/links, remove image dependencies and Markdown decoration."""
    lines = []
    fenced = False
    for line in markdown.splitlines():
        if line.lstrip().startswith('```'):
            fenced = not fenced
            continue
        if fenced:
            lines.append(line)
            continue
        line = re.sub(r'!\[[^\]]*\]\([^\n)]+\)', '', line)
        line = re.sub(r'\[([^\]]+)\]\(([^\s)]+)\)',
                      lambda m: m[1]+' ('+urljoin(base_url, m[2])+')', line)
        line = re.sub(r'^#{1,6}\s+', '', line)
        if re.fullmatch(r'\s*\|?(?:\s*:?-+:?\s*\|)+\s*', line):
            continue
        if line.startswith('|'):
            line = ' / '.join(c.strip() for c in line.strip('|').split('|'))
        line = line.replace('**', '').replace('`', '')
        line = line.replace('\u2014', ' - ').replace('\u2013', '-').replace('\u2019', "'").replace('\u2192', '->')
        line = unicodedata.normalize('NFKD', line).encode('ascii', 'replace').decode('ascii')
        lines.append(textwrap.fill(line, width=88, subsequent_indent='  ' if line.startswith('- ') else '',
                                   break_long_words=False, break_on_hyphens=False) if line else '')
    return re.sub(r'\n{3,}', '\n\n', '\n'.join(lines)).strip()+'\n'


def write_docs(destination, component='release', ref='main', upstream_license=None, root=ROOT):
    if component not in COMPONENTS:
        raise ValueError('Unknown release component: '+component)
    if component == 'bridge' and upstream_license is None:
        raise ValueError('The gateway must retain its actual upstream licence')
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    base = 'https://github.com/SkiltonUSA/Termic-Amiga/blob/'+ref+'/'
    readme = COMPONENTS[component]+'\n'+'='*len(COMPONENTS[component])+'\n\n'
    readme += QUICK_START[component]+'\n\n'
    readme += 'Offline edition of the main GitHub README: '+base+'README.md\n'
    readme += 'Screenshots are omitted. No PNG files are installed.\n\n'
    readme += plain_readme((Path(root)/'README.md').read_text(), base)
    (destination/'README.txt').write_text(readme, encoding='ascii', newline='\n')
    about = '''TERMIC AMIGA - ABOUT, CREDITS AND LICENCES

Component: '''+COMPONENTS[component]+'''
Project and corresponding source: https://github.com/SkiltonUSA/Termic-Amiga
Release source: '''+base+'''

Amiga adaptation: Dominic Skilton, with Codex assistance.
Original Termic: Simion Agavriloaei and contributors.
Termic application code is GNU Affero General Public License, version 3
or later (AGPL-3.0-or-later). The complete licence text follows below.

Components retain their own licences; the Termic licence does not replace
third-party terms. Native SDK archives preserve their upstream notices.
Tiny includes LICENSE and DATA_LICENSES.md under Tools/Tiny. cJSON carries
its notice in Source/vendor. Python, Qt/PySide, shiboken and MCP dependencies
retain their bundled notices in the host application's dependency folders.
MUI, TextEditor.mcc, VBCC, vasm, vlink, NDK, AmiSSL and native Git remain
subject to their respective distributions' terms. Cloud vendor CLIs and
accounts are separate prerequisites and are not included in this download.

The separate Amiga-MCP multi-client gateway is MIT licensed, independently
of Termic. Its LICENSE, source and provenance.json identify its authors,
upstream revision and dependencies. Its version is not the Termic version.

No warranty is provided beyond the terms in the applicable licences.

'''
    if component == 'bridge':
        about += 'AMIGA-MCP GATEWAY - COMPLETE UPSTREAM LICENCE\n\n'+Path(upstream_license).read_text()+'\n\n'
    about += 'TERMIC - COMPLETE GNU AGPL VERSION 3\n\n'+(Path(root)/'LICENSE').read_text()
    (destination/'ABOUT-LICENSE.txt').write_text(about, encoding='utf-8', newline='\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--component', choices=COMPONENTS, default='release')
    parser.add_argument('--ref', default='main')
    parser.add_argument('--upstream-license', type=Path)
    args = parser.parse_args()
    write_docs(args.output, args.component, args.ref, args.upstream_license)

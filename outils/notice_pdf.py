"""Turns the notice of one phase into a PDF, diagrams included.

Usage: python outils/notice_pdf.py docs/notice/phase-N output.pdf
"""

import pathlib
import sys

import pypandoc
from weasyprint import HTML

STYLE = """
@page { size: A4; margin: 18mm 16mm; }
body { font-family: sans-serif; font-size: 10.5pt; line-height: 1.45; }
h1 { font-size: 18pt; } h2 { font-size: 14pt; margin-top: 1.4em; }
img { max-width: 100%; }
code, pre { font-size: 9pt; }
pre { background: #f3f3f3; padding: 6px 8px; white-space: pre-wrap; }
table { border-collapse: collapse; } td, th { border: 1px solid #999; padding: 3px 6px; }
tr { break-inside: avoid; }  /* A row never splits across pages. */
td img { max-height: 80px; width: auto; }  /* Part illustrations stay thumbnails. */
"""


def main(phase_dir: str, output: str) -> None:
    folder = pathlib.Path(phase_dir).resolve()
    html = pypandoc.convert_file(
        str(folder / "README.md"),
        "html5",
        format="gfm",
        extra_args=["--standalone", f"--metadata=pagetitle:{folder.name}"],
    )
    HTML(string=html, base_url=str(folder)).write_pdf(output, stylesheets=[_css()])
    print(f"notice : {output}")


def _css():
    from weasyprint import CSS

    return CSS(string=STYLE)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])

#!/usr/bin/env python3
"""ECHO Portal Rebrand — transforms captive portal HTML/CSS from TRMNL to ECHO identity.

The captive portal HTML lives as gzipped byte arrays in WifiCaptivePage.h.
This script decompresses them, applies ECHO branding replacements, recompresses,
and generates either a report or a patched overlay file.

Usage:
    python3 echo/portal_rebrand.py [--dry-run] [--apply] [--report]

Modes:
    --dry-run   Show what would change without modifying files (default)
    --report    Generate echo/portal-changes.json with all findings
    --apply     Write patched WifiCaptivePage.h to echo/WifiCaptivePage.h.patch
"""
from __future__ import annotations

import argparse
import gzip
import json
import re
import sys
import textwrap
from dataclasses import dataclass, field, asdict
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
ECHO_DIR = Path(__file__).resolve().parent
WIFICAPTIVE_SRC = REPO_ROOT / "lib" / "wificaptive" / "src"
PAGE_HEADER = WIFICAPTIVE_SRC / "WifiCaptivePage.h"

# ── ECHO Design Tokens ─────────────────────────────────────────────────
ECHO_VIOLET = "#8b5cf6"
ECHO_DARK_BG = "#05050a"
ECHO_LIGHT_TEXT = "#eaeaf2"
ECHO_PAPER_LIGHT = "#f5f2ed"
TRMNL_ORANGE = "#F86527"
TRMNL_ORANGE_LOWER = "#f86527"

# ECHO wordmark SVG — monospace "ECHO" with Shadow Lab subtitle, inline
ECHO_LOGO_SVG = textwrap.dedent("""\
<svg width="160" height="48" viewBox="0 0 160 48" fill="none" xmlns="http://www.w3.org/2000/svg">
  <text x="80" y="28" text-anchor="middle" font-family="'Courier New', Courier, monospace"
        font-size="32" font-weight="700" fill="{violet}" letter-spacing="4">ECHO</text>
  <text x="80" y="44" text-anchor="middle" font-family="'Courier New', Courier, monospace"
        font-size="11" fill="{light_text}" letter-spacing="2">SHADOW LAB</text>
</svg>""").format(violet=ECHO_VIOLET, light_text=ECHO_LIGHT_TEXT)


# ── Replacement Rules ───────────────────────────────────────────────────
@dataclass
class Rule:
    pattern: str
    replacement: str
    description: str
    category: str  # "css" | "html" | "text" | "svg"
    regex: bool = False


RULES: list[Rule] = [
    # ── Title tags ──
    Rule(
        "<title>TRMNL Wi-Fi Configuration</title>",
        "<title>ECHO Wi-Fi Setup</title>",
        "Main portal page title",
        "html",
    ),
    Rule(
        "<title>TRMNL Advanced Configuration</title>",
        "<title>ECHO Advanced Setup</title>",
        "Advanced config page title",
        "html",
    ),

    # ── CSS: TRMNL orange (#F86527) → ECHO violet ──
    Rule(
        "border: 2px solid #F86527",
        f"border: 2px solid {ECHO_VIOLET}",
        "Input focus border color",
        "css",
    ),
    Rule(
        "background-color: #F86527",
        f"background-color: {ECHO_VIOLET}",
        "Success button background",
        "css",
    ),
    Rule(
        "border-top: 6px solid #F86527",
        f"border-top: 6px solid {ECHO_VIOLET}",
        "Loader spinner accent color",
        "css",
    ),

    # ── Logo: replace <img src="/logo.svg"> with inline SVG ──
    Rule(
        r'<img src="/logo\.svg"[^/]*/?>',
        f'<div style="text-align:center;margin:20px 0;">{ECHO_LOGO_SVG}</div>',
        "Replace TRMNL logo image with inline ECHO wordmark SVG",
        "svg",
        regex=True,
    ),

    # ── Text: TRMNL device references ──
    Rule(
        "Soft resetting your TRMNL device",
        "Soft resetting your ECHO device",
        "Soft reset confirmation dialog text",
        "text",
    ),
    Rule(
        "will use the official server with their TRMNL",
        "will use the official server with their ECHO device",
        "Custom server warning dialog text",
        "text",
    ),

    # ── URL: trmnl.app placeholder ──
    Rule(
        "https://trmnl.app",
        "https://shadowlab.cc/echo",
        "Custom server placeholder URL",
        "text",
    ),

    # ── Body/background theming (dark mode) ──
    # Apply ECHO dark theme to body
    Rule(
        "font: 400 14px 'Calibri', 'Arial';",
        f"font: 400 14px 'Calibri', 'Arial'; background-color: {ECHO_DARK_BG}; color: {ECHO_LIGHT_TEXT};",
        "Apply ECHO dark background and light text to body",
        "css",
    ),

    # Table backgrounds: white → dark
    Rule(
        "background: white;",
        f"background: {ECHO_DARK_BG};",
        "Table background from white to dark",
        "css",
    ),

    # Table row hover from light gray to subtle violet
    Rule(
        "background: #f2f2f2;",
        "background: rgba(139, 92, 246, 0.15);",
        "Table row hover highlight",
        "css",
    ),

    # Selected row
    Rule(
        "background-color: #BBBB;",
        "background-color: rgba(139, 92, 246, 0.3);",
        "Selected row background",
        "css",
    ),

    # Table border
    Rule(
        "border-bottom: 1px solid #BBBB;",
        f"border-bottom: 1px solid rgba(234, 234, 242, 0.15);",
        "Table row border color",
        "css",
    ),

    # Modal content background
    Rule(
        "background-color: #fefefe;",
        f"background-color: #1a1a2e;",
        "Modal content background",
        "css",
    ),

    # Alert primary styling for dark mode (handles both \r\n and \n)
    Rule(
        r"color: #383d41;\s+background-color: #e2e3e5;\s+border-color: #d6d8db;",
        f"color: {ECHO_LIGHT_TEXT};\n      background-color: rgba(139, 92, 246, 0.15);\n      border-color: rgba(139, 92, 246, 0.3);",
        "Alert primary colors for dark theme",
        "css",
        regex=True,
    ),

    # Blockquote text already white — keep it

    # Button primary (light gray → dark surface)
    Rule(
        r"background-color: #e4eaf0;\s+color: #212529;",
        f"background-color: #2a2a3e;\n      color: {ECHO_LIGHT_TEXT};",
        "Primary button dark theme",
        "css",
        regex=True,
    ),
    Rule(
        "background-color: #d7dfe8;",
        "background-color: #3a3a52;",
        "Primary button hover dark theme",
        "css",
    ),

    # Loader border base from light → dark surface
    Rule(
        "border: 6px solid #f3f3f3;",
        "border: 6px solid #2a2a3e;",
        "Loader spinner base ring dark theme",
        "css",
    ),

    # Caption text
    Rule(
        "color: #6c757d;",
        f"color: rgba(234, 234, 242, 0.6);",
        "Caption text color for dark theme",
        "css",
    ),

    # Close button color
    Rule(
        "color: #aaa;",
        f"color: rgba(234, 234, 242, 0.5);",
        "Modal close button color",
        "css",
    ),
    Rule(
        r"color: black;\s+text-decoration: none;",
        f"color: {ECHO_LIGHT_TEXT};\n      text-decoration: none;",
        "Modal close button hover color",
        "css",
        regex=True,
    ),
]


# ── Data Structures ─────────────────────────────────────────────────────
@dataclass
class ArrayInfo:
    name: str
    original_gz_bytes: int
    original_html_bytes: int
    rebranded_html_bytes: int
    rebranded_gz_bytes: int
    replacements: list[dict] = field(default_factory=list)


@dataclass
class PortalReport:
    source_file: str
    arrays_found: int = 0
    total_replacements: int = 0
    arrays: list[ArrayInfo] = field(default_factory=list)


# ── Core Logic ──────────────────────────────────────────────────────────
def extract_arrays(header_content: str) -> list[tuple[str, bytes]]:
    """Extract named gzipped byte arrays from WifiCaptivePage.h."""
    pattern = r'const uint8_t (\w+)\[\] PROGMEM = \{([^}]+)\}'
    matches = re.findall(pattern, header_content)
    result = []
    for name, hex_data in matches:
        hex_bytes = re.findall(r'0x[0-9a-fA-F]+', hex_data)
        raw = bytes([int(h, 16) for h in hex_bytes])
        result.append((name, raw))
    return result


def apply_rules_to_html(html: str, rules: list[Rule]) -> tuple[str, list[dict]]:
    """Apply all rebrand rules to decompressed HTML. Returns (new_html, replacements)."""
    replacements = []
    for rule in rules:
        if rule.regex:
            matches = list(re.finditer(rule.pattern, html))
            if matches:
                html = re.sub(rule.pattern, rule.replacement, html)
                for m in matches:
                    replacements.append({
                        "description": rule.description,
                        "category": rule.category,
                        "matched": m.group()[:100],
                        "replacement_preview": rule.replacement[:100],
                    })
        else:
            count = html.count(rule.pattern)
            if count > 0:
                html = html.replace(rule.pattern, rule.replacement)
                replacements.append({
                    "description": rule.description,
                    "category": rule.category,
                    "occurrences": count,
                    "matched": rule.pattern[:100],
                    "replacement_preview": rule.replacement[:100],
                })
    return html, replacements


def bytes_to_c_array(data: bytes, name: str) -> str:
    """Convert bytes back to a C PROGMEM array declaration."""
    hex_vals = ", ".join(f"0x{b:x}" for b in data)
    return f"const uint8_t {name}[] PROGMEM = {{ {hex_vals} }};"


def generate_patched_header(original_content: str, patched_arrays: dict[str, bytes]) -> str:
    """Replace byte arrays in the header with patched versions, update lengths."""
    result = original_content
    for name, new_gz in patched_arrays.items():
        # Replace the array content
        old_pattern = re.compile(
            rf'(const uint8_t {re.escape(name)}\[\] PROGMEM = \{{)[^}}]+(}}\s*;)'
        )
        hex_vals = ", ".join(f"0x{b:x}" for b in new_gz)
        result = old_pattern.sub(rf'\1 {hex_vals} \2', result)

        # Update the corresponding _LEN constant
        len_pattern = re.compile(
            rf'(const unsigned int {re.escape(name)}_LEN = )\d+(;)'
        )
        result = len_pattern.sub(rf'\g<1>{len(new_gz)}\2', result)

    return result


def main():
    parser = argparse.ArgumentParser(description="ECHO Portal Rebrand Engine")
    parser.add_argument("--dry-run", action="store_true", default=True,
                        help="Show findings without modifying files (default)")
    parser.add_argument("--apply", action="store_true",
                        help="Write patched WifiCaptivePage.h to echo/ overlay")
    parser.add_argument("--report", action="store_true",
                        help="Write echo/portal-changes.json")
    args = parser.parse_args()

    if args.apply:
        args.dry_run = False

    if not PAGE_HEADER.exists():
        print(f"ERROR: {PAGE_HEADER} not found")
        return 1

    print("ECHO Portal Rebrand Engine")
    print(f"Source: {PAGE_HEADER.relative_to(REPO_ROOT)}")
    print(f"Rules: {len(RULES)}")
    print()

    header_content = PAGE_HEADER.read_text(encoding="utf-8", errors="replace")
    arrays = extract_arrays(header_content)

    report = PortalReport(
        source_file=str(PAGE_HEADER.relative_to(REPO_ROOT)),
        arrays_found=len(arrays),
    )

    patched_arrays: dict[str, bytes] = {}

    for name, gz_data in arrays:
        try:
            html = gzip.decompress(gz_data).decode("utf-8")
        except Exception as e:
            print(f"  SKIP {name}: decompress failed ({e})")
            continue

        new_html, replacements = apply_rules_to_html(html, RULES)
        new_gz = gzip.compress(new_html.encode("utf-8"), compresslevel=9)

        info = ArrayInfo(
            name=name,
            original_gz_bytes=len(gz_data),
            original_html_bytes=len(html),
            rebranded_html_bytes=len(new_html),
            rebranded_gz_bytes=len(new_gz),
            replacements=replacements,
        )
        report.arrays.append(info)
        report.total_replacements += len(replacements)

        if replacements:
            patched_arrays[name] = new_gz

        # Display
        delta_html = len(new_html) - len(html)
        delta_gz = len(new_gz) - len(gz_data)
        status = f"{len(replacements)} replacements" if replacements else "no changes"
        print(f"  {name}: {status}")
        print(f"    HTML: {len(html)} -> {len(new_html)} ({delta_html:+d} bytes)")
        print(f"    GZIP: {len(gz_data)} -> {len(new_gz)} ({delta_gz:+d} bytes)")

        if replacements:
            for r in replacements:
                print(f"    - [{r['category']}] {r['description']}")

        print()

    # Also handle the LOGO_SVG array — replace entirely with ECHO wordmark
    for name, gz_data in arrays:
        if name == "LOGO_SVG":
            new_svg = ECHO_LOGO_SVG
            new_gz = gzip.compress(new_svg.encode("utf-8"), compresslevel=9)
            patched_arrays[name] = new_gz
            print(f"  {name}: replaced with ECHO wordmark SVG")
            print(f"    Original: {len(gz_data)} bytes (gz) -> New: {len(new_gz)} bytes (gz)")
            print()

            # Update report
            for info in report.arrays:
                if info.name == name:
                    info.rebranded_html_bytes = len(new_svg)
                    info.rebranded_gz_bytes = len(new_gz)
                    info.replacements.append({
                        "description": "Full SVG replacement with ECHO wordmark",
                        "category": "svg",
                        "matched": "entire TRMNL logo SVG",
                        "replacement_preview": "ECHO monospace wordmark + SHADOW LAB subtitle",
                    })
                    report.total_replacements += 1

    # Summary
    print(f"Total: {report.total_replacements} replacements across {len(patched_arrays)} arrays")

    # Write report
    if args.report or True:  # always write report
        report_path = ECHO_DIR / "portal-changes.json"
        report_data = {
            "source_file": report.source_file,
            "arrays_found": report.arrays_found,
            "total_replacements": report.total_replacements,
            "echo_design_tokens": {
                "violet": ECHO_VIOLET,
                "dark_bg": ECHO_DARK_BG,
                "light_text": ECHO_LIGHT_TEXT,
                "paper_light": ECHO_PAPER_LIGHT,
            },
            "arrays": [asdict(info) for info in report.arrays],
        }
        report_path.write_text(json.dumps(report_data, indent=2))
        print(f"\nReport: {report_path.relative_to(REPO_ROOT)}")

    # Write patched overlay
    if not args.dry_run and patched_arrays:
        overlay_path = ECHO_DIR / "WifiCaptivePage.h.patch"
        patched_content = generate_patched_header(header_content, patched_arrays)
        overlay_path.write_text(patched_content, encoding="utf-8")
        print(f"Overlay: {overlay_path.relative_to(REPO_ROOT)}")

        # Also write decompressed HTML files for review
        for name, gz_data in patched_arrays.items():
            try:
                html = gzip.decompress(gz_data).decode("utf-8")
                review_path = ECHO_DIR / f"portal_{name}_rebranded.html"
                review_path.write_text(html)
                print(f"Review: {review_path.relative_to(REPO_ROOT)}")
            except Exception:
                pass

        print(f"\nTo apply: copy echo/WifiCaptivePage.h.patch over lib/wificaptive/src/WifiCaptivePage.h")
    elif args.dry_run:
        print("\nDry run — no files modified. Use --apply to generate overlay.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
